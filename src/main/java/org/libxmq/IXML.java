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

import java.nio.charset.StandardCharsets;

import org.libxmq.imp.ixml.IXMLAnalyzer;
import org.libxmq.imp.ixml.IXMLGrammar;
import org.libxmq.imp.ixml.earley.Earley;
import org.libxmq.imp.ixml.earley.Grammar;
import org.libxmq.imp.ixml.earley.Utf8;
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
    // The grammar of the Earley parser, created on the first parse. It keeps the tokens of
    // the code points seen in the inputs, like used_unicodes in the C version, so only the
    // new code points of an input are added to it. Parses with it are synchronized on it.
    private Grammar earley;

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
        if (ll != null) return parseLL(input);
        byte[] buf = input.getBytes(StandardCharsets.UTF_8);
        return parseEarley(buf, 0, buf.length);
    }

    /**
       Parse an UTF-8 input with the grammar. The Earley parser decodes the code points
       directly from the bytes, the input is not copied into a String.
       @param input The input to parse, UTF-8.
       @return The parse tree.
       @throws ParseException if the input is rejected by the grammar, or is not valid UTF-8.
     */
    public XmlDocument parse(byte[] input) throws ParseException
    {
        return parse(input, 0, input.length);
    }

    /**
       Parse an UTF-8 input with the grammar, the bytes start to stop of the buffer.
       @param buf The buffer with the input, UTF-8.
       @param start The first byte of the input.
       @param stop The end of the input, exclusive.
       @return The parse tree.
       @throws ParseException if the input is rejected by the grammar, or is not valid UTF-8.
     */
    public XmlDocument parse(byte[] buf, int start, int stop) throws ParseException
    {
        if (ll == null) return parseEarley(buf, start, stop);
        // The LL(*) parser still works on a String.
        for (int i = start; i < stop; )
        {
            int d = Utf8.decode(buf, i, stop);
            if (d < 0) throw error(Earley.INVALID_UTF8 + ": " + Utf8.describeInvalid(buf, i, stop), "input");
            i += Utf8.length(d);
        }
        return parseLL(new String(buf, start, stop - start, StandardCharsets.UTF_8));
    }

    private XmlDocument parseLL(String input) throws ParseException
    {
        XmlDocument d = ll.parseInputOrFailure(input, false);
        if (!IXMLAnalyzer.isFailure(d)) return d;
        XmlElement info = d.rootElement().element("info");
        throw error(info != null ? info.textContent() : "syntax error", "input");
    }

    private XmlDocument parseEarley(byte[] buf, int start, int stop) throws ParseException
    {
        Earley.Result r;
        synchronized (this)
        {
            if (earley == null) earley = Earley.grammar(source);
            r = Earley.parse(earley, buf, start, stop);
        }
        if (r.doc() != null) return r.doc();
        throw error(r.error() + " at position " + r.position(), "input");
    }

    private static ParseException error(String message, String source)
    {
        return new ParseException(ParseErrorCode.XMQ_ERROR_IXML_SYNTAX_ERROR, message, 0, 0, source, message, 0);
    }
}
