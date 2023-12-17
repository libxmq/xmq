/* libxmq - Copyright (C) 2023 Fredrik Öhrström (spdx: MIT)

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

#include<assert.h>
#include<ctype.h>
#include<memory.h>
#include<string.h>
#include<stdio.h>
#include<unistd.h>

#ifdef PLATFORM_WINAPI
#include<windows.h>
#else
#include<termios.h>
#endif

#define LIBXML_STATIC
#define LIBXSLT_STATIC
#define XMLSEC_STATIC

#include<libxml/tree.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>
#include <libxslt/xslt.h>
#include <libxslt/xsltInternals.h>
#include <libxslt/transform.h>
#include <libxslt/xsltutils.h>

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
    XMQ_CLI_CMD_TO_XMQ,
    XMQ_CLI_CMD_TO_XML,
    XMQ_CLI_CMD_TO_HTMQ,
    XMQ_CLI_CMD_TO_HTML,
    XMQ_CLI_CMD_TO_JSON,
    XMQ_CLI_CMD_RENDER_TERMINAL,
    XMQ_CLI_CMD_RENDER_HTML,
    XMQ_CLI_CMD_RENDER_TEX,
    XMQ_CLI_CMD_TOKENIZE,
    XMQ_CLI_CMD_DELETE,
    XMQ_CLI_CMD_DELETE_ENTITY,
    XMQ_CLI_CMD_REPLACE,
    XMQ_CLI_CMD_REPLACE_ENTITY,
    XMQ_CLI_CMD_TRANSFORM,
} XMQCliCmd;

typedef enum {
    XMQ_CLI_CMD_GROUP_NONE,
    XMQ_CLI_CMD_GROUP_TO,
    XMQ_CLI_CMD_GROUP_RENDER,
    XMQ_CLI_CMD_GROUP_TOKENIZE,
    XMQ_CLI_CMD_GROUP_XPATH,
    XMQ_CLI_CMD_GROUP_ENTITY,
    XMQ_CLI_CMD_GROUP_TRANSFORM,
} XMQCliCmdGroup;

typedef enum {
    XMQ_RENDER_MONO = 0,
    XMQ_RENDER_COLOR_DARKBG = 1,
    XMQ_RENDER_COLOR_LIGHTBG= 2
} XMQRenderStyle;

typedef struct XMQCliEnvironment XMQCliEnvironment;
struct XMQCliEnvironment
{
    XMQDoc *doc;
    bool use_detect;
    bool use_color;
    bool dark_mode;
};

typedef struct XMQCliCommand XMQCliCommand;

struct XMQCliCommand
{
    XMQCliEnvironment *env;
    XMQCliCmd cmd;
    const char *in;
    const char *out;
    const char *xpath;
    const char *entity;
    const char *content; // Content to replace something.
    XMQDoc *xslt_doq; // The xmq document loaded to generate the xslt.
    xsltStylesheetPtr xslt; // The xslt trasnform to be used.
    xmlDocPtr   node_doc;
    xmlNodePtr  node_content; // Tree content to replace something.
    XMQContentType in_format;
    XMQContentType out_format;
    XMQRenderFormat render_to;
    bool render_raw;
    bool only_style;
    XMQTrimType trim;
    bool use_color; // Uses color or not for terminal/html/tex
    bool dark_mode; // If false assume background is light.
    bool print_help;
    bool print_version;
    bool debug;
    bool verbose;
    int  add_indent;
    bool compact;
    bool escape_newlines;
    bool escape_non_7bit;
    int  tab_size; // Default 8
    const char *implicit_root;

    // Command tok
    XMQCliTokenizeType tok_type; // Do not pretty print, just debug/colorize tokens.
    XMQCliCommand *next; // Point to next command to be executed.
};


// FUNCTION DECLARATIONS /////////////////////////////////////////////////////////////

XMQCliCommand *allocate_cli_command(XMQCliEnvironment *env);
bool cmd_delete(XMQCliCommand *command);
bool cmd_replace(XMQCliCommand *command);
XMQCliCmd cmd_from(const char *s);
XMQCliCmdGroup cmd_group(XMQCliCmd cmd);
int cmd_load(XMQCliCommand *command);
const char *cmd_name(XMQCliCmd cmd);
bool cmd_to(XMQCliCommand *command);
bool cmd_transform(XMQCliCommand *command);
void cmd_unload(XMQCliCommand *command);
const char *content_type_to_string(XMQContentType ct);
void debug_(const char* fmt, ...);
void disable_raw_mode();
void enable_raw_mode();
void enableAnsiColorsTerminal();
bool has_debug(int argc, const char **argv);
bool has_verbose(int argc, const char **argv);
bool handle_global_option(const char *arg, XMQCliCommand *command);
bool handle_option(const char *arg, XMQCliCommand *command);
bool perform_command(XMQCliCommand *c);
void prepare_command(XMQCliCommand *c);
void print_help_and_exit();
void print_version_and_exit();
const char *render_format_to_string(XMQRenderFormat rt);
void replace_entity(xmlDoc *doc, xmlNodePtr node, const char *entity, const char *content, xmlNodePtr node_content);
void replace_entities(xmlDoc *doc, const char *entity, const char *content, xmlNodePtr node_content);
XMQRenderStyle render_style(bool *use_color, bool *dark_mode);
int tokenize_input(XMQCliCommand *command);
void verbose_(const char* fmt, ...);
void write_print(void *buffer, const char *content);
bool xmq_parse_cmd_line(int argc, const char **argv, XMQCliCommand *command);

/////////////////////////////////////////////////////////////////////////////////////

const char *error_to_print_on_exit = NULL;

bool verbose_enabled__ = false;

void verbose_(const char* fmt, ...)
{
    if (verbose_enabled__) {
        va_list args;
        va_start(args, fmt);
        vfprintf(stderr, fmt, args);
        va_end(args);
    }
}

bool debug_enabled__ = false;

void debug_(const char* fmt, ...)
{
    if (debug_enabled__) {
        va_list args;
        va_start(args, fmt);
        vfprintf(stderr, fmt, args);
        va_end(args);
    }
}


XMQCliCmd cmd_from(const char *s)
{
    if (!s) return XMQ_CLI_CMD_NONE;
    if (!strcmp(s, "to_xmq")) return XMQ_CLI_CMD_TO_XMQ;
    if (!strcmp(s, "to_xml")) return XMQ_CLI_CMD_TO_XML;
    if (!strcmp(s, "to_htmq")) return XMQ_CLI_CMD_TO_HTMQ;
    if (!strcmp(s, "to_html")) return XMQ_CLI_CMD_TO_HTML;
    if (!strcmp(s, "to_json")) return XMQ_CLI_CMD_TO_JSON;
    if (!strcmp(s, "render_terminal")) return XMQ_CLI_CMD_RENDER_TERMINAL;
    if (!strcmp(s, "render_html")) return XMQ_CLI_CMD_RENDER_HTML;
    if (!strcmp(s, "render_tex")) return XMQ_CLI_CMD_RENDER_TEX;
    if (!strcmp(s, "tokenize")) return XMQ_CLI_CMD_TOKENIZE;
    if (!strcmp(s, "delete")) return XMQ_CLI_CMD_DELETE;
    if (!strcmp(s, "delete_entity")) return XMQ_CLI_CMD_DELETE_ENTITY;
    if (!strcmp(s, "replace")) return XMQ_CLI_CMD_REPLACE;
    if (!strcmp(s, "replace_entity")) return XMQ_CLI_CMD_REPLACE_ENTITY;
    if (!strcmp(s, "transform")) return XMQ_CLI_CMD_TRANSFORM;
    return XMQ_CLI_CMD_NONE;
}

const char *cmd_name(XMQCliCmd cmd)
{
    switch (cmd)
    {
    case XMQ_CLI_CMD_NONE: return "noop";
    case XMQ_CLI_CMD_TO_XMQ: return "to_xmq";
    case XMQ_CLI_CMD_TO_XML: return "to_xml";
    case XMQ_CLI_CMD_TO_HTMQ: return "to_htmq";
    case XMQ_CLI_CMD_TO_HTML: return "to_html";
    case XMQ_CLI_CMD_TO_JSON: return "to_json";
    case XMQ_CLI_CMD_RENDER_TERMINAL: return "render_terminal";
    case XMQ_CLI_CMD_RENDER_HTML: return "render_html";
    case XMQ_CLI_CMD_RENDER_TEX: return "render_tex";
    case XMQ_CLI_CMD_TOKENIZE: return "tokenize";
    case XMQ_CLI_CMD_DELETE: return "delete";
    case XMQ_CLI_CMD_DELETE_ENTITY: return "delete_entity";
    case XMQ_CLI_CMD_REPLACE: return "replace";
    case XMQ_CLI_CMD_REPLACE_ENTITY: return "replace_entity";
    case XMQ_CLI_CMD_TRANSFORM: return "transform";
    }
    return "?";
}

XMQCliCmdGroup cmd_group(XMQCliCmd cmd)
{
    switch (cmd)
    {
    case XMQ_CLI_CMD_TO_XMQ:
    case XMQ_CLI_CMD_TO_XML:
    case XMQ_CLI_CMD_TO_HTMQ:
    case XMQ_CLI_CMD_TO_HTML:
    case XMQ_CLI_CMD_TO_JSON:
        return XMQ_CLI_CMD_GROUP_TO;

    case XMQ_CLI_CMD_RENDER_TERMINAL:
    case XMQ_CLI_CMD_RENDER_HTML:
    case XMQ_CLI_CMD_RENDER_TEX:
        return XMQ_CLI_CMD_GROUP_RENDER;
    case XMQ_CLI_CMD_TOKENIZE:
        return XMQ_CLI_CMD_GROUP_TOKENIZE;
    case XMQ_CLI_CMD_DELETE:
    case XMQ_CLI_CMD_REPLACE:
        return XMQ_CLI_CMD_GROUP_XPATH;
    case XMQ_CLI_CMD_DELETE_ENTITY:
    case XMQ_CLI_CMD_REPLACE_ENTITY:
        return XMQ_CLI_CMD_GROUP_ENTITY;
    case XMQ_CLI_CMD_TRANSFORM:
        return XMQ_CLI_CMD_GROUP_TRANSFORM;
    case XMQ_CLI_CMD_NONE:
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
    }
    assert(false);
    return "?";
}

XMQCliCommand *allocate_cli_command(XMQCliEnvironment *env)
{
    XMQCliCommand *c = malloc(sizeof(XMQCliCommand));
    memset(c, 0, sizeof(*c));

    c->use_color = env->use_color;
    c->dark_mode = env->dark_mode;
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
    c->tab_size = 8;

    return c;
}

bool handle_option(const char *arg, XMQCliCommand *command)
{
    if (!arg) return false;

    XMQCliCmdGroup group = cmd_group(command->cmd);

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
    if (command->cmd == XMQ_CLI_CMD_TO_XMQ ||
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
        if (!strcmp(arg, "--color"))
        {
            command->env->use_detect = false;
            command->use_color = true;
            if (command->render_to == XMQ_RENDER_PLAIN)
            {
                command->render_to = XMQ_RENDER_TERMINAL;
            }
            return true;
        }
        if (!strcmp(arg, "--mono"))
        {
            command->env->use_detect = false;
            command->use_color = false;
            return true;
        }
        if (!strcmp(arg, "--lightbg"))
        {
            command->env->use_detect = false;
            command->dark_mode = false;
            return true;
        }
        if (!strcmp(arg, "--darkbg"))
        {
            command->env->use_detect = false;
            command->dark_mode = true;
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
        if (group == XMQ_CLI_CMD_GROUP_XPATH && command->content != NULL )
        {
            command->content = arg;
            return true;
        }
    }

    if (group == XMQ_CLI_CMD_GROUP_ENTITY)
    {
        if (!strncmp(arg, "--with-xml-file=", 12))
        {
            const char *file = arg+12;
            XMQCliEnvironment env;
            memset(&env, 0, sizeof(env));
            XMQCliCommand cmd;
            memset(&cmd, 0, sizeof(XMQCliCommand));
            cmd.env = &env;
            cmd.in = file;
            cmd.in_format = XMQ_CONTENT_DETECT;
            cmd.trim = XMQ_TRIM_NONE;
            size_t len = strlen(file);
            // .xml
            if (len > 4 && !(strncmp(file+len-4, ".xml", 4)))
            {
                cmd.in_format = XMQ_CONTENT_XML;
            }
            if (len > 5 && !(strncmp(file+len-5, ".html", 5)))
            {
                cmd.in_format = XMQ_CONTENT_HTML;
            }
            cmd_load(&cmd);
            xmlDocPtr doc = (xmlDocPtr)xmqGetImplementationDoc(cmd.env->doc);
            command->node_doc = doc;
            command->node_content = doc->children;
            return true;
        }

        if (!strncmp(arg, "--with-text-file=", 17))
        {
            const char *file = arg+17;
            verbose_("(xmq) reading text file %s\n", file);
            FILE *f = fopen(file, "rb");
            fseek(f, 0L, SEEK_END);
            size_t sz = ftell(f);
            fseek(f, 0L, SEEK_SET);
            char *buf = malloc(sz+1);
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
        if (command->xslt == NULL)
        {
            XMQDoc *doq = xmqNewDoc();

            bool ok = xmqParseFileWithType(doq, arg, NULL, XMQ_CONTENT_DETECT, XMQ_TRIM_DEFAULT);

            if (!ok)
            {
                int rc = xmqDocErrno(command->env->doc);
                const char *error = xmqDocError(command->env->doc);
                fprintf(stderr, error, command->in);
                xmqFreeDoc(command->env->doc);
                command->env->doc = NULL;
                return rc;
            }

            verbose_("(xmq) loaded xslt %s\n", arg);

            xmlDocPtr xslt = (xmlDocPtr)xmqGetImplementationDoc(doq);

            command->xslt_doq = doq;
            command->xslt = xsltParseStylesheetDoc(xslt);
            //command->xslt = xsltParseStylesheetFile((const xmlChar *)arg);
            return true;
        }
    }

    return false;
}

#ifndef PLATFORM_WINAPI
struct termios orig_termios;
#endif

void disable_raw_mode()
{
#ifndef PLATFORM_WINAPI
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
#endif
}

void enable_raw_mode()
{
#ifndef PLATFORM_WINAPI
    tcgetattr(STDIN_FILENO, &orig_termios);
    atexit(disable_raw_mode);

    struct termios raw;
    tcgetattr(STDIN_FILENO, &raw);
    raw.c_lflag &= ~ECHO;
    raw.c_lflag &= ~ICANON;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
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


XMQRenderStyle render_style(bool *use_color, bool *dark_mode)
{
    // The Linux vt console is by default black. So dark-mode.
    char *term = getenv("TERM");
    if (!strcmp(term, "linux"))
    {
        *use_color = true;
        *dark_mode = true;
        verbose_("(xmq) assuming dark bg\n");
        return XMQ_RENDER_COLOR_DARKBG;
    }

    char *xmq_mode = getenv("XMQ_BG");
    if (xmq_mode != NULL)
    {
        if (!strcmp(xmq_mode, "MONO"))
        {
            *use_color = false;
            *dark_mode = false;
            verbose_("(xmq) XMQ_BG set to MONO\n");
            return XMQ_RENDER_MONO;
        }
        if (!strcmp(xmq_mode, "LIGHT"))
        {
            *use_color = true;
            *dark_mode = false;
            verbose_("(xmq) XMQ_BG set to LIGHT\n");
            return XMQ_RENDER_COLOR_LIGHTBG;
        }
        if (!strcmp(xmq_mode, "DARK"))
        {
            *use_color = true;
            *dark_mode = true;
            verbose_("(xmq) XMQ_BG set to DARK\n");
            return XMQ_RENDER_COLOR_DARKBG;
        }
        *use_color = false;
        *dark_mode = false;
        verbose_("(xmq) XMQ_BG content is bad, using MONO\n");
        return XMQ_RENDER_MONO;
    }

    char *colorfgbg = getenv("COLORFGBG");
    if (colorfgbg != NULL)
    {
        // Black text on white background: 0;default;15
        // White text on black background: 15;default;0
        *use_color = true;
        *dark_mode = true;
        verbose_("(xmq) COLORFGBG means DARK\n");
        return XMQ_RENDER_COLOR_DARKBG;
    }

    if (!isatty(1))
    {
        *use_color = false;
        *dark_mode = false;
        verbose_("(xmq) using mono since output is not a tty\n");
        return XMQ_RENDER_MONO;
    }
    if (!isatty(0))
    {
        error_to_print_on_exit =
            "xmq: stdin is not a tty so xmq cannot talk to the terminal to detect if "
            "background dark/light, defaults to dark. To silence this warning please "
            "set environment variable XMQ_BG=MONO|DARK|LIGHT or supply the options: "
            "render_terminal --color --lightbg | --color --darkbg | --mono\n";
        *use_color = true;
        *dark_mode = true;
        verbose_("(xmq) Cannot talk to terminal, assuming DARK background.\n");
        return XMQ_RENDER_COLOR_DARKBG;
    }

    bool is_dark = true;
#ifndef PLATFORM_WINAPI
    if (!strcmp(term, "xterm-256color"))
    {
        enable_raw_mode();
        printf("\x1b]11;?\x07");
        fflush(stdout);

        fd_set readset;
        struct timeval time;
        int r = 0;
        int g = 0;
        int b = 0;

        // Wait at most 100ms for the terminal to respond.
        FD_ZERO(&readset);
        FD_SET(STDIN_FILENO, &readset);
        time.tv_sec = 0;
        time.tv_usec = 100000;

        if (select(STDIN_FILENO + 1, &readset, NULL, NULL, &time) == 1)
        {
            // Expected response:
            // \033]11;rgb:ffff/ffff/dddd\07
            char buf[25];
            memset(buf, 0, sizeof(buf));
            int i = 0;
            for (;;)
            {
                char c = getchar();
                if (c == 0x07) break;
                buf[i++] = c;
                if (i > 24) break;
            }

            if (buf[0] == 0x1b && 3 == sscanf(buf+1, "]11;rgb:%04x/%04x/%04x", &r, &g, &b))
            {
                double brightness = (0.2126*r + 0.7152*g + 0.0722*b)/256.0;
                if (brightness > 153) {
                    is_dark = false;
                }
            }
        }
        else
        {
            error_to_print_on_exit =
                "xmq: no response from terminal whether background is dark/light, defaults to dark.\n"
                "To silence this warning please set environment variable XMQ_BG=MONO|DARK|LIGHT.\n";
            verbose_("(xmq) Terminal does not respond with background color within 100ms.\n");
            is_dark = true;
        }
        disable_raw_mode();
    }
#endif

    if (is_dark)
    {
        *use_color = true;
        *dark_mode = true;
        verbose_("(xmq) terminal responds with dark background\n");
        return XMQ_RENDER_COLOR_DARKBG;
    }
    *use_color = true;
    *dark_mode = false;
    verbose_("(xmq) terminal responds with light background\n");
    return XMQ_RENDER_COLOR_LIGHTBG;
}

bool handle_global_option(const char *arg, XMQCliCommand *command)
{
    debug_("(xmq) option %s\n", arg);
    if (!strcmp(arg, "--help") ||
        !strcmp(arg, "-h"))
    {
        command->print_help = true;
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
    if (!strcmp(arg, "--version"))
    {
        command->print_version = true;
        return true;
    }
    if (!strcmp(arg, "--render-style"))
    {
        bool c, d;
        int rc = render_style(&c, &d);
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
    if (!strcmp(arg, "--xml"))
    {
        command->in_format=XMQ_CONTENT_XML;
        return true;
    }
    if (!strcmp(arg, "--html"))
    {
        command->in_format=XMQ_CONTENT_HTML;
        return true;
    }
    if (!strncmp(arg, "--tabsize=", 10))
    {
        for (const char *i = arg+10; *i; i++)
        {
            if (!isdigit(*i))
            {
                printf("xmq: tab size must be a positive integer\n");
                exit(1);
            }
        }
        command->tab_size = atoi(arg+10);
        return true;
    }
    if (!strncmp(arg, "--iroot=", 8))
    {
        command->implicit_root = arg+8;
        return true;
    }
    if (!strncmp(arg, "--trim=", 7))
    {
        if (!strcmp(arg+7, "default"))
        {
            command->trim = XMQ_TRIM_DEFAULT;
        }
        else if (!strcmp(arg+7, "none"))
        {
            command->trim = XMQ_TRIM_NONE;
        }
        else if (!strcmp(arg+7, "normal"))
        {
            command->trim = XMQ_TRIM_NORMAL;
        }
        else if (!strcmp(arg+7, "extra"))
        {
            command->trim = XMQ_TRIM_EXTRA;
        }
        else if (!strcmp(arg+7, "reshuffle"))
        {
            command->trim = XMQ_TRIM_RESHUFFLE;
        }
        else
        {
            printf("No such trim rule \"%s\"!\n", arg+7);
            exit(1);
        }
        return true;
    }

    return false;
}

void print_help_and_exit()
{
    printf("Usage: xmq [options] <file> ( <command> [options] )*\n"
           "\n"
           "  --root=<name>\n"
           "             Create a root node <name> unless the file starts with a node with this <name> already.\n"
           "  --xmq|--htmq|--xml|--html|--json\n"
           "             The input format is normally auto detected but you can force the input format here.\n"
           "  --trim=none|default|normal|extra|reshuffle\n"
           "             When reading the input data, the default setting for xml/html content is to trim whitespace using normal.\n"
           "             For xmq/htmq/json the default settings is none since whitespace is explicit in xmq/htmq/json.\n"
           "             none: Keep all whitespace as is.\n"
           "             default: Use normal for xml/html and none for xmq/htmq/json.\n"
           "             normal: Remove leading ending whitespace and incindental indentation.\n"
           "             extra: Like normal but also squeeze multiple consecutive whitespaces int a single whitespace.\n"
           "             reshuffle: Like extra but also move words between lines to shrink line width.\n"
           "  --help     Display this help and exit.\n"
           "  --verbose  Output extra information on stderr.\n"
           "  --debug    Output debug information on stderr.\n"
           "  --version  Output version information and exit.\n"
           "\n"
           "COMMANDS\n"
           "to_xmq\n"
           "to_htmq\n"
           "             Write the content as xmq/htmq on stdout. If stdout is a tty, then this command behaves as render_terminal.\n"
           "  --compact\n"
           "             By default to_xmq pretty-prints the output. Using this option will result in a single line compact xmq/htmq.\n"
           "  --indent=n\n"
           "             Use the given number of spaces for indentation. Default is 4.\n"
           "  --escape-newlines\n"
           "             Use the entity &#10; instead of actual newlines in xmq quotes. This is automatic in compact mode.\n"
           "  --escape-non-7bit\n"
           "             Escape all non-7bit chars using entities like &#160;\n"
           "\n"
           "to_xml\n"
           "to_html\n"
           "to_json\n"
           "             Write the content as xml/html/json on stdout.\n"
           "\n"
           "render_terminal\n"
           "render_html\n"
           "render_tex\n"
           "             Render the content as xmq/htmq for presentation on a terminal, as html or as LaTeX.\n"
           "  --color\n"
           "  --mono\n"
           "             By default, xmq generates syntax colored output if writing to a terminal.\n"
           "             You can force it to produce color even if writing to a pipe or a file using --color,\n"
           "             and disable color with --mono.\n"
           "             Colors can be configured with the XMQ_COLORS environment variable.\n"
           "  --lightbg\n"
           "  --darkbg\n"
           "             Use a colorscheme suitable for a light background or a dark background.\n"
           "  --nostyle\n"
           "             Do not output html/tex preamble/postamble.\n"
           "  --onlystyle\n"
           "             Output only the html/tex preamble.\n"
           "\n"
           "  You can also use --compact, --indent=n, --escape-newlines and --escape-non-7bit with the render commands.\n"
           "\n"
           "tokenize\n"
           "             Do not create a DOM tree for the content, just tokenize the input. Each token can be printed\n"
           "             using colors for terminal/html/tex or with location information or with debug information.\n"
           "             Location information is useful for editors to get help on syntax highlighting.\n"
           "  --type=[location|terminal|tex|debugtokens|debugcontent]\n"
           "\n"
           "select\n"
           "delete\n"
           "             Select or delete nodes in the DOM.\n"
           "  --xpath=<xpath-expression>\n"
           "             Select or delete nodes matching this xpath expression.\n"
           "  --entity=<entity-name>\n"
           "             Select or delete entity nodes matching this name.\n"
           "\n"
           "replace\n"
           "             Replace parts of the DOM.\n"
           "  --xpath=<xpath-expression>\n"
           "             Replace nodes matching this xpath expression.\n"
           "  --entity=<entity-name>\n"
           "             Replace entity nodes matching this name.\n"
           "  --text=<text>\n"
           "             Replace with this text. The text is safely quoted for insertion into the document.\n"
           "  --textfile=<file-name>\n"
           "             Replace with the text from this file. The text is safely quoted for insertion into the document.\n"
           "  --file=<file-name>\n"
           "             Replace with the content of this file which has to be proper xmq/htmq/xml/html/json.\n"
           "\nIf a single minus is given as <file> then xmq reads from stdin.\n"
           "If neither <file> nor <command> given, then the xmq reads from stdin.\n");
    exit(0);
}

void print_version_and_exit()
{
    printf("xmq: %s\n", xmqVersion());
    exit(0);
}

int tokenize_input(XMQCliCommand *command)
{
    verbose_("(xmq) tokenizing input\n");
    XMQOutputSettings *output_settings = xmqNewOutputSettings();
    xmqSetupPrintStdOutStdErr(output_settings);
    xmqSetupDefaultColors(output_settings, command->dark_mode);

    XMQParseCallbacks *callbacks = xmqNewParseCallbacks();

    switch (command->tok_type) {
    case XMQ_CLI_TOKENIZE_TERMINAL:
        xmqSetupDefaultColors(output_settings, command->dark_mode);
        xmqSetupParseCallbacksColorizeTokens(callbacks, XMQ_RENDER_TERMINAL, command->dark_mode);
        break;
    case XMQ_CLI_TOKENIZE_HTML:
        xmqSetupDefaultColors(output_settings, command->dark_mode);
        xmqSetupParseCallbacksColorizeTokens(callbacks, XMQ_RENDER_HTML, command->dark_mode);
        break;
    case XMQ_CLI_TOKENIZE_TEX:
        xmqSetupDefaultColors(output_settings, command->dark_mode);
        xmqSetupParseCallbacksColorizeTokens(callbacks, XMQ_RENDER_TEX, command->dark_mode);
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
    xmqTokenizeFile(state, command->in);

    int err = 0;
    if (xmqStateErrno(state))
    {
        fprintf(stderr, "%s\n", xmqStateErrorMsg(state));
        err = xmqStateErrno(state);
    }

    xmqFreeParseState(state);
    xmqFreeParseCallbacks(callbacks);
    xmqFreeOutputSettings(output_settings);

    return err;
}

void write_print(void *buffer, const char *content)
{
    printf("%s", content);
}

int cmd_load(XMQCliCommand *command)
{
    command->env->doc = xmqNewDoc();

    if (command &&
        command->in &&
        command->in[0] == '-' &&
        command->in[1] == 0)
    {
        command->in = NULL;
    }

    bool ok = xmqParseFileWithType(command->env->doc, command->in, command->implicit_root, command->in_format, command->trim);
    if (!ok)
    {
        int rc = xmqDocErrno(command->env->doc);
        const char *error = xmqDocError(command->env->doc);
        fprintf(stderr, error, command->in);
        xmqFreeDoc(command->env->doc);
        command->env->doc = NULL;
        return rc;
    }

    verbose_("(xmq) loaded %s\n", command->in);

    return 0;
}

void cmd_unload(XMQCliCommand *command)
{
    if (command && command->env && command->env->doc)
    {
        verbose_("(xmq) unloading document\n");
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
    xmqSetAddIndent(settings, command->add_indent);
    xmqSetUseColor(settings, command->use_color);
    xmqSetOutputFormat(settings, command->out_format);
    xmqSetRenderFormat(settings, command->render_to);
    xmqSetRenderRaw(settings, command->render_raw);
    xmqSetRenderOnlyStyle(settings, command->only_style);
    xmqSetupDefaultColors(settings, command->dark_mode);

    /*
    verbose_("(xmq) print %s render %s\n",
             content_type_to_string(settings->output_format),
             render_format_to_string(settings->render_to)
        );
    */
    xmqSetupPrintStdOutStdErr(settings);
    xmqPrint(command->env->doc, settings);
    printf("\n");

    xmqFreeOutputSettings(settings);
    return true;
}

