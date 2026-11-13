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
import org.libxmq.imp.xml.HtmlSave;
import org.libxmq.imp.xml.XmlDocument;
import org.libxmq.imp.xml.XmlElement;
import org.libxmq.imp.xml.XmlSave;

/**
   The XMQ class is used to parse/print/render XMQ/XML/HTML/JSON documents.
*/
public class XMQ
{
    /**
       Construct an XMQ using the xmq DOM implementation.
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
    public XmlDocument parseFile(Path file, InputSettings is) throws IOException, ParseException
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
    public XmlDocument parseBuffer(String buffer, InputSettings is) throws ParseException
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
        if (doc instanceof XmlDocument d)
        {
            return XmlSave.docDump(d, "utf-8");
        }
        if (doc instanceof XmlElement e)
        {
            return XmlSave.nodeDump(e.doc(), e, 0, false) + "\n";
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
        if (doc instanceof XmlDocument d)
        {
            return HtmlSave.docContentDump(d, true);
        }
        return "";
    }

    /**
       Print the document as json.
       @param doc Document to print.
       @param os  Settings for printing.
       @return A string with JSON.
     */
    public String toJSON(Object doc, OutputSettings os)
    {
        if (!(doc instanceof org.libxmq.imp.xml.XmlDocument d)) return "";
        return new org.libxmq.imp.JsonPrinter().toJson(d, os, true);
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
        // The same format as the C generate_state_error_message, followed by an empty line
        // like the C command line prints it.
        StringBuilder out = new StringBuilder();
        out.append(e.source()).append(":").append(e.line()).append(":").append(e.column()).append(": ");
        out.append(e.errorCode().value >= ParseErrorCode.XMQ_WARNING_QUOTES_NEEDED.value ? "warning" : "error");
        out.append(": ");
        ResourceBundle bundle = ResourceBundle.getBundle("Messages", locale);
        out.append(bundle.getString(e.errorCode().toString()));
        String text = e.text() == null ? "" : e.text();
        int at = e.column() - 1;
        boolean atEnd = at >= text.length();
        if (e.errorCode() == ParseErrorCode.XMQ_ERROR_INVALID_CHAR || e.errorCode() == ParseErrorCode.XMQ_ERROR_JSON_INVALID_CHAR)
        {
            int cp = atEnd ? 0 : text.codePointAt(at);
            out.append(" \"").append(cp == 0 ? "" : new String(Character.toChars(cp))).append("\" U+")
               .append(Integer.toHexString(cp).toUpperCase(Locale.ROOT));
        }
        if (!atEnd)
        {
            out.append("\n").append(text).append("\n").append(" ".repeat(Math.max(1, at))).append("^");
        }
        out.append("\n\n");
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
