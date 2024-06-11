/* libxmq - Copyright (C) 2023 Fredrik Öhrström (spdx: MIT)

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

import javax.xml.XMLConstants;
import javax.xml.parsers.DocumentBuilderFactory;
import javax.xml.parsers.DocumentBuilder;

import org.w3c.dom.DOMException;
import org.w3c.dom.Document;
import org.w3c.dom.DocumentType;
import org.w3c.dom.Element;
import org.w3c.dom.Node;
import org.w3c.dom.NodeList;

public class XMQ
{
    public static void main(String[] args)
    {
        System.out.println("PARSING...");
        try {
            DocumentBuilderFactory factory = DocumentBuilderFactory.newInstance();
            DocumentBuilder builder = factory.newDocumentBuilder();
            Document document = builder.parse("input.xml");
            XMQ q = new XMQ();
            q.processNode(document);
        } catch (Exception e) {
            e.printStackTrace();
        }
    }

    void processNode(Node n)
    {
        String name = n.getNodeName();
//        int type = node.getNodeType();
        System.out.println("N "+name);
    }

    void eat_xmq_comment_to_eol(XMQParseState state, Eaten eaten)
    {
        /*
        SourcePos sp = new Sour
        const char *i = state->i;
        const char *end = state->buffer_stop;

        size_t line = state->line;
        size_t col = state->col;
        increment('/', 1, &i, &line, &col);
        increment('/', 1, &i, &line, &col);

        *comment_start = i;

        char c = 0;
        while (i < end && c != '\n')
        {
            c = *i;
            increment(c, 1, &i, &line, &col);
        }
        if (c == '\n') *comment_stop = i-1;
        else *comment_stop = i;
        state->i = i;
        state->line = line;
        state->col = col;
        */
    }

}
