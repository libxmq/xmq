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

import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Optional;

import org.libxmq.imp.xml.XPath;
import org.libxmq.imp.xml.XPathException;
import org.libxmq.imp.xml.XmlAttr;
import org.libxmq.imp.xml.XmlElement;
import org.libxmq.imp.xml.XmlNode;
import org.libxmq.imp.xml.XmlNodeType;

import org.libxmq.imp.Util;

/**
 * The Query class implemets the basic querying methods.
 */
public class Query
{
    private static final Map<String, XPath> xpath_cache_ = new HashMap<>();

    private Object node_;

    /**
       Build a new query from a DOM node.
       @param node The DOM node from which the paths start when querying.
    */
    public Query(Object node)
    {
        node_ = node;
    }

    /**
       Return the node from which the query starts.
       @return A DOM node.
    */
    public Object node()
    {
        return node_;
    }

    /**
     * Fetch a potentially cached compiled xpath expression.
     * @param xpath The xpath to retrieve as an expression.
     * @return The compiled xpath expression.
     * @throws NotFoundException if the xpath is invalid.
     */
    protected XPath getPath(String xpath) throws NotFoundException
    {
        try
        {
            XPath p = xpath_cache_.get(xpath);
            if (p == null)
            {
                p = XPath.compile(xpath);
                xpath_cache_.put(xpath, p);
            }
            return p;
        }
        catch (XPathException e)
        {
            throw new NotFoundException("Invalid xpath "+xpath+" below node "+Util.getXPath(node()));
        }
    }

    /**
       Evaluate an xpath against the query node, converting xpath exceptions
       into xmq exceptions.
       @param xpath The xpath to evaluate.
       @return The nodes matching the xpath.
       @throws NotFoundException on xpath errors.
    */
    private List<XmlNode> select(String xpath) throws NotFoundException
    {
        try
        {
            return getPath(xpath).selectNodes((XmlNode)node_);
        }
        catch (XPathException e)
        {
            throw new NotFoundException("XPath error for "+xpath+" below node "+Util.getXPath(node())+": "+e.getMessage());
        }
    }

    /**
       Perform a callback for each node matching the xpath expression.
       @param xpath An xpath, for example: //book
       @param cb The callback function.
       @return The number of nodes matched.
    */
    public int forEach(String xpath, NodeCallback cb)
    {
        try
        {
            List<XmlNode> nodes = select(xpath);
            int matched = 0;
            for (XmlNode c : nodes)
            {
                matched++;
                if (cb.invoke(c) == Proceed.STOP)
                {
                    break;
                }
            }
            return matched;
        }
        catch (NotFoundException e)
        {
            return 0;
        }
    }

    /**
       Perform a single callback for a single node matching the xpath expression.
       @param xpath An xpath that is assumed to match only a single , for example: /library
       @param cb The callback function.
       @throws NotFoundException if the expected xpath was not found.
    */
    public void expect(String xpath, NodeCallback cb) throws NotFoundException
    {
        List<XmlNode> nodes = select(xpath);
        if (nodes.isEmpty())
        {
            throw new NotFoundException("Could not find "+xpath+" below node "+Util.getXPath(node()));
        }
        cb.invoke(nodes.get(0));
    }

    /**
       Fetches a single element from the xpath.
       @param xpath An xpath that is assumed to match only a single , for example: /library
       @throws NotFoundException if the expected xpath was not found.
       @throws TooManyException  if more than one element matched the xpath.
       @return The found element.
    */
    public XmlElement element(String xpath) throws NotFoundException, TooManyException
    {
        List<XmlNode> nodes = select(xpath);
        if (nodes.isEmpty())
        {
            throw new NotFoundException("Could not find "+xpath+" below node "+Util.getXPath(node()));
        }
        if (nodes.size() > 1)
        {
            throw new TooManyException("Found "+nodes.size()+" matches for "+xpath+" below node "+Util.getXPath(node()));
        }
        XmlNode c = nodes.get(0);
        if (c instanceof XmlElement e)
        {
            return e;
        }
        throw new NotFoundException("Match "+xpath+" is not an element below node "+Util.getXPath(node()));
    }

