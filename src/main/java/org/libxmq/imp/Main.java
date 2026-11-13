/* libxmq - Copyright (C) 2025-2026 Fredrik Öhrström (spdx: MIT)

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

package org.libxmq.imp;

import java.nio.file.Files;
import java.nio.charset.StandardCharsets;
import java.nio.file.Path;
import java.nio.file.Paths;

import java.io.InputStream;
import java.io.OutputStream;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.Future;
import org.eclipse.lsp4j.jsonrpc.Launcher;
import org.eclipse.lsp4j.launch.LSPLauncher;
import org.eclipse.lsp4j.services.LanguageClient;

import org.libxmq.imp.xml.HtmlParser;
import org.libxmq.imp.xml.HtmlSave;
import org.libxmq.imp.xml.XmlDocument;
import org.libxmq.imp.xml.XmlElement;
import org.libxmq.imp.xml.XmlParseException;
import org.libxmq.imp.xml.XmlParser;
import org.libxmq.imp.xml.XmlSave;
import org.libxmq.imp.xml.XmlTrim;

import org.libxmq.ParseException;
import org.libxmq.OutputSettings;
import org.libxmq.XMQ;

/**
 * The main entry point of the xmq command line tool.
 */
public class Main
{
    /**
     * Creates the Main object.
     */
    public Main()
    {
    }

