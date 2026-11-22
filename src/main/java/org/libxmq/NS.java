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

import org.libxmq.imp.xml.XmlDomApi;

/**
   The namespace of an element added with XMQ.addElement etc, like XMQNS in xmq.h.
   The uri can be prefixed with {prefix}, eg NS.here("{drv}urn:myapp:driver").
   <pre>
   NS.NONE                   No namespace.
   NS.PARENT                 The same namespace as the parent.
   NS.here(uri)              Declare the namespace on the element.
   NS.ancestor(uri)          Use the namespace declared on an ancestor. If no ancestor
                             declares it, it is declared on the root element, with a
                             generated prefix ns1, ns2... if no prefix is given.
   </pre>
*/
public final class NS
{
    /** No namespace, like NS_NONE. */
    public static final NS NONE = new NS(XmlDomApi.NS_NONE, null);
    /** The same namespace as the parent, like NS_PARENT. */
    public static final NS PARENT = new NS(XmlDomApi.NS_PARENT, null);

    private final int action;
    private final String uri;

    private NS(int action, String uri)
    {
        this.action = action;
        this.uri = uri;
    }

    /**
       Declare the namespace on the element, like NS_HERE.
       @param uri The namespace uri, eg urn:cargo or {c}urn:cargo for the prefix c.
       @return The namespace.
     */
    public static NS here(String uri)
    {
        return new NS(XmlDomApi.NS_HERE, uri);
    }

    /**
       Use the namespace declared on an ancestor, like NS_ANCESTOR.
       @param uri The namespace uri, eg urn:cargo or {c}urn:cargo for the prefix c.
       @return The namespace.
     */
    public static NS ancestor(String uri)
    {
        return new NS(XmlDomApi.NS_ANCESTOR, uri);
    }

    int action()
    {
        return action;
    }

    String uri()
    {
        return uri;
    }
}
