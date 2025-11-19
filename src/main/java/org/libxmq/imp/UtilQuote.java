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

public class UtilQuote
{
    protected final static boolean debug_ = false;
    /**
     * We analyze part of a larger buffer, typically the whole xmq source file.
     * The part is copied into this buffer_ and any CRLF is converted to LF in the process.
     */
    protected String buffer_;
    /**
     * The buffer_start_ points to where in the larger buffer_ we find the quote to be parsed.
     */
    protected int buffer_start_;
    /**
     * The buffer_stop_ points to just after the quote to be parsed.
     */
    protected int buffer_stop_;
    /**
     * The minimum indentation found.
     */
    protected int min_indent_ = Integer.MAX_VALUE;
    /**
     * Stores start,stop,nl for each line found inside a quote.
     */
    protected ArrayList<Line> lines_;
    /**
     * Has newlines.
     */
    protected boolean has_nl_;
    /**
     * Stores the number of leading newline characters found before the first line with non-space characters.
     */
    protected int num_leading_nl_;
    /**
     * Stores the index of the last leading newline character after which the line contains non-space characters.
     * Is -1 if no leading newline is found.
     */
    protected int last_leading_nl_;
    /**
     * Set to true if the quote only consists of newlines and spaces.
     */
    protected boolean all_leading_;
    /**
     * Stores the number of ending newline characters found after the last line with non-space characters.
     */
    protected int num_ending_nl_;
    /**
     * Stores the index of the first ending newline characters after which lines do not contain non-space characters.
     * Is -1 if no such ending newline is found.
     */
    protected int first_ending_nl_;
    /**
     * While looking for the first ending newline, remember the last indent.
     * This indent influences the incidental indentation as well.
     */
    protected int last_indent_;
    /**
     * Start stores the index either buffer_start_, if no leading newlines are found or
     * start stores the last_leading_nl+1, ie the first character of the first line with non-space characters.
     */
    protected int start_;
    /**
     * Stop stores the index either buffer_stop_, if no ending newlines are found or
     * stop stores the first_ending_nl, ie after the end of the last line with non-space characters.
     */
    protected int stop_;

    // Output values
    protected int max_num_consecutive_single_quotes_;
    protected int max_num_consecutive_double_quotes_;
    protected boolean needs_compound_;

    protected UtilQuote(String b, int start, int stop)
    {
        StringBuilder sb = new StringBuilder();
        for (int i = start; i < stop; ++i)
        {
            char c = b.charAt(i);
            if (c == '\r')
            {
                if (i+1 < stop && b.charAt(i+1) == '\n')
                {
                    // Skip the CR in CRLF
                    i++;
                }
                // A single CR translates into an LF.
                sb.append('\n');
                if (!has_nl_) has_nl_ = true;
            }
            else if (c == '\n' && !has_nl_)
            {
                has_nl_ = true;
                sb.append(c);
            }
            else sb.append(c);
        }

        buffer_ = sb.toString();
        buffer_start_ = 0;
        buffer_stop_ = buffer_.length();
    }

    /**
     * Look at the char at index start and count how many identical chars there are forward.
     * Used to count identical quotes in a sequence or count number of spaces used for indentation.
     */
    protected int countSame(int start, int stop)
    {
        int i = start;
        char c = buffer_.charAt(i);
        for (i++; i < stop; i++)
        {
            char cc = buffer_.charAt(i);
            if (cc != c) return i-start;
        }
        return i-start;
    }

    /**
     * Scans q_ for leading newline ('\n') and space (' ') characters.
     * <p>
     * The method identifies sequences of newlines and spaces at the start of the string
     * and determines the index of the last newline that precedes a line with non-space content.
     * </p>
     *
     * <h4>Example</h4>
     * <pre>{@code
     * q_ = " \n  \n   Hello World";
     * findLeadingNewlines();
     *
     * // num_leading_nl_ = 2
     * // last_leading_nl_ = 4
     * }</pre>
     */
    protected void findLeadingNewlines()
    {
        int last_nl = -1;
        int num_nl = 0;
        int i = buffer_start_;
        for (; i < buffer_stop_; ++i)
        {
            char c = buffer_.charAt(i);
            if (c != ' ' && c != '\n') break;
            if (c == '\n')
            {
                last_nl = i;
                num_nl++;
            }
        }
        if (i == buffer_stop_) all_leading_ = true;
        last_leading_nl_ = last_nl;
        num_leading_nl_ = num_nl;
    }

    protected void findEndingNewlines()
    {
        int first_nl = -1;
        int num_nl = 0;
        last_indent_ = -1;
        for (int i = buffer_stop_-1; i >= buffer_start_; --i)
        {
            char c = buffer_.charAt(i);
            if (c != ' ' && c != '\n') break;
            if (c == '\n')
            {
                first_nl = i;
                num_nl++;
                if (last_indent_ == -1)
                {
                    last_indent_ = buffer_stop_-i-1;
                    if (debug_) System.out.println("LAST INDENT "+last_indent_);
                }
            }
        }
        first_ending_nl_ = first_nl;
        num_ending_nl_ = num_nl;
    }

}
