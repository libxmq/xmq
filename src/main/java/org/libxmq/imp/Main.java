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

import org.xml.sax.InputSource;
import java.io.StringReader;
import java.io.StringWriter;

import org.jdom2.Document;
import org.jdom2.Element;
import org.jdom2.DocType;
import org.jdom2.ProcessingInstruction;
import org.jdom2.input.SAXBuilder;
import org.jdom2.output.Format;
import org.jdom2.output.XMLOutputter;
import org.jdom2.input.sax.XMLReaders;

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
        if (args.length == 0)
        {
            printHelp();
            return;
        }

        if (args.length == 1 && args[0].equals("lsp"))
        {
            startServer(System.in, System.out);
            return;
        }

        try
        {
            Path p = Paths.get(args[0]);
            String content = Files.readString(p, StandardCharsets.UTF_8);
            if (args.length > 1 && args[1].equals("tokenize"))
            {
                XMQParseIntoTokens to = new XMQParseIntoTokens();
                if (args.length > 2 && args[2].equals("--type=debugtokens"))
                {
                    to.debugTokens();
                }
                else if (args.length > 2 && args[2].equals("--type=debugcontent"))
                {
                    to.debugContent();
                }
                to.parse(content, args[0]);
                System.out.println("");
            }
            else if (args.length > 1 && args[1].equals("to-xml"))
            {
                XMQParseIntoDOM pa = new XMQParseIntoDOM();
                pa.parse(content, args[0]);

                Format format = Format.getPrettyFormat();
                format.setOmitDeclaration(true);
                format.setEncoding("utf-8");
                XMLOutputter outputter = new XMLOutputter(format);
                StringWriter sw = new StringWriter();
                sw.append("<?xml version=\"1.0\" encoding=\"utf-8\"?>");
                sw.append(outputter.outputString(pa.doc()));
                sw.append("\n");
                System.out.print(sw.toString());
            }
            else
            {
                Document doc;

                // Xmq input can never start with a less than char, so if the
                // input starts with a less than char (after leading whitespace),
                // then it is xml. Then parse the xml with the default xml
                // parser and print the dom as xmq below.
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

                if (i < content.length() && content.charAt(i) == '<')
                {
                    Document xml_doc = FixedEntityParser.parseWithoutDuplicateEntities(content);
                    doc = xml_doc;
                }
                else
                {
                    XMQParseIntoDOM pa = new XMQParseIntoDOM();
                    pa.parse(content, args[0]);
                    doc = pa.doc();
                }

                XMQPrintState ps = new XMQPrintState();
                ps.defaultTheme();
                ps.output_settings = new OutputSettings().setIndentAmount(4);
                for (String arg : args)
                {
                    if (arg.equals("--compact"))
                    {
                        ps.output_settings.setCompact(true);
                    }
                }
                XMQPrinter pr = new XMQPrinter();
                pr.print_node(ps, doc, 0);
                System.out.print(ps.buffer);
                boolean noFinalNl = false;
                for (String arg : args)
                {
                    if (arg.equals("--no-final-nl"))
                    {
                        noFinalNl = true;
                    }
                }
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

    /**
     * Constructs the xmq doctype value from an xml doctype declaration in the
     * source. For the declaration
     *
     *     <!DOCTYPE time [
     *     <!ENTITY copy "&#169;">
     *     ]>
     *
     * the returned value is (\n is a real newline)
     *
     *     time [\n<!ENTITY copy "&#169;">\n]
     *
     * That is, the name, then, if there is an internal subset, a bracketed
     * list with one entity declaration per line. The printer prints it as a
     * multi line value, or as a single line if compact mode is set.
     * @param content The original xml source.
     * @param dtd The parsed jdom2 document type.
     * @return The xmq doctype value.
     */
    static String doctype_value(String content, DocType dtd)
    {
        StringBuilder v = new StringBuilder(dtd.getElementName());
        int p = content.indexOf("<!DOCTYPE");
        if (p < 0) return v.toString();

        // Locate the internal subset of the doctype, if any. The doctype has
        // the form <!DOCTYPE name [ internal-subset ]>, where the internal
        // subset may contain entity declarations of its own (which end with a
        // greater than char). All scans are done outside of quoted strings.
        char quote = 0;
        int lb = -1, rb = -1, end = -1;
        for (int k = p; k < content.length(); k++)
        {
            char c = content.charAt(k);
            if (quote != 0)
            {
                if (c == quote) quote = 0;
                continue;
            }
            if (c == '\'' || c == '"')
            {
                quote = c;
                continue;
            }
            if (lb < 0 && c == '[')
            {
                lb = k;
                continue;
            }
            if (lb >= 0 && rb < 0 && c == ']')
            {
                rb = k;
                continue;
            }
            if (c == '>')
            {
                if (lb < 0 || rb >= 0)
                {
                    // A doctype with no internal subset ends at the first
                    // greater than char. A doctype with an internal subset
                    // ends at the greater than char after the closing bracket.
                    end = k;
                    break;
                }
                // A greater than char inside the internal subset (the one
                // that ends an entity declaration) is not the end of the
                // doctype; keep scanning for the closing bracket.
            }
        }
        if (end < 0 || lb < 0 || rb < 0) return v.toString();

        // The internal subset, one entity declaration per line, each line
        // trimmed of incidental indentation, and empty lines dropped.
        StringBuilder sub = new StringBuilder();
        for (String line : content.substring(lb+1, rb).split("\n"))
        {
            String t = line.strip();
            if (t.isEmpty()) continue;
            if (sub.length() > 0) sub.append('\n');
            sub.append(t);
        }
        if (sub.length() == 0) return v.toString();

        v.append(" [");
        v.append('\n');
        v.append(sub);
        v.append('\n');
        v.append(']');
        return v.toString();
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
