/* libxmq - Copyright (C) 2026 Fredrik Öhrström (spdx: MIT) */

package org.libxmq.imp;

import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.List;

import org.libxmq.OutputSettings;
import org.libxmq.ParseException;
import org.libxmq.XMQ;
import org.libxmq.imp.ixml.IXMLAnalyzer;
import org.libxmq.imp.xml.HtmlSave;
import org.libxmq.imp.xml.XPath;
import org.libxmq.imp.xml.XPathException;
import org.libxmq.imp.xml.XmlAttr;
import org.libxmq.imp.xml.XmlDocument;
import org.libxmq.imp.xml.XmlElement;
import org.libxmq.imp.xml.XmlEntityRef;
import org.libxmq.imp.xml.XmlNode;
import org.libxmq.imp.xml.XmlNodeType;
import org.libxmq.imp.xml.XmlSave;
import org.libxmq.imp.xml.XmlText;
import org.libxmq.imp.xml.XmlTrim;

/**
 * The xmqj command line, parsed with the grammar library/xmq-utils/xmqcli.ixml
 * like the C command line.
 *
 * The arguments are joined into a single line (CliJoin), parsed by the
 * generated XMQCliParser and the tree is walked into the global settings and
 * a list of commands, which are then performed on the loaded document like
 * perform_command in xmq-cli.c.
 */
final class Cli
{
    /** Thrown to stop with an exit code, after printing an error message. */
    static final class Exit extends Exception
    {
        private static final long serialVersionUID = 1L;
        final int code;
        Exit(int code) { super(null, null, false, false); this.code = code; }
    }

    /** A command with its options and arguments, in the order given. */
    static final class Command
    {
        String name;
        final List<String[]> options = new ArrayList<>(); // {name, value or null}
        final List<String> args = new ArrayList<>();

        // Output settings of to/render commands.
        boolean compact, escape_tabs, escape_newlines, escape_non7bit, prefer_double_quotes, no_final_nl;
        boolean omit_decl, render_raw, add_nl;
        int indent = 4;
        // The coloring, like the C XMQCliCommand use_color, bg_dark_mode, bg_forced
        // and truecolor. Copied from the environment, then changed by --bg=.
        boolean use_color, bg_dark_mode, bg_forced, truecolor;
        String theme_spec;        // --theme= or XMQ_THEME
        String use_id, use_class;  // --id= --class= of render-html
        boolean only_style;       // --onlystyle
        // --use-is= --use-es= --use-et= --use-cr= --use-nl= of the render commands.
        String use_is, use_es, use_et, use_cr, use_nl;
        String tokenize_type;
        int with_file; // replace-entity --with-file (1) or --with-text-file (2)
        // transform: the compiled stylesheet and the --stringparam/--param values (xpath expressions).
        org.libxmq.imp.xml.libxslt1.Stylesheet xslt;
        final java.util.LinkedHashMap<String, String> xslt_params = new java.util.LinkedHashMap<>();
        // validate: the compiled xsd, its file name and --silent.
        Validate xsd;
        String xsd_name;
        boolean silent;
    }

    /** exit(code) called deep inside a library callback in the C version (the xsd messages). */
    static final class ExitNow extends RuntimeException
    {
        private static final long serialVersionUID = 1L;
        final int code;
        ExitNow(int code) { super(null, null, false, false); this.code = code; }
    }

    // Global settings.
    boolean trim_none, trim_heuristic, no_merge, lines, no_input, in_is_content;
    /** --ixml-fail-silent: a rejected ixml input gives an empty document. */
    boolean ixml_fail_silent;
    /** An ixml input was rejected, the failure document was output and xmq exits with 1. */
    boolean ixml_failed;
    /** --we-should-stop-using-yaml: no reminder comment before the to-yaml output. */
    boolean we_should_stop_using_yaml;
    String in_format;        // null = detect, else xmq xml html text
    String in;              // file name, "-" for stdin, or the content with -i
    String ixml_source;      // --ixml=grammar.ixml or a positional grammar
    final List<Command> commands = new ArrayList<>();
    boolean yaml_input;      // The input was loaded as yaml.
    String loaded_content;   // The content of the current document.
    byte[] input_bytes;      // The undecoded input file or stdin, null for -i content.

    static final String[] NOT_IMPLEMENTED = {
        "for-each", "statistics",
        "substitute-entity", "substitute-char-entities", "page", "pa", "browse", "br", "heu",
        "load", "xsl", "replace",
    };

    /**
     * Run the command line.
     * @param args The command line arguments.
     * @return The exit code.
     */
    static int run(String[] args)
    {
        Cli cli = new Cli();
        try
        {
            cli.parse(args);
            cli.perform();
            return 0;
        }
        catch (Exit e)
        {
            return e.code;
        }
        catch (ExitNow e)
        {
            return e.code;
        }
    }

    static Exit fail(String message)
    {
        System.err.println(message);
        return new Exit(1);
    }

    // ------------------------------------------------------------ the tree

    void parse(String[] args) throws Exit
    {
        String line = CliJoin.join(args);
        XmlDocument tree;
        try
        {
            tree = XMQCliParser.parse(line);
        }
        catch (XMQCliParser.ParseError e)
        {
            StringBuilder b = new StringBuilder("xmq: bad command line, " + e.getMessage() + "\n");
            if (line.indexOf('\n') < 0 && line.length() < 200)
            {
                b.append(line).append('\n').append(" ".repeat(Math.max(0, e.col - 1))).append("^\n");
            }
            b.append("Try 'xmq --help' for more information");
            throw fail(b.toString());
        }
        for (XmlElement e : tree.rootElement().elements())
        {
            switch (e.name())
            {
            case "option" -> globalOption(e.attributeValue("name"), value(e));
            case "grammar" -> grammar(e.textContent());
            case "input" -> { in_is_content = true; in = e.textContent(); }
            case "file" -> in = e.textContent();
            case "command" -> command(e);
            default -> throw fail("xmq: unexpected " + e.name() + " in the command line tree");
            }
        }
    }

    // The value attribute, null if not present (an empty value is "").
    static String value(XmlElement e)
    {
        XmlAttr a = e.attribute("value");
        return a == null ? null : e.attributeValue("value");
    }

    static String optionString(String name, String value)
    {
        return value == null ? "--" + name : "--" + name + "=" + value;
    }

