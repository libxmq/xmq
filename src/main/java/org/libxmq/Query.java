/* libxmq - Copyright (C) 2024-2025 Fredrik Öhrström (spdx: MIT)

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

import java.util.Optional;
import org.libxmq.*;
import org.libxmq.imp.*;
import org.w3c.dom.Document;
import org.w3c.dom.Node;
import org.w3c.dom.NodeList;

import javax.xml.xpath.XPath;
import javax.xml.xpath.XPathConstants;
import javax.xml.xpath.XPathExpression;
import javax.xml.xpath.XPathExpressionException;
import javax.xml.xpath.XPathFactory;

public class Query
{
    private Node node_;
    private XPathFactory xpath_factory_;
    private XPath xpath_;

    public Query(Node node)
    {
        node_ = node;
        xpath_factory_ = XPathFactory.newInstance();
        xpath_ = xpath_factory_.newXPath();
    }

    public Node node()
    {
        return node_;
    }

    protected XPathExpression getXPathExpression(String xpath) throws XPathExpressionException
    {
        return xpath_.compile(xpath);
    }

    public int forEach(String xpath, NodeCallback cb)
    {
        return 0;
    }

    public void expect(String xpath, NodeCallback cb) throws NotFoundException
    {
    }

    public boolean getBoolean(String xpath) throws DecodingException,NotFoundException
    {
        String s = getString(xpath, null);
        if (s.equals("true")) return true;
        if (s.equals("false")) return false;

        throw new DecodingException("boolean", "not a boolean", s);
    }

    public int getInt(String xpath, String restriction) throws DecodingException, NotFoundException
    {
        String s = getString(xpath, null);
        try
        {
            int i = Integer.parseInt(s);
            return i;
        }
        catch (NumberFormatException e)
        {
            throw new DecodingException("int", "not an integer", s);
        }
    }

    public long getLong(String xpath, String restriction) throws DecodingException, NotFoundException
    {
        String s = getString(xpath, null);
        try
        {
            long l = Long.parseLong(s);
            return l;
        }
        catch (NumberFormatException e)
        {
            throw new DecodingException("long", "not a long", s);
        }
    }

    public String getString(String xpath, String restriction) throws DecodingException, NotFoundException
    {
        try
        {
            XPathExpression expr = getXPathExpression(xpath);
            Node n = (Node)expr.evaluate(node_, XPathConstants.NODE);
            if (n == null) throw new NotFoundException("Could not find "+xpath+" below node "+Util.getXPath(node()));
            String text = n.getTextContent();
            return text;
        }
        catch (XPathExpressionException e)
        {
            throw new NotFoundException("Invalid xpath "+xpath+" below node "+Util.getXPath(node()));
        }
    }

    Node firstChild(String name)
    {
        if (node_ == null || name == null) return null;

        if (!Util.isValidElementName(name)) return null;

        NodeList children = node_.getChildNodes();
        for (int i = 0; i < children.getLength(); i++)
        {
            Node child = children.item(i);
            if (name.equals(child.getNodeName()))
            {
                return child;
            }
        }
        return null; // not found
    }

    // var speed = q.optional(q.getInt("speed", "..."));
    // var speed = q.optional(q.getInt("speed", "0 <= speed <= 200"));
    // var weight = q.optional(q.getInt("weight", "minInclusive(value=0) maxInclusive(value=200)"));
    public Optional<Integer> optional(int i) throws DecodingException
    {
        return Optional.of(-4711);
    }

    public Optional<Boolean> optional(boolean b) throws DecodingException
    {
        return Optional.of(true);
    }

    public Optional<String> optional(String s) throws DecodingException
    {
        return Optional.of(s);
    }

}
