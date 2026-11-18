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
        // Like C stdio, stdout is fully buffered when it is not a terminal: the bytes reach the
        // file descriptor in 4096 byte blocks (and at fflush/exit), so the order of the stdout and
        // stderr output in a combined file is the same as for the C version.
        if (!stdoutIsTerminal())
        {
            final java.io.PrintStream out = new java.io.PrintStream(new CStdout(), false, StandardCharsets.UTF_8);
            System.setOut(out);
            Runtime.getRuntime().addShutdownHook(new Thread(out::flush));
        }
        else
        {
            System.setOut(new java.io.PrintStream(
                new java.io.FileOutputStream(java.io.FileDescriptor.out), true, StandardCharsets.UTF_8));
        }

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

        // The command line is parsed with the grammar library/xmq-utils/xmqcli.ixml, see Cli.
        // Like the C version (and libxslt), transforms and deep documents recurse deeply,
        // run the command line in a thread with a large stack.
        int[] result = new int[1];
        Thread t = new Thread(null, () -> result[0] = Cli.run(args), "xmqj", 1L << 30);
        t.start();
        t.join();
        int status = result[0];
        System.out.flush();
        if (status != 0) System.exit(status);
    }

    private static int runGenerateParser(String[] args)
    {
        // xmqj generate-parser --c grammar.ixml [--prefix=name]
        // xmqj generate-parser --java grammar.ixml [--package=a.b] [--class=Name]
        boolean c = false, java = false;
        String prefix = null, pkg = null, cls = null;
        String file = null;
        boolean bad = false;
        for (int k = 1; k < args.length; k++)
        {
            if (args[k].equals("--c")) c = true;
            else if (args[k].equals("--java")) java = true;
            else if (args[k].startsWith("--prefix=")) prefix = args[k].substring(9);
            else if (args[k].startsWith("--package=")) pkg = args[k].substring(10);
            else if (args[k].startsWith("--class=")) cls = args[k].substring(8);
            else if (file == null && !args[k].startsWith("--")) file = args[k];
            else bad = true;
        }
        if (c == java || file == null || bad)
        {
            System.err.println("Usage: xmqj generate-parser --c grammar.ixml [--prefix=name] > parser.c");
            System.err.println("       xmqj generate-parser --java grammar.ixml [--package=a.b] [--class=Name] > Name.java");
            return 2;
        }
        // The grammar file name without .ixml, as an identifier.
        String base = Path.of(file).getFileName().toString().replaceAll("\\.ixml$", "").replaceAll("[^A-Za-z0-9_]", "_");
        if (base.isEmpty() || Character.isDigit(base.charAt(0))) base = "p_" + base;
        try
        {
            String grammar = Files.readString(Path.of(file), StandardCharsets.UTF_8);
            if (c)
            {
                System.out.print(org.libxmq.imp.ixml.IXMLGenerateC.generate(grammar, prefix != null ? prefix : base, file));
            }
            else
            {
                if (cls == null) cls = Character.toUpperCase(base.charAt(0)) + base.substring(1) + "Parser";
                System.out.print(org.libxmq.imp.ixml.IXMLGenerateJava.generate(grammar, pkg, cls, file));
            }
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
                System.out.println(XMQPrinter.toXmq(doc));
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
     * @param no_merge True to not merge text and character entities, ie --no-merge.
     * @return The document or null.
     * @throws ParseException If the xmq content cannot be parsed.
     */
    static XmlDocument load(String content, String file, boolean trim_none, boolean no_merge) throws ParseException
    {
        return load(content, file, trim_none, no_merge, null);
    }

    /**
     * Is the content json, like the json detection in the C xmqDetectContentType:
     * it starts with { " [ or a digit, or is exactly true false or null.
     * @param content The content.
     * @param i Position of the first non whitespace character.
     * @return True if json.
     */
    static boolean is_json(String content, int i)
    {
        if (i >= content.length()) return false;
        char c = content.charAt(i);
        if (c == '{' || c == '"' || c == '[' || (c >= '0' && c <= '9')) return true;
        int l = c == 't' || c == 'n' ? 4 : c == 'f' ? 5 : 0;
        if (l == 0 || i + l > content.length()) return false;
        String w = content.substring(i, i + l);
        if (!w.equals("true") && !w.equals("false") && !w.equals("null")) return false;
        return i + l == content.length() || (content.charAt(i + l) == '\n' && i + l + 1 == content.length());
    }

    /**
     * Load content into a document, with a forced format.
     * @param content The content to parse.
     * @param file The source file name, used in error messages.
     * @param trim_none True to keep all whitespace, ie --trim=none.
     * @param no_merge True to not merge text and character entities, ie --no-merge.
     * @param format The format: null to detect, or json.
     * @return The document or null.
     * @throws ParseException If the content cannot be parsed.
     */
    static XmlDocument load(String content, String file, boolean trim_none, boolean no_merge, String format) throws ParseException
    {
        return load(content, null, file, trim_none, no_merge, format);
    }

    /**
     * Load content, raw is the undecoded input (or null), used when the content is
     * xml since the xml parser detects the encoding itself (UTF-16, ISO-8859-1).
     */
    static XmlDocument load(String content, byte[] raw, String file, boolean trim_none, boolean no_merge, String format) throws ParseException
    {
        if ("yaml".equals(format))
        {
            checkUtf8(raw, file);
            YamlParser yp = new YamlParser();
            yp.parseYaml(content, file);
            return yp.doc();
        }
        if ("json".equals(format))
        {
            checkUtf8(raw, file);
            JsonParser jp = new JsonParser();
            jp.parseJson(content, file);
            return jp.doc();
        }
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
                // The parser works on the bytes, like C, invalid UTF-8 becomes U+FFFD there.
                byte[] bytes = raw;
                // Like C skip_any_potential_bom: a UTF-8 bom is skipped if the input has at least four bytes.
                if (bytes != null && bytes.length > 3 && (bytes[0] & 0xff) == 0xEF && (bytes[1] & 0xff) == 0xBB && (bytes[2] & 0xff) == 0xBF)
                {
                    bytes = java.util.Arrays.copyOfRange(bytes, 3, bytes.length);
                }
                doc = bytes != null ? HtmlParser.parse(bytes, false) : HtmlParser.parse(content, false);
                if (doc.rootElement() == null)
                {
                    // Like C xmq_parse_buffer_html.
                    System.err.print("empty document\n");
                    return null;
                }
            }
            else
            {
                try
                {
                    int options = XmlParser.NOCDATA;
                    doc = raw != null ? XmlParser.parse(raw, file, options) : XmlParser.parse(content, file, options);
                }
                catch (XmlParseException e)
                {
                    e.print();
                    return null;
                }
            }
            XmlTrim.fixupCommentsAfterReadin(doc);
            if (!trim_none) XmlTrim.trimWhitespace(doc);
            return doc;
        }

        if (format == null && is_json(content, i))
        {
            checkUtf8(raw, file);
            JsonParser jp = new JsonParser();
            jp.parseJson(content, file);
            return jp.doc();
        }

        checkUtf8(raw, file);
        XMQParseIntoDOM pa = new XMQParseIntoDOM();
        pa.setMergeText(!no_merge);
        pa.parse(content, file);
        return pa.doc();
    }

    /**
     * The xmq, json and yaml sources must be valid UTF-8, like the C version
     * (xmq_reject_invalid_utf8). raw is the undecoded input, or null if the
     * content did not come from bytes.
     */
    static void checkUtf8(byte[] raw, String file) throws ParseException
    {
        if (raw == null) return;
        ParseException bad = invalidUtf8(raw, file);
        if (bad != null) throw bad;
    }

    /**
     * Find the first invalid UTF-8 sequence (a stray continuation byte, a truncated sequence,
     * an overlong encoding, a surrogate or above U+10FFFF), like find_invalid_utf8 in the C
     * version, and return the error for it, or null if the input is valid. The error shows the
     * line up to the bad bytes, the marker indented by bytes like the C version.
     * @param raw The input bytes.
     * @param source The source name.
     * @return The error or null.
     */
    static ParseException invalidUtf8(byte[] raw, String source)
    {
        int k = 0;
        while (k < raw.length)
        {
            int c = raw[k] & 0xFF;
            if (c < 0x80) { k++; continue; }
            int need = 0, v = 0, min = 0;
            if (c >= 0xC2 && c < 0xE0) { need = 2; v = c & 0x1F; min = 0x80; }
            else if (c >= 0xE0 && c < 0xF0) { need = 3; v = c & 0x0F; min = 0x800; }
            else if (c >= 0xF0 && c < 0xF5) { need = 4; v = c & 0x07; min = 0x10000; }
            boolean ok = need > 0 && raw.length - k >= need;
            for (int j = 1; ok && j < need; j++)
            {
                if ((raw[k + j] & 0xC0) != 0x80) ok = false;
                else v = (v << 6) | (raw[k + j] & 0x3F);
            }
            if (ok && (v < min || v > 0x10FFFF || (v >= 0xD800 && v < 0xE000))) ok = false;
            if (!ok) break;
            k += need;
        }
        if (k >= raw.length) return null;
        int l = 1;
        while (l < 4 && k + l < raw.length && (raw[k + l] & 0xC0) == 0x80) l++;
        StringBuilder info = new StringBuilder();
        for (int j = 0; j < l; j++) info.append(String.format(j > 0 ? " 0x%02X" : "0x%02X", raw[k + j] & 0xFF));
        int line = 1, col = 1, line_start = 0;
        for (int j = 0; j < k; j++)
        {
            if (raw[j] == '\n') { line++; col = 1; line_start = j + 1; }
            else if ((raw[j] & 0xC0) != 0x80) col++;
        }
        String text = new String(raw, line_start, k - line_start, StandardCharsets.UTF_8);
        // The text ends at the bad bytes, the marker goes after it.
        return new ParseException(org.libxmq.ParseErrorCode.XMQ_ERROR_INVALID_UTF8, text, line, col, source, info.toString(), k - line_start);
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
  --no-merge When loading xmq do not merge text quotes and character entities.
  --we-should-stop-using-yaml
             Do not start the to-yaml output with the comment # we should stop using yaml
  --root=<name> Create a root node <name> unless the file starts with a node with this <name> already.
  --trim=none|heuristic|exact
             The default setting when reading xml/html content is to trim whitespace using a heuristic.
             For xmq/htmq/json the default settings is none since whitespace is explicit in xmq/htmq/json.
             Not yet implemented: exact will trim exactly to the significant whitespace according to xml/html rules.
  --verbose  Output extra information on stderr.
  --version  Output version information and exit.
  --features Print the optional parts compiled in: xmq-printing json yaml xslt xsd ixml html.
  --xmq|--htmq|--xml|--html|--ixml|--json|--clines
             The input format is auto detected for xmq/xml/json but you can force the input format here.
  generate-parser --c grammar.ixml [--prefix=name]  Generate a C LL(*) parser with a main function.
  generate-parser --java grammar.ixml [--package=a.b] [--class=Name]  Generate a Java LL(*) parser with a main method.
  analyze grammar.ixml [test.input] [--tree]  Analyze LL(*) suitability and emit a parser DOM,
             or with --tree parse the input with the LL(*) parser and print the parse tree.
  --ixml=grammar.ixml Parse the content using the supplied grammar file.
  --traceixml=trace.xmq Write a trace of the ixml parse (rules, Earley items, tree) to the file,
             render it with library/xmq-utils/render-trace-to-html.xslq or render-trace-to-tex.xslq.
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
  xmq pom.xml page
  xmq index.html delete //script delete //style browse
  xmq template.htmq replace-entity DATE 2024-02-08 to-html > index.html
  xmq data.json select /_/work transform format.xslq to-xml > data.xml
  xmq data.html select "//tr[@class='print_list_table_content']" \
                delete //@class \
                add-root Course \
                transform --stringparam=title=Welcome cols.xslq to-text
  xmq work.xml validate --silent work.xsd no-output
""";


    static void printHelp()
    {
        System.out.print(help);
    }

    /**
     * True when stdout is a terminal. Before JDK 22 System.console() is null unless stdin and
     * stdout are terminals. JDK 22-24 return a console even when stdout is redirected (JDK 25
     * does not), there Console.isTerminal() (JDK 22) tells, called by reflection since the
     * code is compiled for Java 17.
     * @return true if stdout is a terminal.
     */
    static boolean stdoutIsTerminal()
    {
        java.io.Console c = System.console();
        if (c == null) return false;
        try
        {
            java.lang.reflect.Method m = java.io.Console.class.getMethod("isTerminal");
            return (Boolean)m.invoke(c);
        }
        catch (ReflectiveOperationException e)
        {
            return true; // Before JDK 22 a console means a terminal.
        }
    }

    /** A fully buffered stdout like glibc's (block size 4096, flushed at multiples of the block size). */
    static final class CStdout extends java.io.OutputStream
    {
        private static final int BLOCK = 4096;
        private final java.io.FileOutputStream fd = new java.io.FileOutputStream(java.io.FileDescriptor.out);
        private byte[] buf = new byte[BLOCK];
        private int len;

        @Override
        public synchronized void write(int b) throws java.io.IOException
        {
            write(new byte[] { (byte)b }, 0, 1);
        }

        @Override
        public synchronized void write(byte[] b, int off, int n) throws java.io.IOException
        {
            if (len + n > buf.length) buf = java.util.Arrays.copyOf(buf, Math.max(buf.length * 2, len + n));
            System.arraycopy(b, off, buf, len, n);
            len += n;
            if (len >= BLOCK)
            {
                int w = len - len % BLOCK;
                fd.write(buf, 0, w);
                System.arraycopy(buf, w, buf, 0, len - w);
                len -= w;
            }
        }

        @Override
        public synchronized void flush() throws java.io.IOException
        {
            if (len > 0) fd.write(buf, 0, len);
            len = 0;
        }
    }
}