    void globalOption(String name, String value) throws Exit
    {
        switch (name)
        {
        case "help" -> { Main.printHelp(); throw new Exit(0); }
        case "no-input" -> no_input = true;
        case "trim" -> {
            if ("none".equals(value)) trim_none = true;
            else if ("heuristic".equals(value)) { trim_none = false; trim_heuristic = true; }
            else throw fail("No such trim rule \"" + value + "\"!");
        }
        case "no-merge", "nomerge" -> no_merge = true;
        case "we-should-stop-using-yaml" -> we_should_stop_using_yaml = true;
        case "lines" -> lines = true;
        case "xmq", "htmq" -> in_format = "xmq";
        case "xml" -> in_format = "xml";
        case "html" -> in_format = "html";
        case "text" -> in_format = "text";
        case "ixml" -> {
            if (value == null) throw fail("xmq: --ixml as an input format is not implemented in xmqj yet");
            grammar(value);
        }
        case "json" -> in_format = "json";
        case "clines" -> in_format = "text"; // Like C, clines input is loaded as text.
        case "yaml" -> in_format = "yaml";
        case "root", "ixml-engine", "list-library", "xml-of-ixml" ->
            throw fail("xmq: the global option " + optionString(name, value) + " is not implemented in xmqj yet");
        // Logging and things that do not change the result.
        case "debug", "verbose", "trace", "log-xmq", "progress", "yes", "force-download",
             "ixml-all-parses", "ixml-try-to-recover" -> { }
        case "ixml-fail-silent" -> ixml_fail_silent = true;
        case "version" -> { System.out.println("xmqj"); throw new Exit(0); }
        // Like the C version built without any XMQ_NO_... define.
        case "features" -> { System.out.println("xmq-printing json yaml xslt xsd ixml html"); throw new Exit(0); }
        default -> throw fail("xmq: unrecognized global option: '" + optionString(name, value)
                              + "'\nTry 'xmq --help' for more information");
        }
    }

    void grammar(String g) throws Exit
    {
        if (ixml_source != null) throw fail("You have already specified an ixml grammar!");
        if (g.startsWith("ixml:")) throw fail("xmq: library grammars (" + g + ") are not implemented in xmqj yet");
        try
        {
            ixml_source = Files.readString(Path.of(g), StandardCharsets.UTF_8);
        }
        catch (IOException e)
        {
            throw fail("xmq: " + g + ": No such file or directory");
        }
    }

    void command(XmlElement e) throws Exit
    {
        Command c = new Command();
        c.name = e.attributeValue("name");
        c.use_color = env_use_color;
        c.bg_dark_mode = env_bg_dark_mode;
        c.bg_forced = env_bg_forced;
        c.truecolor = env_truecolor;
        c.theme_spec = System.getenv("XMQ_THEME");
        for (String n : NOT_IMPLEMENTED)
        {
            if (n.equals(c.name)) throw fail("xmq: the command " + c.name + " is not implemented in xmqj yet");
        }
        if (c.name.equals("to-json")) { c.compact = true; c.indent = 0; }
        if (c.name.equals("to-yaml")) c.indent = 2; // The default yaml output indentation.
        for (XmlElement p : e.elements())
        {
            if (c.name.equals("validate"))
            {
                // Like handle_option of the validate group in the C version.
                boolean is_option = p.name().equals("option");
                String arg = is_option ? optionString(p.attributeValue("name"), value(p)) : p.textContent();
                if (!validateArg(c, arg, is_option))
                {
                    if (is_option) System.out.print("xmq: option \"" + arg + "\" not available for command \"validate\"\n");
                    else System.out.print("xmq: bad argument \"" + arg + "\" for command \"validate\"\n");
                    throw new Exit(1);
                }
                continue;
            }
            if (p.name().equals("option")) commandOption(c, p.attributeValue("name"), value(p));
            else c.args.add(p.textContent());
        }
        if (c.name.equals("transform") && !c.args.isEmpty())
        {
            // Like C, the stylesheet is loaded and compiled when the command line is read.
            String f = c.args.get(0);
            if (!loadXslt(c, f)) throw fail("xmq: bad argument \"" + f + "\" for command \"transform\"");
        }
        commands.add(c);
    }

    static boolean isTo(String n)
    {
        return n.startsWith("to-") || n.startsWith("render-") || n.equals("tokenize");
    }

    void commandOption(Command c, String name, String value) throws Exit
    {
        c.options.add(new String[] { name, value });
        boolean to = isTo(c.name);
        boolean render = c.name.startsWith("render-");
        if (to && value == null)
        {
            switch (name)
            {
            case "compact" -> { c.compact = true; c.indent = 0; return; }
            case "pretty" -> { c.compact = false; c.indent = c.name.equals("to-json") || c.name.equals("to-yaml") ? 2 : 4; return; }
            case "no-final-nl" -> { c.no_final_nl = true; return; }
            default -> { }
            }
        }
        if ((c.name.equals("to-xmq") || c.name.equals("to-htmq") || render) && value == null)
        {
            switch (name)
            {
            case "escape-tabs" -> { c.escape_tabs = true; return; }
            case "escape-newlines" -> { c.escape_newlines = true; return; }
            case "escape-non-7bit" -> { c.escape_non7bit = true; return; }
            case "prefer-double-quotes" -> { c.prefer_double_quotes = true; return; }
            default -> { }
            }
        }
        if ((c.name.equals("to-xmq") || c.name.equals("to-htmq") || render) && name.equals("indent") && value != null)
        {
            if (!value.matches("[0-9]+")) throw fail("xmq: indent must be a positive integer");
            c.indent = Integer.parseInt(value);
            return;
        }
        if (c.name.equals("to-xml") && name.equals("omit-decl") && value == null) { c.omit_decl = true; return; }
        if (render)
        {
            if (name.equals("nostyle") && value == null) { c.render_raw = true; return; }
            if (name.equals("onlystyle") && value == null) { c.only_style = true; return; }
            if (value != null)
            {
                switch (name)
                {
                case "theme" -> { c.theme_spec = value; return; }
                case "class" -> { c.use_class = value; return; }
                case "id" -> { c.use_id = value; return; }
                case "use-is" -> { c.use_is = value; return; }
                case "use-es" -> { c.use_es = value; return; }
                case "use-et" -> { c.use_et = value; return; }
                case "use-cr" -> { c.use_cr = value; return; }
                case "use-nl" -> { c.use_nl = value; return; }
                default -> { }
                }
            }
            if (name.equals("bg") && value != null)
            {
                switch (value)
                {
                case "light" -> { c.use_color = true; c.bg_dark_mode = false; c.bg_forced = true; }
                case "dark" -> { c.use_color = true; c.bg_dark_mode = true; c.bg_forced = true; }
                case "mono" -> { c.use_color = false; c.bg_dark_mode = false; }
                default -> throw fail("xmq: unknown bg setting \"" + value + "\"");
                }
                return;
            }
        }
        if (c.name.equals("transform") && (name.equals("stringparam") || name.equals("param")) && value != null)
        {
            String arg = "--" + name + "=" + value;
            int eq = value.indexOf('=');
            if (eq < 0)
            {
                throw fail("Invalid: " + arg + "\nUsage: --stringparam=key=value");
            }
            String key = value.substring(0, eq);
            String val = value.substring(eq + 1);
            if (name.equals("stringparam"))
            {
                boolean sq = val.indexOf('\'') >= 0, dq = val.indexOf('"') >= 0;
                if (sq && dq)
                {
                    throw fail("Invalid: " + arg + "\nA string param cannot contain both single and double quotes.");
                }
                char q = sq ? '"' : '\'';
                val = q + val + q;
            }
            c.xslt_params.put(key, val);
            return;
        }
        if (c.name.equals("tokenize") && name.equals("type") && value != null) { c.tokenize_type = value; return; }
        if ((c.name.equals("quote-c") || c.name.equals("unquote-c")) && name.equals("add-nl") && value == null) { c.add_nl = true; return; }
        if (c.name.equals("replace-entity") && value == null)
        {
            if (name.equals("with-file")) { c.with_file = 1; return; }
            if (name.equals("with-text-file")) { c.with_file = 2; return; }
        }
        throw fail("xmq: option \"" + optionString(name, value) + "\" not available for command \"" + c.name + "\"");
    }

