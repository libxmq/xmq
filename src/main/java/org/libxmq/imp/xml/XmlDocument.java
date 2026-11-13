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
 * A document. The children are the top level nodes: the dtd (if any),
 * comments, processing instructions and the root element. Unlike many
 * java DOMs, a document can also have text and multiple elements at the
 * top level, which xmq needs to represent xmq content with multiple roots.
 */
public class XmlDocument extends XmlNode
{
    boolean html_;
    XmlDtd intSubset_;
    String version_ = "1.0";
    String encoding_;
    int standalone_ = -1;
    String url_;

    /**
     * Create an empty xml document.
     */
    public XmlDocument()
    {
        super(null);
        doc_ = this;
    }

    /**
     * Create an empty xml or html document.
     * @param html True for an html document.
     */
    public XmlDocument(boolean html)
    {
        this();
        html_ = html;
    }

    @Override
    public XmlNodeType type()
    {
        return html_ ? XmlNodeType.HTML_DOCUMENT : XmlNodeType.DOCUMENT;
    }

    /**
     * Returns true if this is an html document.
     * @return True if html.
     */
    public boolean isHtml()
    {
        return html_;
    }

    /**
     * Mark the document as html or xml.
     * @param html True for html.
     */
    public void setHtml(boolean html)
    {
        html_ = html;
    }

    /**
     * Returns the root element, ie the first top level element.
     * @return The root element or null.
     */
    public XmlElement rootElement()
    {
        for (XmlNode i = children_; i != null; i = i.next_)
        {
            if (i instanceof XmlElement e) return e;
        }
        return null;
    }

    /**
     * Set the root element, replacing any existing root element, like
     * libxml2 xmlDocSetRootElement.
     * @param e The new root.
     */
    public void setRootElement(XmlElement e)
    {
        XmlElement old = rootElement();
        if (old == e) return;
        if (old != null)
        {
            old.replaceWith(e);
        }
        else
        {
            appendRaw(e);
        }
    }

    /**
     * Returns the internal subset dtd.
     * @return The dtd or null.
     */
    public XmlDtd intSubset()
    {
        return intSubset_;
    }

    /**
     * Set the dtd, like libxml2 xmlCreateIntSubset. Unless the dtd is already
     * a child, it is added as the first child of an html document, and before
     * the first element of an xml document (or last if there is no element).
     * @param dtd The dtd.
     */
    public void setIntSubset(XmlDtd dtd)
    {
        if (intSubset_ != null && intSubset_ != dtd && intSubset_.parent_ == this)
        {
            intSubset_.unlink();
        }
        intSubset_ = dtd;
        if (dtd == null) return;
        if (dtd.parent_ == null)
        {
            if (children_ == null)
            {
                appendRaw(dtd);
            }
            else if (html_)
            {
                children_.addPrevSibling(dtd);
            }
            else
            {
                XmlNode next = children_;
                while (next != null && next.type() != XmlNodeType.ELEMENT) next = next.next_;
                if (next == null) appendRaw(dtd);
                else next.addPrevSibling(dtd);
            }
            intSubset_ = dtd;
        }
        dtd.setDocRecursive(this);
    }

    /**
     * Remember the dtd without adding it as a child, used when the caller
     * places the dtd node itself.
     * @param dtd The dtd.
     */
    public void setIntSubsetRef(XmlDtd dtd)
    {
        intSubset_ = dtd;
    }

    /**
     * Find a general entity declared in the internal subset.
     * @param name The entity name.
     * @return The declaration or null.
     */
    public XmlEntityDecl getEntity(String name)
    {
        if (intSubset_ == null) return null;
        return intSubset_.getEntity(name);
    }

    /**
     * Returns the xml version.
     * @return The version, eg 1.0.
     */
    public String version()
    {
        return version_;
    }

    /**
     * Set the xml version.
     * @param v The version.
     */
    public void setVersion(String v)
    {
        version_ = v;
    }

    /**
     * Returns the declared encoding.
     * @return The encoding or null.
     */
    public String encoding()
    {
        return encoding_;
    }

    /**
     * Set the declared encoding.
     * @param e The encoding.
     */
    public void setEncoding(String e)
    {
        encoding_ = e;
    }

    /**
     * Returns the standalone declaration, -1 if not declared, 0 for no, 1 for yes.
     * @return The standalone value.
     */
    public int standalone()
    {
        return standalone_;
    }

    /**
     * Set the standalone declaration.
     * @param s -1 if not declared, 0 for no, 1 for yes.
     */
    public void setStandalone(int s)
    {
        standalone_ = s;
    }

    /**
     * Returns the source url/name of the document.
     * @return The url or null.
     */
    public String url()
    {
        return url_;
    }

    /**
     * Set the source url/name of the document.
     * @param url The url.
     */
    public void setUrl(String url)
    {
        url_ = url;
    }

    @Override
    public String fullName()
    {
        return html_ ? "html_document" : "document";
    }
}
