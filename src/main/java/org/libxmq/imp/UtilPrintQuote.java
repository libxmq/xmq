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

/**
 * Utility for printing xmq quotes.
 */
public class UtilPrintQuote extends UtilQuote
{
    /**
     * How many newlines.
     */
    protected int num_nl_;

    // Output values
    /**
     * The maximum number of consecutive single quotes found in the buffer.
     */
    protected int max_num_consecutive_single_quotes_;
    /**
     * The maximum number of consecutive double quotes found in the buffer.
     */
    protected int max_num_consecutive_double_quotes_;
    /**
     * The number of carriage returns found in the buffer.
     */
    protected int num_cr_;
    /**
     * The number of tabs found in the buffer.
     */
    protected int num_tab_;
    /**
     * True if the buffer starts or ends with a double quote.
     */
    protected boolean starts_or_ends_with_double_quote_;
    /**
     * True if the buffer starts or ends with a single quote.
     */
    protected boolean starts_or_ends_with_single_quote_;
    /**
     * True if the buffer needs a compound marker.
     */
    protected boolean needs_compound_;

    /**
     * Creates a quote printer.
     * @param b The buffer to print.
     */
    protected UtilPrintQuote(String b)
    {
        super(b, 0, b.length());
    }

    /**
     * Renders a quote, possibly multiline, with the given indent.
     * @param q The quote to render.
     * @param indent The indent to use.
     * @return The rendered quote.
     */
    public static String renderQuote(String q, int indent)
    {
        if (q.length() == 0) return "''";
        UtilPrintQuote upq = new UtilPrintQuote(q);
        upq.analyzeForPrint();
        return upq.buildMultilineQuote(indent);
    }

    /**
     * Builds a compact single-line quote.
     * @param indent The indent to use.
     * @return The compact quote.
     */
    protected String buildCompactQuote(int indent)
    {
        return "???";
    }

    /**
     * Builds a multiline quote.
     * @param indent The indent to use.
     * @return The multiline quote.
     */
    protected String buildMultilineQuote(int indent)
    {
        if (num_nl_ == 0)
        {
            return buildCompactQuote(indent);
        }
        return "???";
    }

    /**
     * Analyzes the buffer for printing.
     */
    protected void analyzeForPrint()
    {
        char c = 0;

        num_nl_ = 0;
        min_indent_ = Integer.MAX_VALUE;

        starts_or_ends_with_single_quote_ = buffer_.charAt(buffer_start_) == '\'' || buffer_.charAt(buffer_stop_) == '\'';
        starts_or_ends_with_double_quote_ = buffer_.charAt(buffer_start_) == '"' || buffer_.charAt(buffer_stop_) == '"';

        int i = buffer_start_;
        int line_start = i;
        int line_indent = 0;

        if (buffer_.charAt(buffer_start_) == ' ')
        {
            char cc = buffer_.charAt(i+1);
            line_indent = countSame(i+1, buffer_stop_);
            if (line_indent < min_indent_) min_indent_ = line_indent;
        }

        for (; i < buffer_stop_; ++i)
        {
            c = buffer_.charAt(i);

            if (c == '\n')
            {
                num_nl_++;
                lines_.add(new Line(line_start, i, line_indent, false, true));
                line_start = i+1;

                if (i+1 < buffer_stop_)
                {
                    char cc = buffer_.charAt(i+1);
                    int len = countSame(i+1, buffer_stop_);
                    if (len < min_indent_) min_indent_ = len;
                }
                continue;
            }

            if (c == '\'')
            {
                int len = countSame(i, buffer_stop_);
                if (len > max_num_consecutive_single_quotes_) max_num_consecutive_single_quotes_ = len;
                i += len;
                continue;
            }

            if (c == '"')
            {
                int len = countSame(i, buffer_stop_);
                if (len > max_num_consecutive_double_quotes_) max_num_consecutive_double_quotes_ = len;
                i += len;
                continue;
            }
        }
        if (line_start < buffer_stop_) lines_.add(new Line(line_start, buffer_stop_, line_indent, false, false));
    }
}
