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

/**
 * A namespace, like the libxml2 xmlNs struct. A namespace is declared on an
 * element (the nsDef list) and referenced by elements and attributes using it.
 * A prefix of null is the default namespace.
 */
public class XmlNs
{
    String href_;
    String prefix_;

    /**
     * Create a namespace.
     * @param prefix The prefix, null for the default namespace.
     * @param href The namespace uri.
     */
    public XmlNs(String prefix, String href)
    {
        prefix_ = prefix;
        href_ = href;
    }

    /**
     * Returns the prefix.
     * @return The prefix, null for the default namespace.
     */
    public String prefix()
    {
        return prefix_;
    }

    /**
     * Returns the namespace uri.
     * @return The href.
     */
    public String href()
    {
        return href_;
    }

    /**
     * Set the namespace uri.
     * @param href The new uri.
     */
    public void setHref(String href)
    {
        href_ = href;
    }

    @Override
    public String toString()
    {
        return "xmlns" + (prefix_ == null ? "" : ":" + prefix_) + "=" + href_;
    }
}
