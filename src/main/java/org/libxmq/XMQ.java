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
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.EnumSet;
import java.util.EnumSet;
import javax.xml.parsers.DocumentBuilder;
import javax.xml.parsers.DocumentBuilderFactory;
import org.libxmq.imp.XMQParseIntoDOM;
import org.w3c.dom.Document;

public class XMQ
{
    private DocumentBuilderFactory factory_;
    private DocumentBuilder builder_;

    public XMQ()
    {
        factory_ = DocumentBuilderFactory.newInstance();
        try
        {
            builder_ = factory_.newDocumentBuilder();
        }
        catch (Exception e)
        {
        }
    }

    public XMQ(DocumentBuilderFactory f)
    {
        factory_ = f;
        try
        {
            builder_ = factory_.newDocumentBuilder();
        }
        catch (Exception e)
        {
        }
    }

    public Document parseFile(Path file, InputSettings is) throws IOException
    {
        String buffer = Files.readString(file, StandardCharsets.UTF_8);
        return parseBuffer(buffer, is);
    }

    public Document parseBuffer(String buffer, InputSettings is)
    {
        XMQParseIntoDOM pa = new XMQParseIntoDOM();
        pa.parse(buffer, "buffer");
        return pa.doc();
    }

    public String toXMQ(Document doc, OutputSettings os)
    {
        return "lll";
    }

    public String toXML(Document doc, OutputSettings os)
    {
        return "lll";
    }

    public String toHTML(Document doc, OutputSettings os)
    {
        return "lll";
    }

    public String toJSON(Document doc, OutputSettings os)
    {
        return "lll";
    }

    public String toCLINES(Document doc, OutputSettings os)
    {
        return "lll";
    }

    public String renderHTML(Document doc, OutputSettings os)
    {
        return "lll";
    }

    public String renderTEX(Document doc, OutputSettings os)
    {
        return "lll";
    }

    public static void main(String[] args)
    {
        System.out.println("PRUTT");
    }
}