    // ------------------------------------------------------------ performing

    void perform() throws Exit
    {
        // Like the C command line: add to-xmq when there is no to/render/tokenize
        // command and print when there is no output command.
        boolean has_to = false, has_output = false, do_print = true;
        for (Command c : commands)
        {
            if (c.name.equals("help")) { Main.printHelp(); return; }
            if (isTo(c.name)) { has_to = true; do_print = true; }
            if (c.name.equals("no-output")) do_print = false;
            if (c.name.equals("print") || c.name.equals("save-to") || c.name.equals("no-output")) has_output = true;
        }
        if (!has_to && do_print)
        {
            Command to = new Command();
            to.name = "to-xmq";
            to.use_color = env_use_color;
            to.bg_dark_mode = env_bg_dark_mode;
            to.bg_forced = env_bg_forced;
            to.truecolor = env_truecolor;
            to.theme_spec = System.getenv("XMQ_THEME");
            int at = commands.size();
            for (int i = 0; i < commands.size(); i++)
            {
                String n = commands.get(i).name;
                if (n.equals("print") || n.equals("save-to")) { at = i; break; }
            }
            commands.add(at, to);
        }
        if (!has_output && do_print)
        {
            Command print = new Command();
            print.name = "print";
            commands.add(print);
        }

        String content = readInput();
        IXMLAnalyzer ixml = null;
        if (ixml_source != null)
        {
            try
            {
                ixml = IXMLAnalyzer.compileParser(ixml_source);
            }
            catch (IllegalArgumentException e)
            {
                throw fail("xmq: the ixml grammar is not LL(*), xmqj can only use LL(*) grammars for now.\n" + e.getMessage());
            }
        }
        if (!lines)
        {
            performOn(content, ixml);
            if (ixml_failed) throw new Exit(1);
            return;
        }
        int start = 0;
        while (start < content.length())
        {
            int eol = content.indexOf('\n', start);
            int stop = eol < 0 ? content.length() : eol;
            performOn(content.substring(start, stop), ixml);
            start = stop + 1;
        }
        if (ixml_failed) throw new Exit(1);
    }

    String readInput() throws Exit
    {
        if (no_input) return null;
        if (in_is_content) return in;
        try
        {
            byte[] b = (in == null || in.equals("-")) ? System.in.readAllBytes() : Files.readAllBytes(Path.of(in));
            if (b.length > 2 && (((b[0] & 0xFF) == 0xFF && (b[1] & 0xFF) == 0xFE) || ((b[0] & 0xFF) == 0xFE && (b[1] & 0xFF) == 0xFF)))
            {
                // Like skip_any_potential_bom in the C version: UTF-16 with a bom cannot be handled.
                throw new Exit(1);
            }
            input_bytes = b;
            // Invalid UTF-8 becomes U+FFFD, the xml parser gets the bytes and reports it.
            return new String(b, StandardCharsets.UTF_8);
        }
        catch (IOException e)
        {
            throw fail("xmq: " + in + ": No such file or directory");
        }
    }

    void performOn(String content, IXMLAnalyzer ixml) throws Exit
    {
        String source = in == null || (!in_is_content && in.equals("-")) ? "stdin" : in_is_content ? "-i argument" : in;
        loaded_content = content;
        // Like C, tokenize works on the content and nothing is loaded before it.
        for (Command c : commands)
        {
            if (c.name.equals("tokenize"))
            {
                tokenize(content, source, c);
                return;
            }
        }
        XmlDocument doc = load(content, source, ixml);
        if (doc == null) throw new Exit(1);
        String output = null;
        for (Command c : commands)
        {
            switch (c.name)
            {
            case "delete" -> delete(doc, c.args.get(0));
            case "delete-entity" -> deleteEntity(doc, c.args.get(0));
            case "select" -> doc = select(doc, c.args.get(0));
            case "add-root" -> doc = addRoot(doc, c.args.get(0));
            case "add" -> add(doc, c.args.get(0));
            case "replace-entity" -> replaceEntity(doc, c);
            case "expand-entities" -> expandEntities(doc, content);
            case "quote-c", "unquote-c" -> doc = quote(doc, c);
            case "transform" -> doc = transform(doc, c, source);
            case "validate" -> { if (!validate(doc, c)) throw new Exit(1); }
            case "no-output" -> { }
            case "print" -> { if (output != null) System.out.print(output); }
            case "save-to" -> save(output, c.args.get(0));
            default -> {
                if (!isTo(c.name)) throw fail("xmq: the command " + c.name + " is not implemented in xmqj yet");
                if (c.name.equals("tokenize"))
                {
                    tokenize(content, source, c);
                    return;
                }
                output = to(doc, c);
            }
            }
        }
    }

