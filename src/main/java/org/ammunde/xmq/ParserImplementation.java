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

class ParserImplementation
{
    ActionsXMQ actions_;
    String file_ = "";
    String buf_ = "";
    int pos_;
    int line_;
    int col_;

    ParserImplementation(ActionsXMQ a, String f, String b)
    {
        actions_ = a;
        file_ = f;
        buf_ = b;
        pos_ = 0;
        line_ = 1;
        col_ = 1;
    }

    boolean parse()
    {
        return false;
    }


    boolean isNewLine(char c)
    {
        return c == '\n';
    }

    boolean isWhiteSpace(char c)
    {
        return c == ' ' || c == '\t' || c == '\r' || c == '\n';
    }

    void eatWhiteSpace()
    {
        while (true)
        {
            if (pos_ >= buf_.length()) break;
            char c = buf_.charAt(pos_);
            if (isNewLine(c))
            {
                col_ = 1;
                line_++;
            }
            else if (!isWhiteSpace(c)) break;
            pos_++;
            col_++;
        }
    }
}
