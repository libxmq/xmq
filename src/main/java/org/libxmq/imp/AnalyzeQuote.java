
package org.libxmq.imp;

import java.util.ArrayList;

public class AnalyzeQuote
{
    /**
     * We can analyze part of a larger buffer, typically the whole xmq source file.
     * Buffer_ points to the source file content.
     */
    private String buffer_;
    /**
     * The buffer_start_ points to where in the larger buffer_ we find the quote to be parsed.
     */
    private int buffer_start_;
    /**
     * The buffer_stop_ points to just after the quote to be parsed.
     */
    private int buffer_stop_;

    // Working values
    private int current_indent_;
    private int min_indent_ = Integer.MAX_VALUE;
    private int max_indent_;

    /**
     * Stores the number of leading newline characters found before the first line with non-space characters.
     */
    private int num_leading_nl_;
    /**
     * Stores the index of the last leading newline character after which the line contains non-space characters.
     * Is -1 if no leading newline is found.
     */
    private int last_leading_nl_;
    /**
     * Stores the number of ending newline characters found after the last line with non-space characters.
     */
    private int num_ending_nl_;
    /**
     * Stores the index of the first ending newline characters after which lines do not contain non-space characters.
     * Is -1 if no such ending newline is found.
     */
    private int first_ending_nl_;
    /**
     * Start stores the index either 0, if no leading newlines are found or
     * start stores the last_leading_nl+1, ie the first character of the first line with non-space characters.
     */
    private int start_;
    /**
     * Stop stores the index either q_.length(), if no ending newlines are found or
     * stop stores the first_ending_nl, ie after the end of the last line with non-space characters.
     */
    private int stop_;

    // Output values
    private int incidental_indentation_;
    private int max_num_consecutive_single_quotes_;
    private int max_num_consecutive_double_quotes_;
    private boolean needs_compound_;

    public AnalyzeQuote(String b, int start, int stop)
    {
        buffer_ = b;
        buffer_start_ = start;
        buffer_stop_ = stop;
    }

    /**
     * Look at the char at index start and count how many identical chars there are forward.
     * Used to count identical quotes in a sequence or count number of spaces used for indentation.
     */
    private int countSame(int start, int stop)
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
    private void findLeadingNewlines()
    {
        int last_nl = -1;
        int num_nl = 0;
        for (int i = buffer_start_; i < buffer_stop_; ++i)
        {
            char c = buffer_.charAt(i);
            if (c != ' ' && c != '\n') break;
            if (c == '\n')
            {
                last_nl = i;
                num_nl++;
            }
        }
        last_leading_nl_ = last_nl;
        num_leading_nl_ = num_nl;
    }

    private void findEndingNewlines()
    {
        int first_nl = -1;
        int num_nl = 0;
        for (int i = buffer_stop_-1; i >= buffer_start_; --i)
        {
            char c = buffer_.charAt(i);
            if (c != ' ' && c != '\n') break;
            if (c == '\n')
            {
                first_nl = i;
                num_nl++;
            }
        }
        first_ending_nl_ = first_nl;
        num_ending_nl_ = num_nl;
    }

    public void analyzeForParse()
    {
        char c = 0;

        findLeadingNewlines();
        findEndingNewlines();

        start_ = last_leading_nl_+1;
        stop_ = first_ending_nl_;
        if (start_ > stop_) stop_ = buffer_.length();

        for (int i = start_; i < stop_; ++i)
        {
            c = buffer_.charAt(i);

            if (i == start_ && buffer_.charAt(i) == ' ')
            {
                int len = countSame(i, stop_);
                if (len < min_indent_) min_indent_ = len;
                if (len > max_indent_) max_indent_ = len;
                i += len;
            }
            else if (c == '\n' && i+1 < stop_ && buffer_.charAt(i+1) == ' ')
            {
                int len = countSame(i+1, stop_);
                if (len < min_indent_) min_indent_ = len;
                if (len > max_indent_) max_indent_ = len;
                i += len;
            }
        }
    }

    public String parseQuote()
    {
        StringBuilder sb = new StringBuilder();

        analyzeForParse();

        for (int i = 0; i < num_leading_nl_-1; ++i) sb.append('\n');
        for (int i = start_; i < stop_; ++i)
        {
            char c = buffer_.charAt(i);

            if (i == start_ && c == ' ')
            {
                i += min_indent_-1; // Counter the ++i
            }
            else if (c == '\n' && i+1 < stop_ && buffer_.charAt(i+1) == ' ')
            {
                sb.append(c);
                i += min_indent_; // The ++i eats the newline.
            }
            else
            {
                sb.append(c);
            }
        }
        for (int i = 0; i < num_ending_nl_-1; ++i) sb.append('\n');

        return sb.toString();
    }

    public void analyzeForPrint(ArrayList<QuotePart> parts)
    {
        char c = 0;
        int i = buffer_start_;

        for (; i < buffer_stop_; ++i)
        {
            c = buffer_.charAt(i);

            if (c == '\'')
            {
                int len = countSame(i, stop_);
                if (len > max_num_consecutive_single_quotes_) max_num_consecutive_single_quotes_ = len;
                i += len;
                continue;
            }

            if (c == '"')
            {
                int len = countSame(i, stop_);
                if (len > max_num_consecutive_double_quotes_) max_num_consecutive_double_quotes_ = len;
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
            " mxi="+max_indent_;
    }
}