    /**
     * Command line entry point.
     * @param args The command line arguments.
     * @throws Exception If parsing or output fails.
     */
    public static void main(String[] args) throws Exception
    {
        // C writes raw UTF-8 bytes to stdout. The JVM console output defaults to
        // the native locale charset (ASCII under a C/POSIX locale), which would
        // replace non-ASCII characters in values with '?'. Force UTF-8 so the
        // rendered output matches the C implementation byte-for-byte.
        System.setOut(new java.io.PrintStream(
            new java.io.FileOutputStream(java.io.FileDescriptor.out), true, StandardCharsets.UTF_8));

        if (args.length == 0)
        {
            printHelp();
            return;
        }

        if (args[0].equals("generate-parser"))
        {
            int status = runGenerateParser(args);
            if (status != 0) System.exit(status);
            return;
        }

        if (args[0].equals("analyze"))
        {
            int status = runAnalyze(args);
            if (status != 0) System.exit(status);
            return;
        }

        if (args.length == 1 && args[0].equals("lsp"))
        {
            startServer(System.in, System.out);
            return;
        }

        try
        {
            // Collect options (in any position), the input file and the commands.
            boolean compact = false, escTabs = false, escNl = false, esc7 = false,
                   noFinalNl = false, trimNone = false, prefDQ = false, noMerge = false;
            String file = null;
            java.util.List<String> cmds = new java.util.ArrayList<>();
            for (String a : args)
            {
                if (a.equals("--compact")) compact = true;
                else if (a.equals("--escape-tabs")) escTabs = true;
                else if (a.equals("--escape-newlines")) escNl = true;
                else if (a.equals("--escape-non-7bit")) esc7 = true;
                else if (a.equals("--prefer-double-quotes")) prefDQ = true;
                else if (a.equals("--no-final-nl")) noFinalNl = true;
                else if (a.equals("--trim=none")) trimNone = true;
                else if (a.equals("--nomerge") || a.equals("--no-merge")) noMerge = true;
                else if (a.startsWith("--"))
                {
                    // Ignore options we do not act on (--no-merge, --lines,
                    // --text, --ixml=..., --nostyle, --theme=..., --type=... etc).
                }
                else if (file == null) file = a;
                else cmds.add(a);
            }
            if (file == null)
            {
                printHelp();
                return;
            }
            Path p = Paths.get(file);
            String content = Files.readString(p, StandardCharsets.UTF_8);

            // --trim=none is honored by load() below.
            // Compact mode always escapes newlines (see C xmq.c).
            if (compact) escNl = true;

            OutputSettings os = new OutputSettings()
                .setIndentAmount(4)
                .setCompact(compact)
                .setEscapeTabs(escTabs)
                .setEscapeNewlines(escNl)
                .setEscapeNon7bit(esc7)
                .setPreferDoubleQuotes(prefDQ)
                .setHtmq(cmds.contains("to-htmq"));

            boolean has_cmd = cmds.contains("to-xml");
            boolean has_cmd_html = cmds.contains("to-html");
            boolean is_tokenize = cmds.contains("tokenize");
            boolean render_html = cmds.contains("render-html");
            boolean render_terminal = cmds.contains("render-terminal");
            boolean render_tex = cmds.contains("render-tex");
            if (is_tokenize)
            {
                XMQParseIntoTokens to = new XMQParseIntoTokens();
                for (String a : args)
                {
                    if (a.equals("--type=debugtokens")) to.debugTokens();
                    else if (a.equals("--type=debugcontent")) to.debugContent();
                }
                to.parse(content, file);
                System.out.println("");
                return;
            }

            XmlDocument doc = load(content, file, trimNone, noMerge);
            if (doc == null)
            {
                System.exit(1);
                return;
            }

            if (render_html || render_terminal || render_tex)
            {
                boolean render_raw = false;
                // 0 = follow browser preference (xmq_auto), 1 = dark, 2 = light
                int bg_mode = 0;
                for (String a : args)
                {
                    if (a.equals("--nostyle")) render_raw = true;
                    else if (a.equals("--bg=dark")) bg_mode = 1;
                    else if (a.equals("--bg=light")) bg_mode = 2;
                }

                XMQTheme theme = new XMQTheme();
                theme.installDefault();
                if (render_html)
                {
                    theme.setupHtmlColoring(render_raw, bg_mode);
                }
                else
                {
                    // render-terminal / render-tex: wrap just the content when
                    // raw, otherwise use a full document scaffold (not fully
                    // ported yet; html is the fully supported render).
                    theme.content = new org.libxmq.imp.XMQThemeStrings(render_raw ? "" : "",
                        render_raw ? "" : "");
                }

                XMQPrintState ps = new XMQPrintState();
                ps.output_settings = os;
                ps.theme = theme;
                XMQPrinter pr = new XMQPrinter();
                pr.print_rendered(ps, doc);
                System.out.print(ps.buffer);
                if (!noFinalNl) System.out.println();
            }
            else if (has_cmd_html)
            {
                // to-html: like C xmq_print_html, fixup the dom and then emit
                // the document using the libxml2 html serializer.
                XmlTrim.fixupHtmlBeforeWriteout(doc);
                System.out.print(HtmlSave.docContentDump(doc, true));
            }
            else if (has_cmd)
            {
                // to-xml: like C xmq_print_xml, fixup the dom and then emit
                // the document using the libxml2 xml serializer.
                XmlTrim.fixupHtmlBeforeWriteout(doc);
                System.out.print(XmlSave.docDump(doc, "utf-8"));
            }
            else
            {
                XMQPrintState ps = new XMQPrintState();
                ps.defaultTheme();
                ps.output_settings = os;
                XMQPrinter pr = new XMQPrinter();
                pr.print_node(ps, doc, 0);
                System.out.print(ps.buffer);
                if (!noFinalNl) System.out.println();
            }
        }
        catch (ParseException e)
        {
            System.err.print(XMQ.printException(e));
        }
        catch (Exception e)
        {
            e.printStackTrace(System.err);
        }
    }

    private static int runGenerateParser(String[] args)
    {
        // xmqj generate-parser --c grammar.ixml [--prefix=name]
        boolean c = false;
        String prefix = null;
        String file = null;
        for (int k = 1; k < args.length; k++)
        {
            if (args[k].equals("--c")) c = true;
            else if (args[k].startsWith("--prefix=")) prefix = args[k].substring(9);
            else if (file == null && !args[k].startsWith("--")) file = args[k];
            else file = null;
        }
        if (!c || file == null)
        {
            System.err.println("Usage: xmqj generate-parser --c grammar.ixml [--prefix=name] > parser.c");
            return 2;
        }
        if (prefix == null)
        {
            // The grammar file name without .ixml, as a C identifier.
            String base = Path.of(file).getFileName().toString().replaceAll("\\.ixml$", "");
            prefix = base.replaceAll("[^A-Za-z0-9_]", "_");
            if (prefix.isEmpty() || Character.isDigit(prefix.charAt(0))) prefix = "p_" + prefix;
        }
        try
        {
            String grammar = Files.readString(Path.of(file), StandardCharsets.UTF_8);
            System.out.print(org.libxmq.imp.ixml.IXMLGenerateC.generate(grammar, prefix, file));
            return 0;
        }
        catch (java.io.IOException | IllegalArgumentException e)
        {
            System.err.println("xmqj generate-parser: " + e.getMessage());
            return 1;
        }
    }