    XmlDocument load(String content, String source, IXMLAnalyzer ixml) throws Exit
    {
        if (content == null) return new XmlDocument();
        if (ixml != null)
        {
            // A rejected input is output as a failure document, ixml(ixml:state=failed){info=...},
            // like in the C version, and xmq exits with 1.
            XmlDocument d = ixml.parseInputOrFailure(content, ixml_fail_silent);
            if (IXMLAnalyzer.isFailure(d)) ixml_failed = true;
            return d;
        }
        if ("text".equals(in_format))
        {
            XmlDocument d = new XmlDocument();
            d.appendRaw(new XmlText(content));
            return d;
        }
        try
        {
            String format = "json".equals(in_format) || "yaml".equals(in_format) ? in_format : null;
            // A file with a .yml or .yaml extension is yaml, like in C.
            if (in_format == null && in != null && !in_is_content)
            {
                String lower = in.toLowerCase(java.util.Locale.ROOT);
                if (lower.endsWith(".yml") || lower.endsWith(".yaml")) format = "yaml";
            }
            yaml_input = "yaml".equals(format);
            // The xml parser detects the encoding from the bytes, unless the input is split into lines.
            byte[] raw = !lines && input_bytes != null ? input_bytes : null;
            XmlDocument d = Main.load(content, raw, source, trim_none, no_merge, format);
            // Like C, an explicit --trim=heuristic trims any input, not only xml and html
            // (which Main.load already trims). Trimming twice changes nothing.
            if (d != null && trim_heuristic) XmlTrim.trimWhitespace(d);
            return d;
        }
        catch (ParseException e)
        {
            System.err.print(XMQ.printException(e));
            return null;
        }
    }

    // ------------------------------------------------------------ validate

    /** Like cli_xsd_put: a 4096 byte buffer (snprintf) and puts. */
    static void xsdPut(String message)
    {
        byte[] b = message.getBytes(StandardCharsets.UTF_8);
        int n = Math.min(b.length, 4095);
        System.out.write(b, 0, n);
        System.out.write('\n');
    }

    /** Like cli_xsd_parse_message: the schema cannot be parsed, print the first error and exit. */
    static void xsdParseMessage(Object ctx, String message, boolean is_warning)
    {
        if (is_warning)
        {
            System.err.print("xmq: " + ctx + " parse warning\n");
            xsdPut(message);
            return;
        }
        System.err.print("xmq: " + ctx + " parse error\n");
        xsdPut(message);
        throw new ExitNow(1);
    }

    /** Like cli_xsd_validate_message: the document is not valid, print the first error and exit. */
    static void xsdValidateMessage(Object ctx, String message, boolean is_warning)
    {
        if (is_warning)
        {
            System.out.print("xmq: validation warning\n");
            xsdPut(message);
            return;
        }
        System.out.print("xmq: Document cannot be validated against " + ctx + "\n");
        xsdPut(message);
        throw new ExitNow(1);
    }

    /** Like cli_xsd_silent_message: exit at the first error without a message. */
    static void xsdSilentMessage(Object ctx, String message, boolean is_warning)
    {
        if (!is_warning) throw new ExitNow(1);
    }

    /** Like the validate group of handle_option in the C version: the xsd is loaded and compiled here. */
    boolean validateArg(Command c, String arg, boolean is_option)
    {
        if (arg.startsWith("--silent"))
        {
            c.silent = true;
            return true;
        }
        if (!is_option && c.xsd == null)
        {
            String[] error = new String[1];
            XmlDocument doq = loadForXslt(arg, null, error);
            if (doq == null)
            {
                System.err.print((error[0] != null ? error[0] : "xmq: parse error") + "\n");
                return false;
            }
            c.xsd_name = arg;
            c.xsd = Validate.compile(doq, Cli::xsdParseMessage, arg);
            return true;
        }
        return false;
    }

    /** Like cmd_validate in the C version. */
    boolean validate(XmlDocument doc, Command c)
    {
        if (c.xsd == null)
        {
            System.err.print("xmq: Schema xsd not valid, cannot be used to validate document.\n");
            throw new ExitNow(1);
        }
        c.xsd.setMessageCallback(c.silent ? Cli::xsdSilentMessage : Cli::xsdValidateMessage, c.xsd_name);
        int status = c.xsd.validate(doc);
        if (status == Validate.ERROR_XSD_COMPILE)
        {
            System.err.print("xmq: Schema xsd not valid, cannot be used to validate document.\n");
            throw new ExitNow(1);
        }
        if (status != Validate.OK)
        {
            System.out.print("xmq: Document failed validation agains " + in + "\n");
            return false;
        }
        c.xsd = null;
        return true;
    }

    // ------------------------------------------------------------ xslt

    /** Load a document for xslt like xmqParseFileWithType(doc, path, NULL, XMQ_CONTENT_DETECT, XMQ_FLAG_TRIM_NONE). */
    static XmlDocument loadForXslt(String path, boolean[] is_xmq, String[] error)
    {
        byte[] b;
        try
        {
            b = Files.readAllBytes(Path.of(path));
        }
        catch (IOException | java.nio.file.InvalidPathException e)
        {
            // The error of the document (xmqDocError) in the C version.
            error[0] = "xmq: " + path + ": No such file or directory\n";
            return null;
        }
        String content = new String(b, StandardCharsets.UTF_8);
        if (is_xmq != null) is_xmq[0] = isXmqContent(content);
        try
        {
            XmlDocument d = Main.load(content, b, path, true, false, null);
            // Documents parsed by xmq have no url in the C version.
            if (d != null) d.setUrl(null);
            return d;
        }
        catch (ParseException e)
        {
            System.err.print(XMQ.printException(e));
            return null;
        }
    }

    /** Like load_xslt in the C version. */
    boolean loadXslt(Command c, String file) throws Exit
    {
        boolean[] xmq = new boolean[1];
        String[] error = new String[1];
        XmlDocument src = loadForXslt(file, xmq, error);
        if (src == null)
        {
            System.err.print((error[0] != null ? error[0] : "xmq: parse error") + "\n");
            return false;
        }
        if (xmq[0])
        {
            // We want to be able to use char entities in the xslq by default.
            // So we replace any explicit &#10; with actual newlines etc.
            XmlNode root = src.firstChild();
            if (root != null) XsltSupport.substituteCharEntities(src, root);
        }
        c.xslt = compileXslt(src);
        return c.xslt != null;
    }

