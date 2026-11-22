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
import java.util.Locale;
import java.util.ResourceBundle;
import org.libxmq.imp.XMQParseIntoDOM;
import org.libxmq.imp.XMQPrintState;
import org.libxmq.imp.XMQPrinter;
import org.libxmq.imp.xml.HtmlSave;
import org.libxmq.imp.xml.XmlDocument;
import org.libxmq.imp.xml.XmlDomApi;
import org.libxmq.imp.xml.XmlElement;
import org.libxmq.imp.xml.XmlNs;
import org.libxmq.imp.xml.XmlSave;
import org.libxmq.imp.xml.XmlText;

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
       Create a new empty document, like xmqNewDoc.
       @return The document.
     */
    public XmlDocument newDoc()
    {
        return XmlDomApi.domNew();
    }

    /**
       Set the doctype of the document, eg html, like xmqSetDocType.
       @param doc The document.
       @param name The doctype name.
     */
    public void setDocType(XmlDocument doc, String name)
    {
        XmlDomApi.domSetDtd(doc, name, null, null);
    }

    /**
       Add the root element to the document, like xmqAddRootElement.
       @param doc The document.
       @param name The element name.
       @param ns The namespace, NS.NONE or NS.here(uri). NS.PARENT is the same as NS.NONE here.
       @return The root element.
       @throws IllegalArgumentException if the namespace is invalid.
     */
    public XmlElement addRootElement(XmlDocument doc, String name, NS ns)
    {
        XmlElement e = (XmlElement)XmlDomApi.nodeNewElement(doc, name, null);
        XmlDomApi.domSetRootElement(doc, e);
        if (ns.action() != XmlDomApi.NS_PARENT) applyNs(e, ns);
        return e;
    }

    /**
       Declare a namespace with a prefix on an element, without using it for the
       element itself, like xmqAddNamespace. Use it in children with NS.ancestor(uri).
       @param element The element.
       @param ns The namespace, NS.here("{prefix}uri"), the prefix is required.
       @throws IllegalArgumentException if the namespace is not NS.here with a prefix,
               or if the prefix is already declared on the element for another uri.
     */
    public void addNamespace(XmlElement element, NS ns)
    {
        String[] pu = ns.action() == XmlDomApi.NS_HERE ? XmlDomApi.nsSplitPuri(ns.uri()) : null;
        if (pu == null || pu[0] == null)
        {
            throw new IllegalArgumentException("addNamespace needs NS.here(\"{prefix}uri\")");
        }
        XmlNs found = XmlDomApi.nsLookupPrefix(element, pu[0]);
        if (found == null || found.href() == null || !found.href().equals(pu[1]))
        {
            if (XmlDomApi.nsDeclare(element, pu[0], pu[1]) == null)
            {
                throw new IllegalArgumentException("The prefix " + pu[0] + " is already declared on " + element.name());
            }
        }
    }

    /**
       Add an element as the last child of parent, like xmqAddElement.
       @param parent The parent element.
       @param name The element name.
       @param ns The namespace, NS.NONE, NS.PARENT, NS.here(uri) or NS.ancestor(uri).
       @return The new element.
       @throws IllegalArgumentException if the namespace is invalid.
     */
    public XmlElement addElement(XmlElement parent, String name, NS ns)
    {
        // The new element is in the namespace of the parent, unless ns says otherwise.
        XmlElement e = (XmlElement)XmlDomApi.nodeNewElement(parent.doc(), name, parent.ns());
        XmlDomApi.nodeAppendChild(parent, e);
        if (ns.action() != XmlDomApi.NS_PARENT) applyNs(e, ns);
        return e;
    }

    /**
       Add an element with attributes, like xmqAddElementWithAttrs.
       @param parent The parent element.
       @param name The element name.
       @param ns The namespace, see addElement.
       @param attrs The attributes as name, value pairs, eg "href", "https://libxmq.org".
       @return The new element.
     */
    public XmlElement addElementWithAttrs(XmlElement parent, String name, NS ns, String... attrs)
    {
        XmlElement e = addElement(parent, name, ns);
        setAttrs(e, attrs);
        return e;
    }

    /**
       Add an element with a text value, key=value in xmq, like xmqAddKeyValue.
       @param parent The parent element.
       @param key The element name.
       @param value The text, an empty or null value gives an empty element.
       @param ns The namespace, see addElement.
       @return The new element.
     */
    public XmlElement addKeyValue(XmlElement parent, String key, String value, NS ns)
    {
        XmlElement e = addElement(parent, key, ns);
        if (value != null && !value.isEmpty()) XmlDomApi.nodeAppendChild(e, XmlDomApi.nodeNewText(parent.doc(), value));
        return e;
    }

    /**
       Add an element with a text value and attributes, like xmqAddKeyValueWithAttrs.
       @param parent The parent element.
       @param key The element name.
       @param value The text.
       @param ns The namespace, see addElement.
       @param attrs The attributes as name, value pairs.
       @return The new element.
     */
    public XmlElement addKeyValueWithAttrs(XmlElement parent, String key, String value, NS ns, String... attrs)
    {
        XmlElement e = addKeyValue(parent, key, value, ns);
        setAttrs(e, attrs);
        return e;
    }

    private static void applyNs(XmlElement e, NS ns)
    {
        if (!XmlDomApi.nodeApplyXmqns(e, ns.action(), ns.uri()))
        {
            throw new IllegalArgumentException("Bad namespace " + ns.uri() + " for element " + e.name());
        }
    }

    private static void setAttrs(XmlElement e, String... attrs)
    {
        if (attrs.length % 2 != 0) throw new IllegalArgumentException("The attributes must be name, value pairs.");
        for (int i = 0; i < attrs.length; i += 2)
        {
            e.setAttribute(attrs[i], attrs[i + 1] == null ? "" : attrs[i + 1]);
        }
    }

    /**
       Compile an ixml grammar, to parse inputs with parseBufferWithIXML. Like parsing
       the grammar with xmqParseBufferWithType and XMQ_CONTENT_IXML.
       @param grammar The ixml grammar source.
       @return The compiled grammar.
       @throws ParseException if the grammar has a syntax error.
     */
    public IXML compileIXML(String grammar) throws ParseException
    {
        return IXML.compile(grammar);
    }

    /**
       Parse a buffer with an ixml grammar, like xmqParseBufferWithIXML.
       @param buffer The input to parse.
       @param ixml The compiled grammar.
       @return The parse tree.
       @throws ParseException if the input is rejected by the grammar.
     */
    public XmlDocument parseBufferWithIXML(String buffer, IXML ixml) throws ParseException
    {
        return ixml.parse(buffer);
    }

    /**
       Quote a value as xmq on a single line, eg 'car go' or ('howdy'&amp;#10;'dowdy'),
       like xmqCompactQuote.
       @param content The value.
       @return The quoted value.
     */
    public String compactQuote(String content)
    {
        if (content == null || content.isEmpty()) return "''";
        XmlDocument d = XmlDomApi.domNew();
        XmlElement e = (XmlElement)XmlDomApi.nodeNewElement(d, "x", null);
        XmlDomApi.domSetRootElement(d, e);
        XmlDomApi.nodeAppendChild(e, new XmlText(content));
        String s = toXMQ(d, new OutputSettings().setCompact(true).setEscapeNewlines(true));
        // Remove x= and the newline.
        return s.substring(2, s.length() - 1);
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
        ps.setOutputSettings(os != null ? os : new OutputSettings());
        ps.defaultTheme();
        XMQPrinter pr = new XMQPrinter();
        pr.print_node(ps, doc, 0);
        return ps.buffer().toString() + "\n";
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
        // The detail follows the error: "unexpected character: YAML: ..."
        if (e.info() != null && !e.info().isEmpty()) out.append(": ").append(e.info());
        String text = e.text() == null ? "" : e.text();
        int at = e.charIndex();
        boolean at_end = at < 0 || at >= text.length();
        if (e.errorCode() == ParseErrorCode.XMQ_ERROR_INVALID_CHAR || e.errorCode() == ParseErrorCode.XMQ_ERROR_JSON_INVALID_CHAR)
        {
            int cp = at_end ? 0 : text.codePointAt(at);
            out.append(" \"").append(cp == 0 ? "" : new String(Character.toChars(cp))).append("\" U+")
               .append(Integer.toHexString(cp).toUpperCase(Locale.ROOT));
        }
        // For invalid UTF-8 the text is the line up to the bad bytes and the marker goes after it.
        if (!at_end || e.errorCode() == ParseErrorCode.XMQ_ERROR_INVALID_UTF8)
        {
            // C prints the line as a C string, it ends at a NUL character.
            int nul = text.indexOf('\0');
            String shown = nul >= 0 ? text.substring(0, nul) : text;
            out.append("\n").append(shown).append("\n").append(" ".repeat(Math.max(1, at))).append("^");
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
