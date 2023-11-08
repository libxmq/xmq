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

#ifndef PLATFORM_WINAPI
#include<termios.h>
#endif

#define LIBXML_STATIC
#define LIBXSLT_STATIC
#define XMLSEC_STATIC

#include<libxml/tree.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>

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
    XMQ_CLI_CMD_ENTITY
} XMQCliCmd;

typedef enum {
    XMQ_CLI_CMD_GROUP_NONE,
    XMQ_CLI_CMD_GROUP_TO,
    XMQ_CLI_CMD_GROUP_RENDER,
    XMQ_CLI_CMD_GROUP_TOKENIZE,
    XMQ_CLI_CMD_GROUP_MATCHERS,
    XMQ_CLI_CMD_GROUP_ENTITIES,
} XMQCliCmdGroup;

typedef struct XMQCliEnvironment XMQCliEnvironment;
struct XMQCliEnvironment
{
    XMQDoc *doc;
    bool use_color;
    bool dark_mode;
};

typedef struct XMQCliCommand XMQCliCommand;

struct XMQCliCommand
{
    XMQCliEnvironment *env;
    XMQCliCmd cmd;
    char *in;
    char *out;
    const char *xpath;
    const char *entity;
    const char *content;
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

bool verbose_enabled_ = false;

void verbose(const char* fmt, ...)
{
    if (verbose_enabled_) {
        va_list args;
        va_start(args, fmt);
        vfprintf(stderr, fmt, args);
        va_end(args);
    }
}

bool debug_enabled_ = false;

void debug(const char* fmt, ...)
{
    if (debug_enabled_) {
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
    if (!strcmp(s, "entity")) return XMQ_CLI_CMD_ENTITY;
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
    case XMQ_CLI_CMD_ENTITY: return "entity";
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
        return XMQ_CLI_CMD_GROUP_MATCHERS;
    case XMQ_CLI_CMD_ENTITY:
        return XMQ_CLI_CMD_GROUP_ENTITIES;
    case XMQ_CLI_CMD_NONE:
        return XMQ_CLI_CMD_GROUP_NONE;
    }
    return XMQ_CLI_CMD_GROUP_NONE;
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
            command->use_color = true;
            if (command->render_to == XMQ_RENDER_PLAIN)
            {
                command->render_to = XMQ_RENDER_TERMINAL;
            }
            return true;
        }
        if (!strcmp(arg, "--mono"))
        {
            command->use_color = false;
            return true;
        }
        if (!strcmp(arg, "--lightbg"))
        {
            command->dark_mode = false;
            return true;
        }
        if (!strcmp(arg, "--darkbg"))
        {
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

    if (group == XMQ_CLI_CMD_GROUP_MATCHERS && command->xpath == NULL)
    {
        command->xpath = arg;
        return true;
    }

    if (group == XMQ_CLI_CMD_GROUP_ENTITIES)
    {
        if (command->entity == NULL)
        {
            command->entity = arg;
            return true;
        }

        if (group == XMQ_CLI_CMD_GROUP_ENTITIES && command->entity != NULL)
        {
            command->content = arg;
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

bool is_bg_dark()
{
#ifndef PLATFORM_WINAPI
    bool is_dark = true;

    char *colorfgbg = getenv("COLORFGBG");
    if (colorfgbg != NULL)
    {
        return true;
    }

    char *term = getenv("TERM");
    if (!strcmp(term, "linux"))
    {
        return true;
    }

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
        disable_raw_mode();
    }
    return is_dark;
#else
    return false;
#endif
}

bool handle_global_option(const char *arg, XMQCliCommand *command)
{
    debug("(xmq) option %s\n", arg);
    if (!strcmp(arg, "--help") ||
        !strcmp(arg, "-h"))
    {
        command->print_help = true;
        return true;
    }
    if (!strcmp(arg, "--debug"))
    {
        command->debug = true;
        debug_enabled_ = true;
        verbose_enabled_ = true;
        return true;
    }
    if (!strcmp(arg, "--verbose"))
    {
        command->verbose = true;
        verbose_enabled_ = true;
        return true;
    }
    if (!strcmp(arg, "--version"))
    {
        command->print_version = true;
        return true;
    }
    if (!strcmp(arg, "--is-dark-bg"))
    {
        if (is_bg_dark()) exit(0); else exit(1);
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
    printf("xmq - xml/xmq/html/htmq/json processor [version %s]\n"
           "....\n",
           xmqVersion());
    exit(0);
}

void print_version_and_exit()
{
    printf("xmq: %s\n%s\n", xmqVersion(), xmqCommit());
    exit(0);
}

int tokenize_input(XMQCliCommand *command)
{
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

    verbose("(xmq) loading %s\n", command->in);

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

    return 0;
}

void cmd_unload(XMQCliCommand *command)
{
    if (command && command->env && command->env->doc)
    {
        debug("(xmq) unloading document\n");
        xmqFreeDoc(command->env->doc);
        command->env->doc = NULL;
    }
}

bool cmd_to(XMQCliCommand *command)
{
    XMQOutputSettings *settings = xmqNewOutputSettings();
    settings->compact = command->compact;
    settings->escape_newlines = command->escape_newlines;
    settings->escape_non_7bit = command->escape_non_7bit;
    settings->add_indent = command->add_indent;
    settings->use_color = command->use_color;
    settings->output_format = command->out_format;
    settings->render_to = command->render_to;
    settings->render_raw = command->render_raw;
    settings->only_style = command->only_style;
    xmqSetupDefaultColors(settings, command->dark_mode);

    xmqSetupPrintStdOutStdErr(settings);
    xmqPrint(command->env->doc, settings);
    printf("\n");

    xmqFreeOutputSettings(settings);
    return true;
}

bool cmd_delete(XMQCliCommand *command)
{
    xmlDocPtr doc = (xmlDocPtr)xmqGetImplementationDoc(command->env->doc);

    xmlXPathContextPtr ctx = xmlXPathNewContext(doc);
    assert(ctx);

    xmlXPathObjectPtr objects = xmlXPathEvalExpression((const xmlChar*)command->xpath, ctx);

    if(objects == NULL)
    {
        verbose("xmq: no nodes deleted\n");
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

void replace_entities(xmlNodePtr node, const char *entity, const char *content)
{
    xmlNodePtr i = node;
    if (!i) return;

    if (i->type == XML_ENTITY_REF_NODE)
    {
        printf("ENTITY %s\n", i->name);
        return;
    }

    while (i)
    {
        replace_entities(i->children, entity, content);
        i = i->next;
    }
}

bool cmd_entity(XMQCliCommand *command)
{
    xmlDocPtr doc = (xmlDocPtr)xmqGetImplementationDoc(command->env->doc);

    replace_entities((xmlNodePtr)doc, command->entity, command->content);

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
    case XMQ_CLI_CMD_ENTITY:
        return;
    case XMQ_CLI_CMD_NONE:
        return;
    }
}

bool perform_command(XMQCliCommand *c)
{
    if (c->cmd == XMQ_CLI_CMD_NONE) return true;

    debug("(xmq) perform %s\n", cmd_name(c->cmd));
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
        return cmd_delete(c);
    case XMQ_CLI_CMD_ENTITY:
        return cmd_entity(c);
    }
    assert(false);
}

bool xmq_parse_cmd_line(int argc, char **argv, XMQCliCommand *command)
{
    int i = 1;

    for (i = 1; argv[i]; ++i)
    {
        char *arg = argv[i];

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
        //printf("COMMAND %d %s\n", com->cmd, cmd_name(com->cmd));
        for (; argv[i]; ++i)
        {
            char *arg = argv[i];
            //printf("ARGV %d %s use_color=%d\n", i, arg, com->use_color);
            bool ok = handle_option(arg, com);
            //printf("ARGV post  %d %s use_color=%d\n", i, arg, com->use_color);

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
        debug("(xmq) added implicit to_xmq command\n");
        com->next = allocate_cli_command(command->env);
        com->next->cmd = XMQ_CLI_CMD_TO_XMQ;
        prepare_command(com->next);
    }

    return true;
}

int main(int argc, char **argv)
{
    if (argc < 2 && isatty(0)) print_help_and_exit();

    XMQCliEnvironment env;
    memset(&env, 0, sizeof(env));

    if (isatty(1))
    {
        env.use_color = true;
        env.dark_mode = false;
        if (isatty(0))
        {
            env.dark_mode = is_bg_dark();
        }
    }
    XMQCliCommand *first_command = allocate_cli_command(&env);

    bool ok = xmq_parse_cmd_line(argc, argv, first_command);
    if (!ok) return 1;

    debug_enabled_ = first_command->debug;
    verbose_enabled_ = first_command->verbose || debug_enabled_;
    xmqSetDebug(debug_enabled_);
    xmqSetVerbose(verbose_enabled_);

    if (first_command->print_version) print_version_and_exit();
    if (first_command->print_help)  print_help_and_exit();

    XMQCliCommand *c = first_command;
    int rc = cmd_load(c);

    if (!rc) {
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

    return rc;
}