bool cmd_transform(XMQCliCommand *command)
{
    xmlDocPtr doc = (xmlDocPtr)xmqGetImplementationDoc(command->env->doc);
    verbose_("(xmq) transforming\n");

    xmlDocPtr new_doc = xsltApplyStylesheet(command->xslt, doc, NULL);

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

bool cmd_delete(XMQCliCommand *command)
{
    xmlDocPtr doc = (xmlDocPtr)xmqGetImplementationDoc(command->env->doc);

    xmlXPathContextPtr ctx = xmlXPathNewContext(doc);
    assert(ctx);

    xmlXPathObjectPtr objects = xmlXPathEvalExpression((const xmlChar*)command->xpath, ctx);

    verbose_("(xmq) deleting %s\n", command->xpath);
    if (objects == NULL)
    {
        verbose_("(xmq) no nodes deleted\n");
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

void replace_entity(xmlDoc *doc, xmlNode *node, const char *entity, const char *content, xmlNodePtr node_content)
{
    if (!node) return;

    if (node->type == XML_ENTITY_REF_NODE)
    {
        if (!strcmp((const char *)node->name, entity))
        {
            if (content)
            {
                xmlNodePtr new_node = xmlNewDocText(doc, (const xmlChar*)content);
                xmlReplaceNode(node, new_node);
                xmlFreeNode(node);
            }
            if (node_content)
            {
                xmlNodePtr new_node = xmlCopyNode(node_content, 1);
                xmlReplaceNode(node, new_node);
                xmlFreeNode(node);
            }
        }
        return;
    }

    xmlNode *i = node->children;
    while (i)
    {
        xmlNode *next = i->next;
        replace_entity(doc, i, entity, content, node_content);
        i = next;
    }
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
        verbose_("(xmq) replacing xpath %s\n", command->xpath);
    }

    if (command->entity)
    {
        verbose_("(xmq) replacing entity %s\n", command->entity);

        replace_entities(doc, command->entity, command->content, command->node_content);
    }

    return true;
}

void prepare_command(XMQCliCommand *c)
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
    case XMQ_CLI_CMD_TOKENIZE:
        return;
    case XMQ_CLI_CMD_DELETE:
        return;
    case XMQ_CLI_CMD_DELETE_ENTITY:
        return;
    case XMQ_CLI_CMD_REPLACE:
        return;
    case XMQ_CLI_CMD_REPLACE_ENTITY:
        return;
    case XMQ_CLI_CMD_TRANSFORM:
        return;
    case XMQ_CLI_CMD_NONE:
        return;
    }
}

