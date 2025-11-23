/* libxmq - Copyright (C) 2025 Fredrik Öhrström (spdx: MIT)

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

import javax.xml.transform.*;
import javax.xml.transform.dom.DOMSource;
import javax.xml.transform.stream.StreamResult;

public class Main
{
    public static void main(String[] args) throws Exception
    {
        if (args.length == 0)
        {
            printHelp();
            return;
        }

        try
        {
            Path p = Paths.get(args[0]);
            String content = Files.readString(p, StandardCharsets.UTF_8);
            if (args.length > 1 && args[1].equals("tokenize"))
            {
                XMQParseIntoTokens to = new XMQParseIntoTokens();
                to.parse(content, args[0]);
                System.out.println("");
            }
            else if (args.length > 1 && args[1].equals("to-xml"))
            {
                XMQParseIntoDOM pa = new XMQParseIntoDOM();
                pa.parse(content, args[0]);

                TransformerFactory transformerFactory = TransformerFactory.newInstance();
                Transformer transformer = transformerFactory.newTransformer();
                transformer.setOutputProperty(OutputKeys.INDENT, "no");
                DOMSource source = new DOMSource(pa.doc());
                StreamResult result = new StreamResult(System.out);
                transformer.transform(source, result);
            }
            else
            {
                XMQParseIntoDOM pa = new XMQParseIntoDOM();
                pa.parse(content, args[0]);

                XMQPrintState ps = new XMQPrintState();
                ps.defaultTheme();
                XMQPrinter pr = new XMQPrinter();
                pr.print_node(ps, pa.doc(), 0);
                System.out.print(ps.buffer);
            }

        }
        catch (Exception e)
        {
            System.err.println(e);
        }
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