    /**
       Fetches a single optional element from the xpath.
       @param xpath An xpath that is assumed to match only a single , for example: /library
       @throws TooManyException  if more than one element matched the xpath.
       @return The found element.
    */
    public Optional<XmlElement> optionalElement(String xpath) throws TooManyException
    {
        try
        {
            XmlElement e = element(xpath);
            return Optional.of(e);
        }
        catch (NotFoundException e)
        {
            return Optional.empty();
        }
    }

    /**
       Get a boolean from an xpath location. Two strings are valid 'true' and 'false'.
       Anything else will throw a DecodingException.

       @param xpath An xpath that is used to find the boolean.
       @throws NotFoundException if the expected xpath was not found.
       @throws DecodingException if the value was not 'true' or 'false'.
       @throws TooManyException if more than one element matched the xpath.
       @return The boolean.
    */
    public boolean getBoolean(String xpath) throws DecodingException,NotFoundException,TooManyException
    {
        String s = getString(xpath, "");
        if (s.equals("true")) return true;
        if (s.equals("false")) return false;

        throw new DecodingException("boolean", "not a boolean", s);
    }

    /**
       Get an optional boolean from an xpath location. Two strings are valid 'true' and 'false'.
       Anything else will throw a DecodingException.

       @param xpath An xpath that finds the desired boolean.
       @throws DecodingException if the value was not 'true' or 'false'.
       @throws TooManyException if more than one element matched the xpath.
       @return The optional boolean.
    */
    public Optional<Boolean> getOptionalBoolean(String xpath) throws DecodingException,TooManyException
    {
        try
        {
            boolean b = getBoolean(xpath);
            return Optional.of(b);
        }
        catch (NotFoundException e)
        {
            return Optional.empty();
        }
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
        String s = getString(xpath, "");

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
       Get an optional 64 bit IEEE double floating point value from an xpath location and check that
       it complies with the restrictions.

       @param xpath Fetch the value found using this xpath.
       @param restriction The float can for example be restricted in range.
       @return A double value.
       @throws DecodingException if the value was not an integer or if it failed the restriction.
       @throws TooManyException if more than one element matched the xpath.
    */
    public Optional<Double> getOptionalDouble(String xpath, String restriction)
        throws DecodingException, TooManyException
    {
        try
        {
            double d = getDouble(xpath, restriction);
            return Optional.of(d);
        }
        catch (NotFoundException e)
        {
            return Optional.empty();
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
        String s = getString(xpath, "");

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
       Get an optional 32 bit IEEE double floating point value from an xpath location and check that
       it complies with the restrictions.

       @param xpath Fetch the value found using this xpath.
       @param restriction The float can for example be restricted in range.
       @throws DecodingException if the value was not an integer or if it failed the restriction.
       @throws TooManyException if more than one element matched the xpath.
       @return The float found.
    */
    public Optional<Float> getOptionalFloat(String xpath, String restriction)
        throws DecodingException, TooManyException
    {
        try
        {
            float f = getFloat(xpath, restriction);
            return Optional.of(f);
        }
        catch (NotFoundException e)
        {
            return Optional.empty();
        }
    }

    /**
       Get a 32 bit signed integer from an xpath location and check that
       it complies with the restriction.

       @param xpath Fetch the value found using this xpath.
       @param restriction The float can for example be restricted in range.
       @throws NotFoundException if the expected xpath was not found.
       @throws DecodingException if the value was not an integer.
       @throws TooManyException if more than one element matched the xpath.
       @return The integer.
    */
    public int getInt(String xpath, String restriction) throws DecodingException, NotFoundException, TooManyException
    {
        String s = getString(xpath, "");

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
       Get an optional 32 bit signed integer from an xpath location and check that
       it complies with the restriction.

       @param xpath Fetch the value found using this xpath.
       @param restriction The float can for example be restricted in range.
       @throws DecodingException if the value was not an integer.
       @throws TooManyException if more than one element matched the xpath.
       @return The integer.
    */
    public Optional<Integer> getOptionalInt(String xpath, String restriction)
        throws DecodingException, TooManyException
    {
        try
        {
            int i = getInt(xpath, restriction);
            return Optional.of(i);
        }
        catch (NotFoundException e)
        {
            return Optional.empty();
        }
    }

    /**
       Get a 64 bit signed integer from an xpath location that complies with
       any restriction.

       @param xpath Fetch the value found using this xpath.
       @param restriction The float can for example be restricted in range.
       @throws NotFoundException if the expected xpath was not found.
       @throws DecodingException if the value was not a long integer.
       @throws TooManyException if more than one element matched the xpath.
       @return The found long.
    */
    public long getLong(String xpath, String restriction) throws DecodingException, NotFoundException, TooManyException
    {
        String s = getString(xpath, "");

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
       Get an optional 64 bit signed integer from an xpath location that complies with
       any restriction.

       @param xpath Fetch the value found using this xpath.
       @param restriction The float can for example be restricted in range.
       @throws DecodingException if the value was not a long integer.
       @throws TooManyException if more than one element matched the xpath.
       @return The found long.
    */
    public Optional<Long> getOptionalLong(String xpath, String restriction)
        throws DecodingException, TooManyException
    {
        try
        {
            long l = getLong(xpath, restriction);
            return Optional.of(l);
        }
        catch (NotFoundException e)
        {
            return Optional.empty();
        }
    }

    /**
       Get a string from an xpath location that complies with a restriction.

       @param xpath Fetch the value found using this xpath.
       @param restriction The value can for example be restricted.
       @throws NotFoundException if the expected xpath was not found.
       @throws DecodingException if the value failed the restriction.
       @throws TooManyException if more than one element matched the xpath.
       @return The found string.
    */
    public String getString(String xpath, String restriction) throws DecodingException, NotFoundException, TooManyException
    {
        List<XmlNode> nodes = select(xpath);
        if (nodes.isEmpty())
        {
            throw new NotFoundException("Could not find "+xpath+" below node "+Util.getXPath(node()));
        }
        XmlNode c = nodes.get(0);
        String text;
        if (c instanceof XmlElement e)
        {
            // The direct text content of the element.
            StringBuilder sb = new StringBuilder();
            for (XmlNode i = e.firstChild(); i != null; i = i.next())
            {
                if (i.type() == XmlNodeType.TEXT || i.type() == XmlNodeType.CDATA) sb.append(i.content());
            }
            text = sb.toString();
        }
        else if (c instanceof XmlAttr a)
        {
            text = a.value();
        }
        else
        {
            text = c.textContent();
        }
        return text;
    }

    /**
       Get an optional string from an xpath location that complies with a restriction.

       @param xpath Fetch the value found using this xpath.
       @param restriction The value can for example be restricted.
       @throws DecodingException if the value failed the restriction.
       @throws TooManyException if more than one element matched the xpath.
       @return The found string.
    */
    public Optional<String> getOptionalString(String xpath, String restriction)
        throws DecodingException, TooManyException
    {
        try
        {
            String s = getString(xpath, restriction);
            return Optional.of(s);
        }
        catch (NotFoundException e)
        {
            return Optional.empty();
        }
    }

    XmlNode firstChild(String name)
    {
        if (!(node_ instanceof XmlNode p) || name == null)
        {
            return null;
        }
        if (!Util.isValidElementName(name))
        {
            return null;
        }
        for (XmlNode child = p.firstChild(); child != null; child = child.next())
        {
            if (child instanceof XmlElement el && name.equals(el.name()))
            {
                return el;
            }
        }
        return null; // not found
    }

    /**
       Fetches a single element and wraps it inside a query from the xpath.
       @param xpath An xpath that is assumed to match only a single , for example: /library
       @throws NotFoundException if the expected xpath was not found.
       @throws TooManyException  if more than one element matched the xpath.
       @return Query element.
    */
    public Query query(String xpath) throws NotFoundException, TooManyException
    {
        XmlElement e = element(xpath);
        return new Query(e);
    }

}