bool perform_command(XMQCliCommand *c)
{
    if (c->cmd == XMQ_CLI_CMD_NONE) return true;

    switch (c->cmd) {
    case XMQ_CLI_CMD_NONE:
        return true;
    case XMQ_CLI_CMD_TO_XMQ:
    case XMQ_CLI_CMD_TO_XML:
    case XMQ_CLI_CMD_TO_HTMQ:
    case XMQ_CLI_CMD_TO_HTML:
    case XMQ_CLI_CMD_TO_JSON:
    case XMQ_CLI_CMD_RENDER_TERMINAL:
    case XMQ_CLI_CMD_RENDER_HTML:
    case XMQ_CLI_CMD_RENDER_TEX:
        return cmd_to(c);
    case XMQ_CLI_CMD_TOKENIZE:
        return tokenize_input(c);
    case XMQ_CLI_CMD_DELETE:
    case XMQ_CLI_CMD_DELETE_ENTITY:
        return cmd_delete(c);
    case XMQ_CLI_CMD_REPLACE:
    case XMQ_CLI_CMD_REPLACE_ENTITY:
        return cmd_replace(c);
    case XMQ_CLI_CMD_TRANSFORM:
        return cmd_transform(c);
    }
    assert(false);
    return false;
}

bool xmq_parse_cmd_line(int argc, const char **argv, XMQCliCommand *command)
{
    int i = 1;

    for (i = 1; argv[i]; ++i)
    {
        const char *arg = argv[i];

        // Stop handling options when arg does not start with - or is exactly "-"
        if (arg[0] != '-' || !strcmp(arg, "-")) break;

        bool ok = handle_global_option(arg, command);
        if (ok) continue;

        printf("xmq: unrecognized global option: '%s'\nTry 'xmq --help' for more information\n", arg);
        return false;
    }

    // If no file name (nor commands) are supplied, or the filename is - then read from stdin.
    if (!argv[i])
    {
        command->cmd = XMQ_CLI_CMD_TO_XMQ;
        return true;
    }

    // Check if not a command, then assume a file.
    // You cannot load files that are named like the commands.
    if (cmd_from(argv[i]) == XMQ_CLI_CMD_NONE)
    {
        // Next argument is the file name or - for reading from stdin.
        command->in = argv[i];
        i++;
    }

    command->cmd = cmd_from(argv[i]);
    i++;

    if (argv[i-1] && command->cmd == XMQ_CLI_CMD_NONE)
    {
        fprintf(stderr, "xmq: no such command \"%s\"\n", argv[i-1]);
        exit(1);
    }

    XMQCliCommand *com = command;
    prepare_command(com);

    while (com->cmd != XMQ_CLI_CMD_NONE)
    {
        for (; argv[i]; ++i)
        {
            const char *arg = argv[i];
            bool ok = handle_option(arg, com);

            if (!ok)
            {
                if (cmd_from(arg) != XMQ_CLI_CMD_NONE) break; // Start next command.
                // Else this is bad....
                printf("xmq: option \"%s\" not available for command \"%s\"\n", arg, cmd_name(com->cmd));
                exit(1);
            }
        }

        if (argv[i] == NULL) break;
        XMQCliCommand *prev = com;
        com = allocate_cli_command(command->env);
        prev->next = com;
        com->cmd = cmd_from(argv[i]);
        i++;
        prepare_command(com);
    }

    if (com->cmd == XMQ_CLI_CMD_NONE ||
        (cmd_group(com->cmd) != XMQ_CLI_CMD_GROUP_RENDER &&
         cmd_group(com->cmd) != XMQ_CLI_CMD_GROUP_TO))
    {
        com->next = allocate_cli_command(command->env);
        com->next->cmd = XMQ_CLI_CMD_TO_XMQ;
        prepare_command(com->next);
    }

    return true;
}