    /** Like xmqCompileXslt(stylesheet, NULL) in the C version. */
    static org.libxmq.imp.xml.libxslt1.Stylesheet compileXslt(XmlDocument src)
    {
        // A copy of the stylesheet, the compiler changes it.
        XmlDocument dom = new XmlDocument();
        if (src.version() != null) dom.setVersion(src.version());
        if (src.encoding() != null) dom.setEncoding(src.encoding());
        if (src.url() != null) dom.setUrl(src.url());
        for (XmlNode n = src.firstChild(); n != null; n = n.next())
        {
            XmlNode copy = org.libxmq.imp.xml.XmlDomApi.nodeCopy(dom, n, true);
            org.libxmq.imp.xml.XmlDomApi.nodeAppendChild(dom, copy);
        }
        for (XmlNode n = dom.firstChild(); n != null; n = n.next())
        {
            if (n instanceof XmlElement) org.libxmq.imp.xml.XmlDomApi.nodeReconcileNs(n);
        }
        org.libxmq.imp.xml.libxslt1.Stylesheet[] style = new org.libxmq.imp.xml.libxslt1.Stylesheet[1];
        org.libxmq.imp.xml.libxslt1.Settings settings = new org.libxmq.imp.xml.libxslt1.Settings();
        settings.message_callback = m -> System.err.print(m);
        settings.loader = (uri, type) -> xsltLoader(style[0], uri, type);
        style[0] = org.libxmq.imp.xml.libxslt1.Xslt.compile(dom, settings);
        return style[0];
    }

    /** Like xmq_xslt_loader in the C version. */
    static XmlDocument xsltLoader(org.libxmq.imp.xml.libxslt1.Stylesheet style, String uri, int type)
    {
        // document('') of a stylesheet without url: the stylesheet itself.
        if (type == org.libxmq.imp.xml.libxslt1.Settings.LOAD_DOCUMENT && (uri == null || uri.isEmpty()) && style != null)
        {
            return org.libxmq.imp.xml.libxslt1.Transform.copyStylesheetDoc(style);
        }
        String path = uri;
        if (path.startsWith("file://"))
        {
            path = path.substring(7);
            if (path.startsWith("localhost/")) path = path.substring(9);
        }
        // Like libxml2, an uri reference with %XX escapes is a file name once unescaped.
        if (path.indexOf(':') < 0 && path.indexOf('%') >= 0 && !Files.isRegularFile(Path.of(path)))
        {
            String u = org.libxmq.imp.xml.XmlUri.unescapeString(path, -1);
            if (u != null) path = u;
        }
        String[] error = new String[1];
        XmlDocument d = loadForXslt(path, null, error);
        if (d == null)
        {
            if (error[0] != null) System.err.print(error[0]);
            return null;
        }
        d.setUrl(uri);
        return d;
    }

    /** Like cmd_transform in the C version. */
    XmlDocument transform(XmlDocument doc, Command c, String source)
    {
        String[] params = new String[2 * c.xslt_params.size()];
        int k = 0;
        for (var e : c.xslt_params.entrySet())
        {
            params[k++] = e.getKey();
            params[k++] = e.getValue();
        }
        // Documents parsed by xmq have no url in the C version, relative uris in document()
        // are resolved against the current directory.
        String url = doc.url();
        doc.setUrl(null);
        XmlDocument result = org.libxmq.imp.xml.libxslt1.Transform.applyStylesheet(c.xslt, doc, params.length == 0 ? null : params, null);
        doc.setUrl(url);
        if (result == null) result = new XmlDocument();
        // Replace the xmq marker elements, eg xmq:entity(name=nbsp), that the
        // transform may have generated.
        XsltSupport.replaceTransformMarkers(result, source);
        return result;
    }

    void save(String output, String file) throws Exit
    {
        try
        {
            Files.writeString(Path.of(file), output == null ? "" : output, StandardCharsets.UTF_8);
        }
        catch (IOException e)
        {
            throw fail("xmq: could not write " + file);
        }
    }

    static List<XmlNode> xpath(XmlDocument doc, String xpath) throws Exit
    {
        try
        {
            return XPath.compile(xpath).selectNodes(doc);
        }
        catch (XPathException e)
        {
            throw fail("xmq: bad xpath " + xpath + ": " + e.getMessage());
        }
    }

    static void unlink(XmlNode n)
    {
        if (n instanceof XmlAttr a)
        {
            if (a.parent() instanceof XmlElement e) e.removeAttribute(a);
        }
        else n.unlink();
    }

    // delete xpath: unlink in reverse order, ie deeper nodes first.
    void delete(XmlDocument doc, String xpath) throws Exit
    {
        List<XmlNode> nodes = xpath(doc, xpath);
        for (int i = nodes.size() - 1; i >= 0; i--) unlink(nodes.get(i));
    }

    void deleteEntity(XmlDocument doc, String entity)
    {
        List<XmlNode> refs = new ArrayList<>();
        collectEntities(doc, entity, refs);
        for (XmlNode n : refs) n.unlink();
    }

    static void collectEntities(XmlNode n, String entity, List<XmlNode> out)
    {
        if (n instanceof XmlEntityRef && entity.equals(n.name())) out.add(n);
        if (n instanceof XmlElement e)
        {
            for (XmlAttr a : e.attributes()) collectEntities(a, entity, out);
        }
        for (XmlNode c = n.firstChild(); c != null; c = c.next()) collectEntities(c, entity, out);
    }

    // select xpath: a new document with the selected nodes, like cmd_select in
    // xmq-cli.c, which adds them in reverse document order. An attribute
    // becomes an element with its name and value.
    XmlDocument select(XmlDocument doc, String xpath) throws Exit
    {
        XmlDocument out = new XmlDocument();
        Object result;
        try
        {
            result = XPath.compile(xpath).evaluate(doc, null);
        }
        catch (XPathException e)
        {
            throw fail("xmq: bad xpath " + xpath + ": " + e.getMessage());
        }
        if (result instanceof Double d)
        {
            out.appendRaw(new XmlText(String.format("%.0f", d)));
            return out;
        }
        if (result instanceof Boolean b)
        {
            out.appendRaw(new XmlText(b ? "1" : "0"));
            return out;
        }
        if (!(result instanceof List<?> list)) throw fail("xmq: unknown xpath select result for " + xpath);
        for (int i = list.size() - 1; i >= 0; i--)
        {
            XmlNode n = (XmlNode)list.get(i);
            if (n instanceof XmlAttr a)
            {
                XmlElement e = new XmlElement(a.name());
                e.addChild(new XmlText(a.textContent()));
                unlink(a);
                out.appendRaw(e);
            }
            else
            {
                n.unlink();
                out.appendRaw(n);
            }
        }
        return out;
    }

