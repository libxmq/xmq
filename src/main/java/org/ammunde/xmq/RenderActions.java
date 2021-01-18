/*
 Copyright (c) 2020 Fredrik Öhrström

 MIT License

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in all
 copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 SOFTWARE.
*/

package org.ammunde.xmq;

interface RenderActions
{
    Object root();
    Object firstNode(Object node);
    Object nextSibling(Object node);
    boolean hasAttributes(Object node);
    Object firstAttribute(Object node);
    Object nextAttribute(Object attr);
    Object parent(Object node);
    boolean isNodeData(Object node);
    boolean isNodeComment(Object node);
    boolean isNodeCData(Object node);
    boolean isNodeDocType(Object node);
    boolean isNodeDeclaration(Object node);
    void loadName(Object node, Str name);
    void loadValue(Object node, Str data);
}