#ifdef PLATFORM_WINAPI
void enableAnsiColorsTerminal()
{
    HANDLE hStdOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hStdOut == INVALID_HANDLE_VALUE) return; // Fail

    DWORD mode;
    if (!GetConsoleMode(hStdOut, &mode)) return; // Fail

    DWORD enabled = (mode & ENABLE_VIRTUAL_TERMINAL_PROCESSING) ? true : false;

    if (enabled) return; // Already enabled colors.

    mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;

    SetConsoleMode(hStdOut, mode);

    SetConsoleOutputCP(CP_UTF8);
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

int main(int argc, const char **argv)
{
    if (argc < 2 && isatty(0)) print_help_and_exit();

    verbose_enabled__ = has_verbose(argc, argv);
    debug_enabled__ = has_debug(argc, argv);

    XMQCliEnvironment env;
    memset(&env, 0, sizeof(env));

    if (isatty(1))
    {
        // Lets try to use detection of background. If any
        // of --mono or --color --darkbg or --ligtbg is set, then do not detect.
        env.use_detect = true;
        // Output is a tty, lets use colors!
        env.use_color = true;
        // Default to dark mode.
        env.dark_mode = true;

#ifdef PLATFORM_WINAPI
        enableAnsiColorsTerminal();
#endif
    }

    if (env.use_detect)
    {
        // See if we can find from the env variables or the terminal if it is dark mode or not.
        render_style(&env.use_color, &env.dark_mode);
    }

    XMQCliCommand *first_command = allocate_cli_command(&env);

    bool ok = xmq_parse_cmd_line(argc, argv, first_command);
    if (!ok) return 1;

    debug_enabled__ = first_command->debug;
    verbose_enabled__ = first_command->verbose || debug_enabled__;
    xmqSetDebug(debug_enabled__);
    xmqSetVerbose(verbose_enabled__);

    if (first_command->print_version) print_version_and_exit();
    if (first_command->print_help)  print_help_and_exit();

    XMQCliCommand *c = first_command;
    int rc = cmd_load(c);

    if (!rc)
    {
        // Execute commands.
        while (c)
        {
            bool ok = perform_command(c);
            if (!ok)
            {
                rc = 1;
                break;
            }
            c = c->next;
        }
    }

    // Free document.
    cmd_unload(first_command);

    // Free commands.
    c = first_command;
    while (c)
    {
        XMQCliCommand *tmp = c;
        c = c->next;
        free(tmp);
    }

    if (error_to_print_on_exit) fprintf(stderr, "%s", error_to_print_on_exit);

    return rc;
}
