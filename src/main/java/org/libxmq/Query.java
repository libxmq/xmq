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

public class Query
{
    protected Node node_;

    public Query(Node node)
    {
        node_ = node;
    }

    public int forEach(String xpath, NodeCallback cb)
    {
        return 0;
    }

    public void expect(String xpath, NodeCallback cb) throws NotFoundException
    {
    }

    public int parallellForEach(String xpath, NodeCallback cb)
    {
        return 0;
    }

    public boolean getBoolean(String xpath) throws DecodingFailedException,NotFoundException
    {
        return true;
    }

    public int getInt(String xpath, String restriction) throws DecodingFailedException, NotFoundException
    {
        return -4711;
    }

    public long getLong(String xpath, String restriction) throws DecodingFailedException, NotFoundException
    {
        return -4711;
    }

    public String getString(String xpath, String restriction) throws DecodingFailedException, NotFoundException
    {
        return "-4711";
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
    public Optional<Integer> optional(int i) throws DecodingFailedException
    {
        return Optional.of(-4711);
    }

    public Optional<Boolean> optional(boolean b) throws DecodingFailedException
    {
        return Optional.of(true);
    }

    public Optional<String> optional(String s) throws DecodingFailedException
    {
        return Optional.of(s);
    }

}
