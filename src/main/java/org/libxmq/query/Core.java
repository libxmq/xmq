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

package org.libxmq.query;

import org.libxmq.XMQDoc;
import org.w3c.dom.Node;
import java.net.InetAddress;
import java.net.URL;

public class Core
{
    public static boolean getBoolean(XMQDoc doq, String xpath)
    {
        return true;
    }

    public static int getInt(XMQDoc doq, String xpath)
    {
        return -4711;
    }

    public static int getIntRel(XMQDoc doq,
                                String xpath,
                                Node node,
                                String restriction)
    {
        // "minInclusive(value=0) maxInclusive(value=2000000)"
        return 0;
    }

    public static String getStringRel(XMQDoc doq,
                                      String xpath,
                                      Node node,
                                      String restriction)
    {
        // "pattern(value='[A-Z]{3} [0-9]{3})");
        return "";
    }

    public static String getHostname(XMQDoc doq, String xpath)
    {
        return null;
    }

    public static InetAddress getFQDNRel(XMQDoc doq, String xpath, Node n)
    {
        return null;
    }

    public static InetAddress getFQDN(XMQDoc doq, String xpath)
    {
        return null;
    }

    public static URL getURL(XMQDoc doq, String xpath, Node n)
    {
        return null;
    }

    public static URL getURLRel(XMQDoc doq, String xpath, Node n)
    {
        return null;
    }

}