    private static int runAnalyze(String[] args)
    {
        // xmqj analyze grammar.ixml [input] [--tree]
        boolean tree = false;
        java.util.List<String> files = new java.util.ArrayList<>();
        for (int k = 1; k < args.length; k++)
        {
            if (args[k].equals("--tree")) tree = true;
            else files.add(args[k]);
        }
        if (files.isEmpty() || files.size() > 2 || (tree && files.size() != 2))
        {
            System.err.println("Usage: xmqj analyze grammar.ixml [test.input] [--tree]");
            return 2;
        }
        try
        {
            String grammar = Files.readString(Path.of(files.get(0)), StandardCharsets.UTF_8);
            String input = files.size() > 1 ? Files.readString(Path.of(files.get(1)), StandardCharsets.UTF_8) : null;
            if (tree)
            {
                XmlDocument doc = org.libxmq.imp.ixml.IXMLAnalyzer.parse(grammar, input);
                XMQPrintState ps = new XMQPrintState();
                ps.defaultTheme();
                ps.output_settings = new OutputSettings().setIndentAmount(4);
                new XMQPrinter().print_node(ps, doc, 0);
                System.out.println(ps.buffer);
                return 0;
            }
            XmlDocument result = org.libxmq.imp.ixml.IXMLAnalyzer.analyze(grammar, input);
            System.out.print(XmlSave.docDump(result, "UTF-8", true));
            XmlElement root = result.rootElement();
            if (!"true".equals(root.attributeValue("supported"))) return 1;
            if (input == null) return 0;
            XmlElement test = root.element("test");
            return test != null && "accepted".equals(test.attributeValue("status")) ? 0 : 1;
        }
        catch (java.io.IOException | IllegalArgumentException e)
        {
            System.err.println("xmqj analyze: " + e.getMessage());
            return 2;
        }
    }

    /**
     * Load content into a document, like the C xmqParseBufferWithType with
     * content detection. Xml and html are trimmed unless trim_none.
     * A parse error is printed on stderr and null is returned.
     * @param content The content to parse.
     * @param file The source file name, used in error messages.
     * @param trim_none True to keep all whitespace, ie --trim=none.
     * @param no_merge True to not merge text and character entities, ie --nomerge.
     * @return The document or null.
     * @throws ParseException If the xmq content cannot be parsed.
     */
    static XmlDocument load(String content, String file, boolean trim_none, boolean no_merge) throws ParseException
    {
        if (content.startsWith("\uFEFF")) content = content.substring(1);
        int i = 0;
        while (i < content.length())
        {
            char c = content.charAt(i);
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r')
            {
                i++;
                continue;
            }
            break;
        }

        // Xmq input can never start with a less than char, so if the input
        // starts with a less than char (after leading whitespace), then it
        // is xml or html.
        if (i < content.length() && content.charAt(i) == '<')
        {
            XmlDocument doc;
            if (is_html(content))
            {
                // Like C, HTML_PARSE_NOBLANKS is only used with an explicit --trim=heuristic.
                doc = HtmlParser.parse(content, false);
            }
            else
            {
                try
                {
                    int options = XmlParser.NOCDATA;
                    doc = XmlParser.parse(content, file, options);
                }
                catch (XmlParseException e)
                {
                    System.err.print(e.getMessage());
                    return null;
                }
            }
            XmlTrim.fixupCommentsAfterReadin(doc);
            if (!trim_none) XmlTrim.trimWhitespace(doc);
            return doc;
        }

        XMQParseIntoDOM pa = new XMQParseIntoDOM();
        pa.setMergeText(!no_merge);
        pa.parse(content, file);
        return pa.doc();
    }

