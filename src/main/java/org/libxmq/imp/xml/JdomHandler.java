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

package org.libxmq.imp.xml;

import org.jdom2.Attribute;
import org.jdom2.Comment;
import org.jdom2.Content;
import org.jdom2.DocType;
import org.jdom2.Document;
import org.jdom2.Element;
import org.jdom2.EntityRef;
import org.jdom2.ProcessingInstruction;
import org.jdom2.Text;

import java.util.ArrayList;
import java.util.List;
import java.util.Stack;

/** A handler that builds a JDOM2 document. */
public class JdomHandler implements Handler
{
    final boolean html;
    final boolean no_trim;

    JdomHandler(boolean html)
    {
        this(html, false);
    }

    JdomHandler(boolean html, boolean no_trim)
    {
        this.html = html;
        this.no_trim = no_trim;
    }

    final Document doc = new Document();
    final Stack<Element> elements = new Stack<>();
    Element root = null;
    final StringBuilder pending_text = new StringBuilder();

    public void doctype(String name, String subset) throws Exception
    {
        DocType d = new DocType(name);
        if (subset != null)
        {
            d.setInternalSubset(subset);
        }
        doc.setDocType(d);
    }

    public void processingInstruction(String target, String data) throws Exception
    {
        ProcessingInstruction pi = new ProcessingInstruction(target, data);
        add(pi);
    }

    public void comment(String text) throws Exception
    {
        add(new Comment(text));
    }

    Element top()
    {
        return elements.isEmpty() ? null : elements.peek();
    }

    void add(Content c)
    {
        Element e = top();
        if (e == null)
        {
            doc.addContent(c);
        }
        else
        {
            e.addContent(c);
        }
    }

    void flush_text()
    {
        if (pending_text.length() == 0)
        {
            return;
        }
        String t = pending_text.toString();
        pending_text.setLength(0);
        Element e = top();
        if (e == null)
        {
            // Text directly under the document is dropped, like
            // whitespace between the root element and the end tag.
            return;
        }
        // The C parser trims text nodes at parse time (trim_text_node
        // -> xmq_un_quote -> xmq_trim_quote), unless --trim=none.
        if (no_trim)
        {
            e.addContent(new Text(t));
            return;
        }
        // Whitespace-only text nodes are dropped, matching C's
        // trim_text_node (is_all_xml_whitespace -> unlink).
        if (t.trim().isEmpty())
        {
            return;
        }
        String v = RawXmlParser.trim_xml_text(t);
        if (v == null || v.length() == 0 || v.trim().isEmpty())
        {
            return;
        }
        e.addContent(new Text(v));
    }

    public void text(String t) throws Exception
    {
        pending_text.append(t);
    }

    public void entityReference(String name) throws Exception
    {
        flush_text();
        EntityRef er = new EntityRef(name);
        Element e = top();
        if (e == null)
        {
            throw RawXmlParser.err("Entity reference &" + name + "; outside of an element", 0);
        }
        e.addContent(er);
    }

    public void startElement(String name, String[] anames, String[] avalues) throws Exception
    {
        flush_text();
        Element e = new RawElement(name);
        for (int k = 0; k < anames.length; k++)
        {
            e.setAttribute(new RawAttribute(anames[k], avalues[k]));
        }
        if (top() == null)
        {
            if (root != null)
            {
                throw RawXmlParser.err("Document has more than one root element", 0);
            }
            root = e;
            doc.addContent(e);
        }
        else
        {
            top().addContent(e);
        }
        elements.push(e);
    }

    public void endElement(String name) throws Exception
    {
        flush_text();
        Element e = top();
        if (e == null || !e.getName().equals(name))
        {
            throw RawXmlParser.err("Element end " + name + " does not match", 0);
        }
        elements.pop();
    }

    Document finish()
    {
        if (html && doc.getDocType() == null)
        {
            // The C html parser (libxml2) adds this doctype automatically
            // for html documents that do not provide one.
            doc.setDocType(new DocType("html PUBLIC "
                                       + "\"-//W3C//DTD HTML 4.0 Transitional//EN\""
                                       + " \"http://www.w3.org/TR/REC-html40/loose.dtd\""));
        }
        return doc;
    }
}
