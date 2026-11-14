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
        boolean compact, escapeTabs, escapeNewlines, escapeNon7bit, preferDoubleQuotes, noFinalNl;
        boolean omitDecl, renderRaw, addNl;
        int indent = 4;
        // The coloring, like the C XMQCliCommand use_color, bg_dark_mode, bg_forced
        // and truecolor. Copied from the environment, then changed by --bg=.
        boolean useColor, bgDarkMode, bgForced, truecolor;
        String themeSpec;        // --theme= or XMQ_THEME
        String useId, useClass;  // --id= --class= of render-html
        boolean onlyStyle;       // --onlystyle
        // --use-is= --use-es= --use-et= --use-cr= --use-nl= of the render commands.
        String useIs, useEs, useEt, useCr, useNl;
        String tokenizeType;
        int withFile; // replace-entity --with-file (1) or --with-text-file (2)
    }

    // Global settings.
    boolean trimNone, trimHeuristic, noMerge, lines, noInput, inIsContent;
    String inFormat;        // null = detect, else xmq xml html text
    String in;              // file name, "-" for stdin, or the content with -i
    String ixmlSource;      // --ixml=grammar.ixml or a positional grammar
    final List<Command> commands = new ArrayList<>();
    boolean yamlInput;      // The input was loaded as yaml.
    String loadedContent;   // The content of the current document.

    static final String[] NOT_IMPLEMENTED = {
        "transform", "validate", "for-each", "statistics",
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
            case "input" -> { inIsContent = true; in = e.textContent(); }
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
        case "no-input" -> noInput = true;
        case "trim" -> {
            if ("none".equals(value)) trimNone = true;
            else if ("heuristic".equals(value)) { trimNone = false; trimHeuristic = true; }
            else throw fail("No such trim rule \"" + value + "\"!");
        }
        case "no-merge", "nomerge" -> noMerge = true;
        case "lines" -> lines = true;
        case "xmq", "htmq" -> inFormat = "xmq";
        case "xml" -> inFormat = "xml";
        case "html" -> inFormat = "html";
        case "text" -> inFormat = "text";
        case "ixml" -> {
            if (value == null) throw fail("xmq: --ixml as an input format is not implemented in xmqj yet");
            grammar(value);
        }
        case "json" -> inFormat = "json";
        case "clines" -> inFormat = "text"; // Like C, clines input is loaded as text.
        case "yaml" -> inFormat = "yaml";
        case "root", "ixml-engine", "list-library", "xml-of-ixml" ->
            throw fail("xmq: the global option " + optionString(name, value) + " is not implemented in xmqj yet");
        // Logging and things that do not change the result.
        case "debug", "verbose", "trace", "log-xmq", "progress", "yes", "force-download",
             "ixml-all-parses", "ixml-try-to-recover", "ixml-fail-silent" -> { }
        case "version" -> { System.out.println("xmqj"); throw new Exit(0); }
        default -> throw fail("xmq: unrecognized global option: '" + optionString(name, value)
                              + "'\nTry 'xmq --help' for more information");
        }
    }

    void grammar(String g) throws Exit
    {
        if (ixmlSource != null) throw fail("You have already specified an ixml grammar!");
        if (g.startsWith("ixml:")) throw fail("xmq: library grammars (" + g + ") are not implemented in xmqj yet");
        try
        {
            ixmlSource = Files.readString(Path.of(g), StandardCharsets.UTF_8);
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
        c.useColor = envUseColor;
        c.bgDarkMode = envBgDarkMode;
        c.bgForced = envBgForced;
        c.truecolor = envTruecolor;
        c.themeSpec = System.getenv("XMQ_THEME");
        for (String n : NOT_IMPLEMENTED)
        {
            if (n.equals(c.name)) throw fail("xmq: the command " + c.name + " is not implemented in xmqj yet");
        }
        if (c.name.equals("to-json")) { c.compact = true; c.indent = 0; }
        if (c.name.equals("to-yaml")) c.indent = 2; // The default yaml output indentation.
        for (XmlElement p : e.elements())
        {
            if (p.name().equals("option")) commandOption(c, p.attributeValue("name"), value(p));
            else c.args.add(p.textContent());
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
            case "no-final-nl" -> { c.noFinalNl = true; return; }
            default -> { }
            }
        }
        if ((c.name.equals("to-xmq") || c.name.equals("to-htmq") || render) && value == null)
        {
            switch (name)
            {
            case "escape-tabs" -> { c.escapeTabs = true; return; }
            case "escape-newlines" -> { c.escapeNewlines = true; return; }
            case "escape-non-7bit" -> { c.escapeNon7bit = true; return; }
            case "prefer-double-quotes" -> { c.preferDoubleQuotes = true; return; }
            default -> { }
            }
        }
        if ((c.name.equals("to-xmq") || c.name.equals("to-htmq") || render) && name.equals("indent") && value != null)
        {
            if (!value.matches("[0-9]+")) throw fail("xmq: indent must be a positive integer");
            c.indent = Integer.parseInt(value);
            return;
        }
        if (c.name.equals("to-xml") && name.equals("omit-decl") && value == null) { c.omitDecl = true; return; }
        if (render)
        {
            if (name.equals("nostyle") && value == null) { c.renderRaw = true; return; }
            if (name.equals("onlystyle") && value == null) { c.onlyStyle = true; return; }
            if (value != null)
            {
                switch (name)
                {
                case "theme" -> { c.themeSpec = value; return; }
                case "class" -> { c.useClass = value; return; }
                case "id" -> { c.useId = value; return; }
                case "use-is" -> { c.useIs = value; return; }
                case "use-es" -> { c.useEs = value; return; }
                case "use-et" -> { c.useEt = value; return; }
                case "use-cr" -> { c.useCr = value; return; }
                case "use-nl" -> { c.useNl = value; return; }
                default -> { }
                }
            }
            if (name.equals("bg") && value != null)
            {
                switch (value)
                {
                case "light" -> { c.useColor = true; c.bgDarkMode = false; c.bgForced = true; }
                case "dark" -> { c.useColor = true; c.bgDarkMode = true; c.bgForced = true; }
                case "mono" -> { c.useColor = false; c.bgDarkMode = false; }
                default -> throw fail("xmq: unknown bg setting \"" + value + "\"");
                }
                return;
            }
        }
        if (c.name.equals("tokenize") && name.equals("type") && value != null) { c.tokenizeType = value; return; }
        if ((c.name.equals("quote-c") || c.name.equals("unquote-c")) && name.equals("add-nl") && value == null) { c.addNl = true; return; }
        if (c.name.equals("replace-entity") && value == null)
        {
            if (name.equals("with-file")) { c.withFile = 1; return; }
            if (name.equals("with-text-file")) { c.withFile = 2; return; }
        }
        throw fail("xmq: option \"" + optionString(name, value) + "\" not available for command \"" + c.name + "\"");
    }

    // ------------------------------------------------------------ performing

    void perform() throws Exit
    {
        // Like the C command line: add to-xmq when there is no to/render/tokenize
        // command and print when there is no output command.
        boolean hasTo = false, hasOutput = false, doPrint = true;
        for (Command c : commands)
        {
            if (c.name.equals("help")) { Main.printHelp(); return; }
            if (isTo(c.name)) { hasTo = true; doPrint = true; }
            if (c.name.equals("no-output")) doPrint = false;
            if (c.name.equals("print") || c.name.equals("save-to") || c.name.equals("no-output")) hasOutput = true;
        }
        if (!hasTo && doPrint)
        {
            Command to = new Command();
            to.name = "to-xmq";
            to.useColor = envUseColor;
            to.bgDarkMode = envBgDarkMode;
            to.bgForced = envBgForced;
            to.truecolor = envTruecolor;
            to.themeSpec = System.getenv("XMQ_THEME");
            int at = commands.size();
            for (int i = 0; i < commands.size(); i++)
            {
                String n = commands.get(i).name;
                if (n.equals("print") || n.equals("save-to")) { at = i; break; }
            }
            commands.add(at, to);
        }
        if (!hasOutput && doPrint)
        {
            Command print = new Command();
            print.name = "print";
            commands.add(print);
        }

        String content = readInput();
        IXMLAnalyzer ixml = null;
        if (ixmlSource != null)
        {
            try
            {
                ixml = IXMLAnalyzer.compileParser(ixmlSource);
            }
            catch (IllegalArgumentException e)
            {
                throw fail("xmq: the ixml grammar is not LL(*), xmqj can only use LL(*) grammars for now.\n" + e.getMessage());
            }
        }
        if (!lines)
        {
            performOn(content, ixml);
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
    }

    String readInput() throws Exit
    {
        if (noInput) return null;
        if (inIsContent) return in;
        try
        {
            if (in == null || in.equals("-"))
            {
                return new String(System.in.readAllBytes(), StandardCharsets.UTF_8);
            }
            return Files.readString(Path.of(in), StandardCharsets.UTF_8);
        }
        catch (IOException e)
        {
            throw fail("xmq: " + in + ": No such file or directory");
        }
    }

    void performOn(String content, IXMLAnalyzer ixml) throws Exit
    {
        String source = in == null || (!inIsContent && in.equals("-")) ? "stdin" : inIsContent ? "-i argument" : in;
        loadedContent = content;
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
            case "quote-c", "unquote-c" -> doc = quote(doc, c);
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
            try
            {
                return ixml.parseInput(content);
            }
            catch (IllegalArgumentException e)
            {
                throw fail("xmq: " + source + ": ixml parse failed: " + e.getMessage());
            }
        }
        if ("text".equals(inFormat))
        {
            XmlDocument d = new XmlDocument();
            d.appendRaw(new XmlText(content));
            return d;
        }
        try
        {
            String format = "json".equals(inFormat) || "yaml".equals(inFormat) ? inFormat : null;
            // A file with a .yml or .yaml extension is yaml, like in C.
            if (inFormat == null && in != null && !inIsContent)
            {
                String lower = in.toLowerCase(java.util.Locale.ROOT);
                if (lower.endsWith(".yml") || lower.endsWith(".yaml")) format = "yaml";
            }
            yamlInput = "yaml".equals(format);
            XmlDocument d = Main.load(content, source, trimNone, noMerge, format);
            // Like C, an explicit --trim=heuristic trims any input, not only xml and html
            // (which Main.load already trims). Trimming twice changes nothing.
            if (d != null && trimHeuristic) XmlTrim.trimWhitespace(d);
            return d;
        }
        catch (ParseException e)
        {
            System.err.print(XMQ.printException(e));
            return null;
        }
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

    void replaceEntity(XmlDocument doc, Command c) throws Exit
    {
        String entity = c.args.get(0);
        String arg = c.args.get(1);
        String text = arg;
        XmlNode tree = null;
        try
        {
            if (c.withFile == 2) text = Files.readString(Path.of(arg), StandardCharsets.UTF_8);
            if (c.withFile == 1)
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
        if (c.addNl && c.name.equals("quote-c")) value += "\n";
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
        XMQParseIntoTokens t = new XMQParseIntoTokens();
        if ("debugtokens".equals(c.tokenizeType)) t.debugTokens();
        else if ("debugcontent".equals(c.tokenizeType)) t.debugContent();
        try
        {
            t.parse(content, source);
        }
        catch (ParseException e)
        {
            System.err.print(XMQ.printException(e));
            throw new Exit(1);
        }
        System.out.println("");
    }

    // The output of a to/render command.
    String to(XmlDocument doc, Command c)
    {
        OutputSettings os = new OutputSettings()
            .setIndentAmount(c.indent)
            .setCompact(c.compact)
            .setEscapeTabs(c.escapeTabs)
            // Compact mode always escapes newlines (see C xmq.c).
            .setEscapeNewlines(c.escapeNewlines || c.compact)
            .setEscapeNon7bit(c.escapeNon7bit)
            .setPreferDoubleQuotes(c.preferDoubleQuotes)
            .setHtmq(c.name.equals("to-htmq"));
        // A yaml input that does not end with a newline is written without a final newline, like C.
        boolean noFinalNl = c.noFinalNl
            || (yamlInput && loadedContent != null && !loadedContent.isEmpty() && !loadedContent.endsWith("\n"));
        String nl = noFinalNl ? "" : "\n";
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
            if (c.omitDecl && x.startsWith("<?xml"))
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
            return new JsonPrinter().toJson(doc, os, !noFinalNl);
        }
        case "to-yaml" -> {
            return YamlPrinter.print(doc, c.indent, !noFinalNl);
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
            if (c.onlyStyle)
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
            if (c.useColor)
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
    boolean envUseColor, envBgDarkMode, envBgForced, envTruecolor;
    {
        lookupBg();
        envTruecolor = detectTruecolor();
        if (!isTerminal()) envUseColor = false;
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
        envBgForced = false;
        String bg = System.getenv("XMQ_BG");
        if (bg != null)
        {
            if (bg.equals("mono")) { envUseColor = false; envBgDarkMode = false; return; }
            envUseColor = true;
            if (bg.equals("light")) { envBgDarkMode = false; envBgForced = true; return; }
            if (bg.equals("dark")) { envBgDarkMode = true; envBgForced = true; return; }
            envUseColor = false;
            envBgDarkMode = false;
            return;
        }
        envUseColor = true;
        envBgDarkMode = true;
        if (term.equals("linux") || term.equals("xterm-256color") || term.equals("xterm")) return;
        String colorfgbg = System.getenv("COLORFGBG");
        if (colorfgbg != null)
        {
            // If the string ends with ;0 then it is dark mode.
            int p = colorfgbg.lastIndexOf(';');
            envBgDarkMode = p < 0 || colorfgbg.substring(p + 1).equals("0");
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
        if (c.bgForced) mode = c.bgDarkMode ? 1 : 2;
        else if (!c.useColor) mode = 0;
        else if (to == XMQPrintState.RenderTo.HTML) mode = 3;
        else mode = c.bgDarkMode ? 1 : 2;

        boolean use_color = mode != 0;
        boolean dark_mode = mode == 1 || mode == 3;
        String spec = c.themeSpec;
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
            theme.setupHtmlColoring(c.renderRaw, mode == 1 ? 1 : mode == 2 ? 2 : 0, c.useId, c.useClass);
        }
        case TEX -> {
            ps.indentation_space = "\\xmqI ";
            ps.explicit_space = " ";
            ps.explicit_nl = "\\linebreak\n";
            theme.setupTexColoring(dark_mode, c.renderRaw);
        }
        default -> { }
        }

        // Like C xmqOverrideSettings.
        if (c.useIs != null) ps.indentation_space = c.useIs;
        if (c.useEs != null) ps.explicit_space = c.useEs;
        if (c.useEt != null) ps.explicit_tab = c.useEt;
        if (c.useCr != null) ps.explicit_cr = c.useCr;
        if (c.useNl != null) ps.explicit_nl = c.useNl;
    }

    // Like text_print_node in xmq.c.
    static void text(StringBuilder b, XmlNode n)
    {
        if (n instanceof XmlEntityRef) b.append("<ENTITY>").append(n.name());
        else if (n instanceof XmlElement) for (XmlNode c = n.firstChild(); c != null; c = c.next()) text(b, c);
        else if (n.type() == XmlNodeType.TEXT || n.type() == XmlNodeType.CDATA) b.append(n.content());
    }
}
