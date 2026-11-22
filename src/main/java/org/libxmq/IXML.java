/* libxmq - Copyright (C) 2026 Fredrik Öhrström (spdx: MIT)

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

import org.libxmq.imp.ixml.IXMLAnalyzer;
import org.libxmq.imp.ixml.IXMLGrammar;
import org.libxmq.imp.ixml.earley.Earley;
import org.libxmq.imp.xml.XmlDocument;
import org.libxmq.imp.xml.XmlElement;

/**
   A compiled ixml grammar, used to parse many inputs, see XMQ.compileIXML and
   XMQ.parseBufferWithIXML. Like an XMQDoc parsed with XMQ_CONTENT_IXML in xmq.h.
   A grammar that is LL(*) is parsed with the fast predictive parser, any other
   grammar with the Earley parser.
*/
public final class IXML
{
    private final String source;
    private final IXMLAnalyzer ll; // null if the grammar is not LL(*).

    private IXML(String source, IXMLAnalyzer ll)
    {
        this.source = source;
        this.ll = ll;
    }

    /**
       Compile an ixml grammar.
       @param grammar The ixml grammar source.
       @return The compiled grammar.
       @throws ParseException if the grammar has a syntax error.
     */
    public static IXML compile(String grammar) throws ParseException
    {
        try
        {
            IXMLGrammar.parse(grammar);
        }
        catch (IllegalArgumentException e)
        {
            throw error(e.getMessage(), "grammar");
        }
        IXMLAnalyzer ll = null;
        try
        {
            ll = IXMLAnalyzer.compileParser(grammar);
        }
        catch (IllegalArgumentException e)
        {
            // Not LL(*), use the Earley parser.
        }
        return new IXML(grammar, ll);
    }

    /**
       The ixml grammar source.
       @return The source.
     */
    public String source()
    {
        return source;
    }

    /**
       Parse an input with the grammar.
       @param input The input to parse.
       @return The parse tree.
       @throws ParseException if the input is rejected by the grammar.
     */
    public XmlDocument parse(String input) throws ParseException
    {
        if (ll != null)
        {
            XmlDocument d = ll.parseInputOrFailure(input, false);
            if (!IXMLAnalyzer.isFailure(d)) return d;
            XmlElement info = d.rootElement().element("info");
            throw error(info != null ? info.textContent() : "syntax error", "input");
        }
        Earley.Result r = Earley.parse(source, input);
        if (r.doc() != null) return r.doc();
        throw error(r.error() + " at position " + r.position(), "input");
    }

    private static ParseException error(String message, String source)
    {
        return new ParseException(ParseErrorCode.XMQ_ERROR_IXML_SYNTAX_ERROR, message, 0, 0, source, message, 0);
    }
}
