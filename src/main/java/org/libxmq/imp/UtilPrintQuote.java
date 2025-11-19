/* libxmq - Copyright (C) 2025 Fredrik Öhrström (spdx: MIT)

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

package org.libxmq.imp;

import java.util.ArrayList;

public class UtilPrintQuote extends UtilQuote
{
    // Output values
    protected int max_num_consecutive_single_quotes_;
    protected int max_num_consecutive_double_quotes_;
    protected boolean needs_compound_;

    protected UtilPrintQuote(String b)
    {
        super(b, 0, b.length());
    }

    public static String renderQuote(String q, int indent)
    {
        UtilPrintQuote upq = new UtilPrintQuote(q);
        return upq.buildMultilineQuote(indent);
    }

    protected String buildCompactQuote(int indent)
    {
        return "???";
    }

    protected String buildMultilineQuote(int indent)
    {
        int line_start = buffer_start_;
        int num_newlines = 0;
        for (int i = buffer_start_; i < buffer_stop_; ++i)
        {
            char c = buffer_.charAt(i);
            if (c == '\n')
            {
                num_newlines++;
                lines_.add(new Line(line_start, i, false, true));
                line_start = i+1;
            }
        }
        lines_.add(new Line(line_start, buffer_stop_, false, false));

        if (num_newlines == 0)
        {
            return buildCompactQuote(indent);
        }
        return "???";
    }

    protected void analyzeForPrint(ArrayList<QuotePart> parts)
    {
        char c = 0;
        int i = buffer_start_;
        has_nl_ = false;

        min_indent_ = Integer.MAX_VALUE;
        for (; i < buffer_stop_; ++i)
        {
            c = buffer_.charAt(i);

            if (c == '\n')
            {
                has_nl_ = true;
                continue;
            }

            if (c == '\'')
            {
                int len = countSame(i, stop_);
                if (len > max_num_consecutive_single_quotes_) max_num_consecutive_single_quotes_ = len;
                i += len;
                continue;
            }
        }
    }
}
