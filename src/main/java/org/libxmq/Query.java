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

/**
 * The Query class implemets the basic querying methods.
 */
public class Query
{
    private Node node_;
    private XPathFactory xpath_factory_;
    private XPath xpath_;

    /**
       Build a new query from a DOM node.
       @param node The DOM node from which the paths start when querying.
    */
    public Query(Node node)
    {
        node_ = node;
        xpath_factory_ = XPathFactory.newInstance();
        xpath_ = xpath_factory_.newXPath();
    }

    /**
       Return the node from which the query starts.
       @return A DOM node.
    */
    public Node node()
    {
        return node_;
    }

    /**
     * Fetch a potentially cached xpath expression.
     * @param xpath The xpath to retrieve as an expression.
     * @throws XPathExpressionException if the xpath is invalid.
     * @return The compiled xpath expression.
     */
    protected XPathExpression getXPathExpression(String xpath) throws XPathExpressionException
    {
        return xpath_.compile(xpath);
    }

    /**
       Perform a callback for each node matching the xpath expression.
       @param xpath An xpath, for example: //book
       @param cb The callback function.
       @return The number of nodes matched.
    */
    public int forEach(String xpath, NodeCallback cb)
    {
        return 0;
    }

    /**
       Perform a single callback for a single node matching the xpath expression.
       @param xpath An xpath that is assumed to match only a single , for example: /library
       @param cb The callback function.
       @throws NotFoundException if the expected xpath was not found.
    */
    public void expect(String xpath, NodeCallback cb) throws NotFoundException
    {
    }

    /**
       Get a boolean from an xpath location. Two strings are valid 'true' and 'false'.
       Anything else will throw a DecodingException. If there is more than one match

       @param xpath An xpath that
       @throws NotFoundException if the expected xpath was not found.
       @throws DecodingException if the value was not 'true' or 'false'.
       @throws TooManyException if more than one element matched the xpath.
       @return The boolean.
    */
    public boolean getBoolean(String xpath) throws DecodingException,NotFoundException,TooManyException
    {
        String s = getString(xpath, null);
        if (s.equals("true")) return true;
        if (s.equals("false")) return false;

        throw new DecodingException("boolean", "not a boolean", s);
    }

    /**
       Get a 64 bit IEEE double floating point value from an xpath location and check that
       it complies with the restrictions.

       @param xpath Fetch the value found using this xpath.
       @param restriction The float can for example be restricted in range.
       @return A double value.
       @throws NotFoundException if the expected xpath was not found.
       @throws DecodingException if the value was not an integer or if it failed the restriction.
       @throws TooManyException if more than one element matched the xpath.
    */
    public double getDouble(String xpath, String restriction)
        throws DecodingException, NotFoundException, TooManyException
    {
        String s = getString(xpath, null);
        try
        {
            double d = Double.parseDouble(s);
            return d;
        }
        catch (NumberFormatException e)
        {
            throw new DecodingException("double", "not a double", s);
        }
    }

    /**
       Get a 32 bit IEEE double floating point value from an xpath location and check that
       it complies with the restrictions.

       @param xpath Fetch the value found using this xpath.
       @param restriction The float can for example be restricted in range.
       @throws NotFoundException if the expected xpath was not found.
       @throws DecodingException if the value was not an integer or if it failed the restriction.
       @throws TooManyException if more than one element matched the xpath.
       @return The float found.
    */
    public float getFloat(String xpath, String restriction)
        throws DecodingException, NotFoundException, TooManyException
    {
        String s = getString(xpath, null);
        try
        {
            float f = Float.parseFloat(s);
            return f;
        }
        catch (NumberFormatException e)
        {
            throw new DecodingException("double", "not a double", s);
        }
    }

    /**
       Get a 32 bit signed integer from an xpath location and check that
       it complies with the restriction.

       @param xpath Fetch the value found using this xpath.
       @param restriction The float can for example be restricted in range.
       @throws NotFoundException if the expected xpath was not found.
       @throws DecodingException if the value was not an integer or if it failed the restriction.
       @throws TooManyException if more than one element matched the xpath.
       @return The integer.
    */
    public int getInt(String xpath, String restriction) throws DecodingException, NotFoundException, TooManyException
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

    /**
       Get a 64 bit signed integer from an xpath location that complies with
       any restriction.

       @param xpath Fetch the value found using this xpath.
       @param restriction The float can for example be restricted in range.
       @throws NotFoundException if the expected xpath was not found.
       @throws DecodingException if the value was not a long integer or if it failed the restriction.
       @throws TooManyException if more than one element matched the xpath.
       @return The found long.
    */
    public long getLong(String xpath, String restriction) throws DecodingException, NotFoundException, TooManyException
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

    /**
       Get a string from an xpath location that complies with a restriction.

       @param xpath Fetch the value found using this xpath.
       @param restriction The float can for example be restricted in range.
       @throws NotFoundException if the expected xpath was not found.
       @throws DecodingException if the value failed the restriction.
       @throws TooManyException if more than one element matched the xpath.
       @return The found string.
    */
    public String getString(String xpath, String restriction) throws DecodingException, NotFoundException, TooManyException
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

    // var speed = q.optional(()->q.getInt("speed", "..."));
    // var speed = q.optional(()->q.getInt("speed", "0 <= speed <= 200"));
    // var weight = q.optional(()->q.getInt("weight", "minInclusive(value=0) maxInclusive(value=200)"));
}