    // add-root name: a new document whose root element holds the old top nodes.
    XmlDocument addRoot(XmlDocument doc, String name)
    {
        XmlDocument out = new XmlDocument();
        XmlElement root = new XmlElement(name);
        out.setRootElement(root);
        for (XmlNode n : doc.children())
        {
            n.unlink();
            root.appendRaw(n);
        }
        return out;
    }

    // add xmq: parse the xmq and add its top nodes last to the document.
    void add(XmlDocument doc, String xmq) throws Exit
    {
        XmlDocument a;
        try
        {
            a = Main.load(xmq, "add", true, false);
        }
        catch (ParseException e)
        {
            System.err.print(XMQ.printException(e));
            throw new Exit(1);
        }
        if (a == null) throw new Exit(1);
        for (XmlNode n : a.children())
        {
            n.unlink();
            doc.appendRaw(n);
        }
    }

    /** Expand the entities declared in the doctype, like xmqExpandEntities. */
    void expandEntities(XmlDocument doc, String content) throws Exit
    {
        long size = content == null ? 0 : content.getBytes(java.nio.charset.StandardCharsets.UTF_8).length;
        try
        {
            org.libxmq.imp.xml.XmlEntityExpander.expand(doc, null, size, org.libxmq.imp.xml.XmlParser.NOCDATA);
        }
        catch (org.libxmq.imp.xml.XmlParseException e)
        {
            throw fail("xmq: " + e.error);
        }
    }

    void replaceEntity(XmlDocument doc, Command c) throws Exit
    {
        String entity = c.args.get(0);
        String arg = c.args.get(1);
        String text = arg;
        XmlNode tree = null;
        try
        {
            if (c.with_file == 2) text = Files.readString(Path.of(arg), StandardCharsets.UTF_8);
            if (c.with_file == 1)
            {
                XmlDocument d = Main.load(Files.readString(Path.of(arg), StandardCharsets.UTF_8), arg, true, false);
                if (d == null) throw new Exit(1);
                tree = d.firstChild();
                text = null;
            }
        }
        catch (IOException e)
        {
            throw fail("xmq: " + arg + ": No such file or directory");
        }
        catch (ParseException e)
        {
            System.err.print(XMQ.printException(e));
            throw new Exit(1);
        }
        List<XmlNode> refs = new ArrayList<>();
        collectEntities(doc, entity, refs);
        for (XmlNode r : refs)
        {
            XmlNode parent = r.parent();
            XmlNode replacement = text != null ? new XmlText(text) : copy(tree);
            r.addPrevSibling(replacement);
            r.unlink();
            mergeText(parent);
        }
    }

    // Merge adjacent text nodes, like xmlTextMerge after replacing an entity.
    static void mergeText(XmlNode parent)
    {
        if (parent == null) return;
        XmlNode i = parent.firstChild();
        while (i != null)
        {
            XmlNode next = i.next();
            if (next != null && i instanceof XmlText a && next instanceof XmlText b
                && a.type() == XmlNodeType.TEXT && b.type() == XmlNodeType.TEXT)
            {
                a.setContent(a.content() + b.content());
                b.unlink();
                continue;
            }
            i = next;
        }
    }

    // A deep copy of an element or text tree (the parts replace-entity --with-file needs).
    static XmlNode copy(XmlNode n)
    {
        if (n instanceof XmlText t) return new XmlText(t.content());
        if (n instanceof XmlEntityRef r) return new XmlEntityRef(r.name());
        if (n instanceof XmlElement e)
        {
            XmlElement c = new XmlElement(e.name());
            for (XmlAttr a : e.attributes()) c.setAttribute(a.name(), a.textContent());
            for (XmlNode k = e.firstChild(); k != null; k = k.next()) c.appendRaw(copy(k));
            return c;
        }
        return new XmlText(n.textContent());
    }

    // quote-c / unquote-c: replace the document with its text content quoted or unquoted.
    XmlDocument quote(XmlDocument doc, Command c)
    {
        StringBuilder text = new StringBuilder();
        for (XmlNode n = doc.firstChild(); n != null; n = n.next()) text.append(n.textContent());
        String value = c.name.equals("quote-c") ? quoteC(text.toString()) : unquoteC(text.toString());
        if (c.add_nl && c.name.equals("quote-c")) value += "\n";
        XmlDocument out = new XmlDocument();
        out.appendRaw(new XmlText(value));
        return out;
    }

    // Like xmq_quote_as_c with quotes in text.c.
    static String quoteC(String s)
    {
        return JsonPrinter.quote(s, true);
    }

    // Like xmq_unquote_as_c removing the quotes in text.c.
    static String unquoteC(String s)
    {
        int i = 0;
        while (i < s.length() && (s.charAt(i) == ' ' || s.charAt(i) == '\t' || s.charAt(i) == '\n' || s.charAt(i) == '\r')) i++;
        if (i >= s.length() || s.charAt(i) != '"') return "[Not a valid C escaped string]";
        i++;
        StringBuilder b = new StringBuilder();
        for (; i < s.length() && s.charAt(i) != '"'; i++)
        {
            char c = s.charAt(i);
            if (c != '\\')
            {
                b.append(c);
                continue;
            }
            if (++i >= s.length()) break;
            switch (s.charAt(i))
            {
            case '"' -> b.append('"');
            case 'n' -> b.append('\n');
            case 'a' -> b.append((char)7);
            case 'b' -> b.append('\b');
            case 't' -> b.append('\t');
            case 'v' -> b.append((char)11);
            case 'f' -> b.append('\f');
            case 'r' -> b.append('\r');
            default -> { }
            }
        }
        if (i >= s.length()) return "[Not a valid C escaped string]";
        return b.toString();
    }

    void tokenize(String content, String source, Command c) throws Exit
    {
        if (!isXmqContent(content))
        {
            // Like C xmqTokenizeFile and xmqTokenizeBuffer.
            System.err.println(in_is_content ? "xmq: you can only tokenize the xmq format" : "You can only tokenize xmq!");
            throw new Exit(1);
        }
        if (!lines && input_bytes != null)
        {
            // The xmq source must be valid UTF-8, like xmqTokenizeBuffer in the C version.
            ParseException bad = Main.invalidUtf8(input_bytes, source);
            if (bad != null)
            {
                System.err.print(tokenizeError(bad));
                throw new Exit(1);
            }
        }
        String type = c.tokenize_type;
        if (type == null || type.equals("terminal") || type.equals("html") || type.equals("tex"))
        {
            colorTokens(content, source, c);
            return;
        }
        XMQParseIntoTokens t = new XMQParseIntoTokens();
        if ("debugtokens".equals(c.tokenize_type)) t.debugTokens();
        else if ("debugcontent".equals(c.tokenize_type)) t.debugContent();
        // The debug tokens are printed while parsing. Like C, which buffers the
        // output, nothing is printed when the parse fails.
        java.io.PrintStream stdout = System.out;
        java.io.ByteArrayOutputStream buf = new java.io.ByteArrayOutputStream();
        System.setOut(new java.io.PrintStream(buf, true, StandardCharsets.UTF_8));
        try
        {
            t.parse(content, source);
        }
        catch (ParseException e)
        {
            System.setOut(stdout);
            // Like C cmd_tokenize the message is not followed by an empty line.
            System.err.print(tokenizeError(e));
            throw new Exit(1);
        }
        finally
        {
            System.setOut(stdout);
        }
        System.out.print(buf.toString(StandardCharsets.UTF_8));
        System.out.println("");
    }

