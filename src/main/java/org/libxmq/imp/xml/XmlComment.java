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
 * A comment node.
 */
public class XmlComment extends XmlNode
{
    String content_;

    /**
     * Create a comment.
     * @param content The comment text, without the comment markers.
     */
    public XmlComment(String content)
    {
        super(null);
        content_ = content;
    }

    @Override
    public XmlNodeType type()
    {
        return XmlNodeType.COMMENT;
    }

    @Override
    public String content()
    {
        return content_;
    }

    @Override
    public void setContent(String content)
    {
        content_ = content;
    }

    @Override
    public String textContent()
    {
        return content_ == null ? "" : content_;
    }

    @Override
    public String fullName()
    {
        return "comment";
    }
}
