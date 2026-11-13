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
 * A processing instruction, eg &lt;?target data?&gt;
 */
public class XmlPI extends XmlNode
{
    String content_;

    /**
     * Create a processing instruction.
     * @param target The pi target.
     * @param content The pi data, can be null.
     */
    public XmlPI(String target, String content)
    {
        super(target);
        content_ = content;
    }

    @Override
    public XmlNodeType type()
    {
        return XmlNodeType.PI;
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
}