    /**
     * Detect html content like the C xmqDetectContentType (find_word_ignore_case):
     * the first word after a less than char (or after a less than !doctype) is html.
     * @param content The source text.
     * @return True if the content is recognized as html.
     */
    static boolean is_html(String content)
    {
        int i = skip_ws(content, 0);
        if (i >= content.length() || content.charAt(i) != '<')
        {
            return false;
        }
        String lower = content.toLowerCase(java.util.Locale.ROOT);
        if (lower.startsWith("<!doctype", i))
        {
            return first_word(lower, i + "<!doctype".length()).equalsIgnoreCase("html");
        }
        return first_word(lower, i + 1).equalsIgnoreCase("html");
    }

    static int skip_ws(String s, int i)
    {
        while (i < s.length() && (s.charAt(i) == ' ' || s.charAt(i) == '\t' || s.charAt(i) == '\n' || s.charAt(i) == '\r')) i++;
        return i;
    }

    static String first_word(String s, int i)
    {
        int start = skip_ws(s, i);
        int e = start;
        while (e < s.length())
        {
            char c = s.charAt(e);
            boolean name = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')
                || c == '_' || c == '.' || c == '-' || c == ':';
            if (!name) break;
            e++;
        }
        return s.substring(start, e);
    }

    static void startServer(InputStream in, OutputStream out) throws InterruptedException, ExecutionException
    {
        /*
        XMQLanguageServer server = new XMQLanguageServer();
        Launcher<LanguageClient> l = LSPLauncher.createServerLauncher(server, in, out);
        Future<?> startListening = l.startListening();
        server.setRemoteProxy(l.getRemoteProxy());
        startListening.get();
        */
    }

    static String help = """
Usage: xmqj [options] <file> ( <command> [options] )*

  --debug    Output debug information on stderr.
  --help     Display this help and exit.
  --license  Print license.
  --lines    Assume each input line is a separate document.
  --nomerge  When loading xmq do not merge text quotes and character entities.
  --root=<name> Create a root node <name> unless the file starts with a node with this <name> already.
  --trim=none|heuristic|exact
             The default setting when reading xml/html content is to trim whitespace using a heuristic.
             For xmq/htmq/json the default settings is none since whitespace is explicit in xmq/htmq/json.
             Not yet implemented: exact will trim exactly to the significant whitespace according to xml/html rules.
  --verbose  Output extra information on stderr.
  --version  Output version information and exit.
  --xmq|--htmq|--xml|--html|--ixml|--json|--clines
             The input format is auto detected for xmq/xml/json but you can force the input format here.
  generate-parser --c grammar.ixml [--prefix=name]  Generate a C LL(*) parser with a main function.
  analyze grammar.ixml [test.input] [--tree]  Analyze LL(*) suitability and emit a parser DOM,
             or with --tree parse the input with the LL(*) parser and print the parse tree.
  --ixml=grammar.ixml Parse the content using the supplied grammar file.
  -z         Do not read from stdin nor from a file. Start with an empty dom.
  -i "a=2" Do not read from a file, use the next argument as the content to parse.

To get help on the commands below: xmq help <command>

COMMANDS
  add
  add-root
  browser pager
  delete delete-entity
  for-each
  help
  no-output
  render-html render-terminal render-tex
  replace replace-entity
  quote-c unquote-c
  select
  statistics
  substitite-char-entities substitute-entity
  to-html to-htmq to-json to-lines to-text to-xml to-xmq
  tokenize
  transform
  validate
  lsp

EXAMPLES
  xmq pom.xml page  xmq index.html delete //script delete //style browse
  xmq template.htmq replace-entity DATE 2024-02-08 to-html > index.html
  xmq data.json select /_/work transform format.xslq to-xml > data.xml
  xmq data.html select "//tr[@class='print_list_table_content']" \
                delete //@class \
                add-root Course \
                transform --stringparam=title=Welcome cols.xslq to-text
  xmq work.xml validate --silent work.xsd noout
""";


    static void printHelp()
    {
        System.out.print(help);
    }
}