    /** The parse error of a tokenize, like C cmd_tokenize without the empty line after it. */
    static String tokenizeError(ParseException e)
    {
        String m = XMQ.printException(e);
        return m.endsWith("\n\n") ? m.substring(0, m.length() - 1) : m;
    }

    /** Like C xmqDetectContentType: neither xml/html (starts with <) nor json. */
    static boolean isXmqContent(String content)
    {
        if (content.startsWith("\uFEFF")) content = content.substring(1);
        int i = 0;
        while (i < content.length() && " \t\n\r".indexOf(content.charAt(i)) >= 0) i++;
        if (i < content.length() && content.charAt(i) == '<') return false;
        return !Main.is_json(content, i);
    }

    /**
     * Mirrors the C cmd_tokenize for no --type and --type=terminal/html/tex:
     * every token is printed wrapped in the theme strings of its color.
     * Like C, --type=terminal colors even when the output is not a terminal,
     * and the XMQ_THEME is only used when the output is a terminal.
     */
    void colorTokens(String content, String source, Command c) throws Exit
    {
        XMQPrintState.RenderTo to = XMQPrintState.RenderTo.TERMINAL;
        if ("html".equals(c.tokenize_type)) to = XMQPrintState.RenderTo.HTML;
        if ("tex".equals(c.tokenize_type)) to = XMQPrintState.RenderTo.TEX;
        if ("terminal".equals(c.tokenize_type)) c.use_color = true;
        c.theme_spec = env_use_color ? System.getenv("XMQ_THEME") : null;
        XMQPrintState ps = new XMQPrintState();
        setupRender(ps, c, to);
        XMQParseIntoColoredTokens t = new XMQParseIntoColoredTokens(ps.theme, to);
        XMQThemeStrings content_strings = ps.theme.content;
        if (content_strings != null && content_strings.pre() != null) t.out.append(content_strings.pre());
        try
        {
            t.parse(content, source);
        }
        catch (ParseException e)
        {
            // Like C cmd_tokenize the message is not followed by an empty line.
            System.err.print(tokenizeError(e));
            throw new Exit(1);
        }
        if (content_strings != null && content_strings.post() != null) t.out.append(content_strings.post());
        if (t.out.isEmpty())
        {
            // Like C, where an empty tokenize output is no output at all.
            System.err.println("xmq: no output found, please add a to/render/tokenize command");
            throw new Exit(1);
        }
        System.out.print(t.out);
        System.out.flush();
    }

    // The output of a to/render command.
    String to(XmlDocument doc, Command c)
    {
        OutputSettings os = new OutputSettings()
            .setIndentAmount(c.indent)
            .setCompact(c.compact)
            .setEscapeTabs(c.escape_tabs)
            // Compact mode always escapes newlines (see C xmq.c).
            .setEscapeNewlines(c.escape_newlines || c.compact)
            .setEscapeNon7bit(c.escape_non7bit)
            .setPreferDoubleQuotes(c.prefer_double_quotes)
            .setHtmq(c.name.equals("to-htmq"));
        // A yaml input that does not end with a newline is written without a final newline, like C.
        boolean no_final_nl = c.no_final_nl
            || (yaml_input && loaded_content != null && !loaded_content.isEmpty() && !loaded_content.endsWith("\n"));
        String nl = no_final_nl ? "" : "\n";
        switch (c.name)
        {
        case "to-html" -> {
            // Like C xmq_print_html, fixup the dom and then use the html serializer.
            XmlTrim.fixupHtmlBeforeWriteout(doc);
            return HtmlSave.docContentDump(doc, true);
        }
        case "to-xml" -> {
            XmlTrim.fixupHtmlBeforeWriteout(doc);
            String x = XmlSave.docDump(doc, "utf-8");
            if (c.omit_decl && x.startsWith("<?xml"))
            {
                int eol = x.indexOf('\n');
                x = eol < 0 ? "" : x.substring(eol + 1);
            }
            return x;
        }
        case "to-clines" -> {
            return Clines.print(doc);
        }
        case "to-json" -> {
            return new JsonPrinter().toJson(doc, os, !no_final_nl);
        }
        case "to-yaml" -> {
            String yaml = YamlPrinter.print(doc, c.indent, !no_final_nl);
            // Like C: the reminder first, unless --we-should-stop-using-yaml or it is already there.
            String note = "# we should stop using yaml\n";
            if (!we_should_stop_using_yaml && !yaml.startsWith(note)) yaml = note + yaml;
            return yaml;
        }
        case "to-text" -> {
            StringBuilder b = new StringBuilder();
            for (XmlNode n = doc.firstChild(); n != null; n = n.next()) text(b, n);
            return b.toString();
        }
        case "render-html", "render-terminal", "render-tex" -> {
            XMQPrintState ps = new XMQPrintState();
            ps.output_settings = os;
            setupRender(ps, c, null);
            if (c.only_style)
            {
                // Like C xmqSetupDefaultColors, print the style and exit.
                XMQThemeStrings st = ps.theme.style;
                System.out.println(st != null && st.pre() != null ? st.pre() : "(null)");
                System.out.flush();
                System.exit(0);
            }
            new XMQPrinter().print_rendered(ps, doc);
            return ps.buffer + nl;
        }
        default -> {
            XMQPrintState ps = new XMQPrintState();
            ps.output_settings = os;
            if (c.use_color)
            {
                // Like C, xmq printed to a terminal is colored with the theme.
                setupRender(ps, c, XMQPrintState.RenderTo.TERMINAL);
                new XMQPrinter().print_rendered(ps, doc);
                return ps.buffer + nl;
            }
            ps.defaultTheme();
            new XMQPrinter().print_node(ps, doc, 0);
            return ps.buffer + nl;
        }
        }
    }

