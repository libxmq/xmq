/* libxmq - Copyright (C) 2024 Fredrik Öhrström (spdx: MIT)

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

import org.w3c.dom.Node;
import java.net.InetAddress;
import java.net.URL;

public class QueryNetwork extends Query
{
    /**
       Build a new query from a DOM node.
       @param node The DOM node from which the paths start when querying.
    */
    public QueryNetwork(Node node)
    {
        super(node);
    }

    /**
       Retrieve an internet address ipv4 or ipv6.
       @param xpath Fetch the value found using this xpath.
       @throws NotFoundException if the expected xpath was not found.
       @throws DecodingException if the value was not an integer or if it failed the restriction.
       @throws TooManyException if more than one element matched the xpath.
       @return The iternet address.
    */
    public static InetAddress getInetAddress(String xpath) throws DecodingException, NotFoundException, TooManyException
    {
        return null;
    }

    /**
       Retrieve a host name, such as mail or gateway or mycomputer.
       @param xpath Use this xpath to find the host name.
       @throws NotFoundException if the expected xpath was not found.
       @throws DecodingException if the value was not an integer or if it failed the restriction.
       @throws TooManyException if more than one element matched the xpath.
       @return The hostname.
    */
    public static String getHostname(String xpath) throws NotFoundException,DecodingException,TooManyException
    {
        return null;
    }

    /**
       Retrieve a fully qualified domain name, such as mail.mydomain.com.

       @param xpath Use this xpath to find the host name.
       @throws NotFoundException if the expected xpath was not found.
       @throws DecodingException if the value was not an integer or if it failed the restriction.
       @throws TooManyException if more than one element matched the xpath.
       @return The fqdn.
    */
    public static String getFQDN(String xpath) throws NotFoundException,DecodingException,TooManyException
    {
        return null;
    }

    /**
       Retrieve an URL, such as https://foo.bar.com?a=123

       @param xpath Use this xpath to find the host name.
       @throws NotFoundException if the expected xpath was not found.
       @throws DecodingException if the value was not an integer or if it failed the restriction.
       @throws TooManyException if more than one element matched the xpath.
       @return The url address.
    */
    public static URL getURL(String xpath) throws NotFoundException,DecodingException,TooManyException
    {
        return null;
    }
}
