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

public class UtilParseQuote extends UtilQuote
{
    /**
     * Expects an xmq quote and removes the surrounding single or double quote chars.
     * For example:
     * '' -> (1,1)
     */
    public static Pair<Integer,Integer> findQuoteStartStop(String b, int start, int stop)
    {
        assert (stop-start) >= 2 : "Must at least two quote characters.";
        assert b.length() >= (stop-start) : "Buffer not long enogh.";
        assert b.charAt(start) == '\'' || b.charAt(start) == '"' : "Must start with single or double quote.";

        int i = start;
        int j = stop-1;
        char qc = b.charAt(i);
        while (b.charAt(i) == qc && b.charAt(j) == qc && i < j)
        {
            i++;
            j--;
        }
        return new Pair<>(i, j+1);
    }

    public static String trimQuote(String b, int start, int stop)
    {
        UtilParseQuote upq = new UtilParseQuote(b, start, stop);
        upq.analyzeForParse();
        return upq.parseQuote();
    }


    protected UtilParseQuote(String b, int start, int stop)
    {
        super(b, start, stop);
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

    public void analyzeForParse()
    {
        if (debug_) System.out.println("ANALZE >"+buffer_+"<\n"+Util.xmq_quote_as_c(buffer_, -1, -1 , true));

        lines_ = new ArrayList<>();
        char c = 0;

        if (!has_nl_)
        {
            // No newlines means that the content is quoted verbatim.
            lines_.add(new Line(buffer_start_, buffer_stop_, false, false));
            return;
        }
        findLeadingNewlines();

        if (all_leading_)
        {
            // The quote consists of only newlines and spaces. This collapses
            // to only newlines and the number of newlines will be one less than
            // what was found.
            return;
        }

        findEndingNewlines();

        boolean first_line = true;
        start_ = buffer_start_;
        if (num_leading_nl_ > 0)
        {
            // Start at the last leading nl.
            start_ = last_leading_nl_;
            // We jump into the quote, we are no longer at the first line.
            first_line = false;
        }

        stop_ = first_ending_nl_;
        if (start_ > stop_) stop_ = buffer_.length();

        int line_start = start_;
        boolean found_nl = false;
        for (int i = start_; i < stop_; ++i)
        {
            c = buffer_.charAt(i);
            if (c == '\n')
            {
                found_nl = true;
                if (i != start_)
                {
                    boolean should_trim = !first_line;
                    String ll = buffer_.substring(line_start, i);
                    if (debug_) System.out.println("ADDED LINE >"+ll+"< trim="+should_trim);
                    lines_.add(new Line(line_start, i, should_trim, true));
                    first_line = false;
                }
                line_start = i+1;
                if (line_start >= stop_) break;

                int len = -1;
                if (buffer_.charAt(line_start) == ' ')
                {
                    len = countSame(line_start, stop_);
                    if (line_start+len < stop_)
                    {
                        char n = buffer_.charAt(line_start+len);
                        // Ignore lines with all spaces for indentation calculation.
                        if (n != '\n')
                        {
                            if (len < min_indent_) min_indent_ = len;
                        }
                    }
                    i += len;
                }
                if (debug_) System.out.println("NEXT INDENT min_indent="+min_indent_);
            }
        }
        if (min_indent_ == Integer.MAX_VALUE) min_indent_ = 0;
        if (line_start < stop_)
        {
            boolean should_trim = !first_line;
            lines_.add(new Line(line_start, stop_, should_trim, false));
            String ll = buffer_.substring(line_start, stop_);
            if (debug_) System.out.println("ADDED LAST LINE >"+ll+"< trim="+should_trim);
            if (should_trim && min_indent_ == Integer.MAX_VALUE)
            {
                min_indent_ = 0;
            }
        }
        if (last_indent_ > 0 && min_indent_ > last_indent_) min_indent_ = last_indent_;
        if (debug_) System.out.println("MIN_INDENT="+min_indent_);
    }

    protected String parseQuote()
    {
        StringBuilder sb = new StringBuilder();

        for (int i = 0; i < num_leading_nl_-1; ++i) sb.append('\n');

        for (Line l : lines_)
        {
            int len = l.stop()-l.start();
            if (len < 0) {
                System.out.println("ARRGGG");
                System.exit(-1);
            }
            if (debug_) System.out.println("CONCAT LINE >"+buffer_.substring(l.start(),l.stop())+"< trim="+l.trim()+" nl="+l.nl()+" min_indent="+min_indent_+" len="+len);

            if (len >= min_indent_ && l.trim())
            {
                sb.append(buffer_.substring(l.start()+min_indent_, l.stop()));
            }
            else
            {
                sb.append(buffer_.substring(l.start(), l.stop()));
            }
            if (l.nl()) sb.append('\n');
        }
        for (int i = 0; i < num_ending_nl_-1; ++i) sb.append('\n');

        return sb.toString();
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

    public String parseInfo()
    {
        return
            "nlenl="+num_leading_nl_+
            " llenl="+last_leading_nl_+
            " nennl="+num_ending_nl_+
            " fennl="+first_ending_nl_+
            " start="+start_+
            " stop="+stop_+
            " mni="+min_indent_+
            " nlines="+lines_.size();
    }
}