    // The coloring found in the environment, like C lookup_bg and detect_truecolor,
    // turned off when the output is not a terminal.
    boolean env_use_color, env_bg_dark_mode, env_bg_forced, env_truecolor;
    {
        lookupBg();
        env_truecolor = detectTruecolor();
        if (!isTerminal()) env_use_color = false;
    }

    /** True if the output is a terminal, like C isatty(1). */
    static boolean isTerminal()
    {
        java.io.Console con = System.console();
        if (con == null) return false;
        try
        {
            // Console.isTerminal exists from Java 22, where a console is also returned for redirected output.
            return (Boolean)java.io.Console.class.getMethod("isTerminal").invoke(con);
        }
        catch (ReflectiveOperationException e)
        {
            return true;
        }
    }

    static boolean contains(String s, String p) { return s != null && s.contains(p); }

    /** Mirrors C lookup_bg. An xterm is not queried, a dark background is assumed. */
    void lookupBg()
    {
        String term = System.getenv("TERM");
        if (term == null) term = "NULL";
        env_bg_forced = false;
        String bg = System.getenv("XMQ_BG");
        if (bg != null)
        {
            if (bg.equals("mono")) { env_use_color = false; env_bg_dark_mode = false; return; }
            env_use_color = true;
            if (bg.equals("light")) { env_bg_dark_mode = false; env_bg_forced = true; return; }
            if (bg.equals("dark")) { env_bg_dark_mode = true; env_bg_forced = true; return; }
            env_use_color = false;
            env_bg_dark_mode = false;
            return;
        }
        env_use_color = true;
        env_bg_dark_mode = true;
        if (term.equals("linux") || term.equals("xterm-256color") || term.equals("xterm")) return;
        String colorfgbg = System.getenv("COLORFGBG");
        if (colorfgbg != null)
        {
            // If the string ends with ;0 then it is dark mode.
            int p = colorfgbg.lastIndexOf(';');
            env_bg_dark_mode = p < 0 || colorfgbg.substring(p + 1).equals("0");
        }
    }

    /** Mirrors C detect_truecolor. */
    static boolean detectTruecolor()
    {
        String override = System.getenv("XMQ_TERM");
        if (override != null) return override.equals("truecolor");
        String colorterm = System.getenv("COLORTERM");
        if (contains(colorterm, "truecolor") || contains(colorterm, "24bit")) return true;
        String term = System.getenv("TERM");
        String term_prog = System.getenv("TERM_PROGRAM");
        if (System.getenv("WT_SESSION") != null) return true;
        if ("iTerm.app".equals(term_prog)) return true;
        if ("Apple_Terminal".equals(term_prog)) return false;
        if (contains(term, "xterm-truecolor") || contains(term, "tmux-truecolor") || contains(term, "screen-truecolor")) return true;
        if (System.getenv("KONSOLE_PROFILE_NAME") != null) return true;
        return contains(term, "xterm") || contains(term, "screen") || contains(term, "tmux")
            || contains(term, "rxvt") || contains(term, "alacritty") || contains(term, "kitty");
    }

    /**
     * Mirrors C compute_color_mode and xmqSetupDefaultColors for the render
     * commands, and for to-xmq/to-htmq printed to a terminal: installs the
     * theme, the coloring of the render format and the --use-is=... overrides.
     * The render format is given by the command name when to is null.
     */
    static void setupRender(XMQPrintState ps, Command c, XMQPrintState.RenderTo to)
    {
        if (to == null) to = switch (c.name)
        {
            case "render-html" -> XMQPrintState.RenderTo.HTML;
            case "render-tex" -> XMQPrintState.RenderTo.TEX;
            default -> XMQPrintState.RenderTo.TERMINAL;
        };
        // compute_color_mode: 0 mono, 1 dark, 2 light, 3 auto (html follows the browser).
        int mode;
        if (c.bg_forced) mode = c.bg_dark_mode ? 1 : 2;
        else if (!c.use_color) mode = 0;
        else if (to == XMQPrintState.RenderTo.HTML) mode = 3;
        else mode = c.bg_dark_mode ? 1 : 2;

        boolean use_color = mode != 0;
        boolean dark_mode = mode == 1 || mode == 3;
        String spec = c.theme_spec;
        if (spec == null)
        {
            if (to == XMQPrintState.RenderTo.TEX) dark_mode = false;
            spec = dark_mode ? "dark" : "light";
        }
        else
        {
            if (spec.endsWith("-dark")) dark_mode = true;
            if (spec.endsWith("-light")) dark_mode = false;
        }

        XMQTheme theme = new XMQTheme();
        theme.installDefault();
        theme.installTheme(spec);
        ps.theme = theme;
        ps.render_to = to;
        ps.indentation_space = theme.indentation_space;
        ps.explicit_space = theme.explicit_space;
        ps.explicit_nl = theme.explicit_nl;
        ps.explicit_tab = theme.explicit_tab;
        ps.explicit_cr = theme.explicit_cr;

        switch (to)
        {
        // Like C, where the detected truecolor is not passed on to the output
        // settings, the 256 color palette is used.
        case TERMINAL -> theme.setupTerminalColoring(dark_mode, use_color, false);
        case HTML -> {
            ps.indentation_space = " ";
            ps.explicit_nl = "\n";
            theme.setupHtmlColoring(c.render_raw, mode == 1 ? 1 : mode == 2 ? 2 : 0, c.use_id, c.use_class);
        }
        case TEX -> {
            ps.indentation_space = "\\xmqI ";
            ps.explicit_space = " ";
            ps.explicit_nl = "\\linebreak\n";
            theme.setupTexColoring(dark_mode, c.render_raw);
        }
        default -> { }
        }

        // Like C xmqOverrideSettings.
        if (c.use_is != null) ps.indentation_space = c.use_is;
        if (c.use_es != null) ps.explicit_space = c.use_es;
        if (c.use_et != null) ps.explicit_tab = c.use_et;
        if (c.use_cr != null) ps.explicit_cr = c.use_cr;
        if (c.use_nl != null) ps.explicit_nl = c.use_nl;
    }

    // Like text_print_node in xmq.c.
    static void text(StringBuilder b, XmlNode n)
    {
        if (n instanceof XmlEntityRef) b.append("<ENTITY>").append(n.name());
        else if (n instanceof XmlElement) for (XmlNode c = n.firstChild(); c != null; c = c.next()) text(b, c);
        else if (n.type() == XmlNodeType.TEXT || n.type() == XmlNodeType.CDATA) b.append(n.content());
    }
}
