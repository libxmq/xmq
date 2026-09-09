/* libxmq - Copyright (C) 2023-2025 Fredrik Öhrström (spdx: MIT)

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

package org.libxmq;

import java.io.IOException;
import java.io.StringWriter;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.EnumSet;
import java.util.EnumSet;
import java.util.Locale;
import java.util.ResourceBundle;
import org.libxmq.imp.XMQParseIntoDOM;
import org.libxmq.imp.XMQPrintState;
import org.libxmq.imp.XMQPrinter;
import org.jdom2.Document;

/**
   The XMQ class is used to parse/print/render XMQ/XML/HTML/JSON documents.
*/
public class XMQ
{
    /**
       Construct an XMQ with the default DOM implementation (jdom2).
     */
    public XMQ()
    {
    }

    /**
       Parse a file containing XMQ/XML/HTML/JSON/CLINES.
       @param file The file to parse.
       @param is The input settings, should white space be trimmed, should text nodes be merged etc.
       @return A DOM document.
       @throws IOException if file cannot be read.
       @throws ParseException if the parse failed.
     */
    public org.jdom2.Document parseFile(Path file, InputSettings is) throws IOException, ParseException
    {
        String buffer = Files.readString(file, StandardCharsets.UTF_8);
        return parseBuffer(buffer, is);
    }

    /**
       Parse a buffer containing XMQ/XML/HTML/JSON/CLINES.
       @param buffer The string to parse.
       @param is The input settings, should white space be trimmed, should text nodes be merged etc.
       @return A DOM document.
       @throws ParseException if the parse failed.
     */
    public org.jdom2.Document parseBuffer(String buffer, InputSettings is) throws ParseException
    {
        XMQParseIntoDOM pa = new XMQParseIntoDOM();
        pa.parse(buffer, "buffer");
        return pa.doc();
    }

    /**
       Parse a file containing XMQ/XML/HTML/JSON/CLINES and return a Query object to the DOM.
       @param file The file to parse.
       @param is The input settings, should white space be trimmed, should text nodes be merged etc.
       @return A Query object.
       @throws IOException if file cannot be read.
       @throws ParseException if the parse failed.
     */
    public Query queryFile(Path file, InputSettings is) throws IOException, ParseException
    {
        return new Query(parseFile(file, is));
    }

    /**
       Parse a buffer containing XMQ/XML/HTML/JSON/CLINES and return a Query object for the DOM.
       @param buffer The string to parse.
       @param is The input settings, should white space be trimmed, should text nodes be merged etc.
       @return A Query object.
       @throws ParseException if the parse failed.
     */
    public Query queryBuffer(String buffer, InputSettings is) throws ParseException
    {
        return new Query(parseBuffer(buffer, is));
    }

    /**
       Print the document as XMQ.
       @param doc Document to print.
       @param os  Settings for printing.
       @return A string with XMQ.
     */
    public String toXMQ(Object doc, OutputSettings os)
    {
        XMQPrintState ps = new XMQPrintState();
        ps.defaultTheme();
        XMQPrinter pr = new XMQPrinter();
        pr.print_node(ps, doc, 0);
        return ps.buffer().toString();
    }

    /**
       Print the document as XML. This means that the argument doc
       is assumed to store an XML document. By printing it as XML the
       document will follow the XML rules.
       @param doc Document to print.
       @param os  Settings for printing.
       @return A string with XML.
     */
    public String toXML(Object doc, OutputSettings os)
    {
        try
        {
            org.jdom2.output.XMLOutputter outputter = new org.jdom2.output.XMLOutputter();
            if (doc instanceof Document d)
            {
                return outputter.outputString(d) + "\n";
            }
            if (doc instanceof org.jdom2.Element e)
            {
                return outputter.outputString(new Document(e)) + "\n";
            }
        }
        catch (Exception e)
        {
            e.printStackTrace();
        }
        return "";
    }

    /**
       Print the document as HTML. This means that the argument doc
       is assumed to store a HTML document. By printing it as HTML the
       document will follow the HTML rules (using self closing tags
       like &lt;br&gt; and potentially using HTML entities like &amp;nbsp;)

       @param doc Document to print.
       @param os  Settings for printing.
       @return A string with HTML.
     */
    public String toHTML(Object doc, OutputSettings os)
    {
        return "lll";
    }

    /**
       Print the document as json.
       @param doc Document to print.
       @param os  Settings for printing.
       @return A string with JSON.
     */
    public String toJSON(Object doc, OutputSettings os)
    {
        return "lll";
    }

    /**
       Print the document as clines.
       @param doc Document to print.
       @param os  Settings for printing.
       @return A string with clines.
     */
    public String toCLINES(Object doc, OutputSettings os)
    {
        return "lll";
    }

    /**
       Render the document as XMQ in HTML. I.e. build a new HTML
       document that when viewed in a browser makes it easy to
       read the XMQ.
       @param doc Document to render
       @param os  Settings for rendering
       @return A string with HTML.
     */
    public String renderHTML(Object doc, OutputSettings os)
    {
        return "lll";
    }

    /**
       Render the document as XMQ in TeX. When the TeX document
       is rendered using for example xetex/xelatex, it will generate a pdf
       containing the human readable XMQ.
       @param doc Document to render
       @param os  Settings for rendering
       @return A string with TeX commands.
     */
    public String renderTEX(Object doc, OutputSettings os)
    {
        return "lll";
    }

    /**
       Render a locale adapted text cli error printout of a parse exception.
       @param e The parse exception.
       @param locale The locale to use.
       @return A string to be printed on the console.
    */
    public static String printException(ParseException e, Locale locale)
    {
        StringBuilder out = new StringBuilder();
        out.append(e.source());
        out.append(":");
        out.append(e.line());
        out.append(":");
        out.append(e.column());
        out.append(": ");
        ResourceBundle bundle = ResourceBundle.getBundle("Messages", locale);
        out.append(bundle.getString(e.errorCode().toString()));
        out.append("\n");
        out.append(e.text());
        out.append("\n");
        for (int i = 1; i < e.column(); ++i)
        {
            out.append(" ");
        }
        out.append("^\n");
        return out.toString();
    }

    /**
       Render a locale adapted text cli error printout of a parse exception using the default locale.
       @param e The parse exception.
       @return A string to be printed on the console.
    */
    public static String printException(ParseException e)
    {
        return printException(e, Locale.getDefault());
    }

}
