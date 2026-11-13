/* libxmq - Copyright (C) 2023-2026 Fredrik Öhrström (spdx: MIT) */

package org.libxmq.imp;

/**
 * Removing the incidental indentation of quotes and comments, an exact port
 * of xmq_un_quote, xmq_un_comment, xmq_trim_quote and their helpers in the
 * C xmq.c and text.c.
 */
final class XmqTrim
{
    private XmqTrim()
    {
    }

    static boolean is_xml_whitespace(char c)
    {
        return c == ' ' || c == '\n' || c == '\t' || c == '\r';
    }

    /**
     * Like xmq_un_quote: optionally remove the surrounding quotes and then remove
     * the incidental indentation.
     * @param b The buffer.
     * @param start Start of the quote.
     * @param stop End of the quote.
     * @param remove_qs Remove the surrounding quotes.
     * @param is_xmq The quote is xmq, the last line indentation counts.
     * @return The content of the quote.
     */
    static String unQuote(String b, int start, int stop, boolean remove_qs, boolean is_xmq)
    {
        int j = 0;
        if (remove_qs && start < stop)
        {
            char q = b.charAt(start);
            if (q == '\'' || q == '"')
            {
                while (start + j < stop - j && b.charAt(start + j) == q && b.charAt(stop - j - 1) == q) j++;
            }
        }
        return trimQuote(b, start + j, stop - j, is_xmq, false);
    }

    /**
     * Like xmq_un_comment: remove the comment markers, the incidental indentation and trailing spaces.
     * @param b The buffer.
     * @param start Start of the comment (the slashes).
     * @param stop End of the comment.
     * @return The content of the comment.
     */
    static String unComment(String b, int start, int stop)
    {
        int i = start;
        while (i < stop && b.charAt(i) == '/') i++;
        if (i == stop || b.charAt(i) != '*')
        {
            // A single line comment //.....
            i = start + 2;
            if (i < stop && b.charAt(i) == ' ') i++;
            while (i < stop && b.charAt(stop - 1) == ' ') stop--;
            if (i > stop) i = stop;
            return trimQuote(b, i, stop, true, true);
        }
        while (i < stop && b.charAt(i) == '/') i++;
        if (i == start)
        {
            // The asterisk is first, a comment continuation. Remove the ending slashes.
            while (stop > start && b.charAt(stop - 1) == '/') stop--;
        }
        else
        {
            // A standard /* */ comment, remove the surrounding slashes.
            int j = 0;
            while (start + j < stop - j && b.charAt(start + j) == '/' && b.charAt(stop - j - 1) == '/') j++;
            start += j;
            stop -= j;
        }
        // Skip the asterisks.
        start++;
        stop--;
        // Skip a single space after the starting asterisk and before the ending asterisk.
        if (start < stop && b.charAt(start) == ' ') start++;
        if (stop - 1 >= start && b.charAt(stop - 1) == ' ') stop--;
        if (start > stop) start = stop;
        return trimQuote(b, start, stop, true, true);
    }

    static boolean check_leading_space_nl(String b, int start, int stop)
    {
        while (start < stop && b.charAt(start) == ' ') start++;
        if (start == stop) return false;
        return b.charAt(start) == '\n';
    }

    // The smallest indentation of the lines after the first (with content), -1 if no newline.
    static long calculate_incidental_indent(String b, int start, int stop)
    {
        long indent = Long.MAX_VALUE;
        int i = start;
        while (i < stop && b.charAt(i) != '\n') i++;
        if (i >= stop) return -1;
        for (;;)
        {
            i++;
            int line_start = i;
            while (i < stop && b.charAt(i) == ' ') i++;
            if (i >= stop) break;
            if (b.charAt(i) != '\n' && b.charAt(i) != '\r')
            {
                long ind = i - line_start;
                if (ind < indent) indent = ind;
            }
            while (i < stop && b.charAt(i) != '\n') i++;
            if (i >= stop) break;
        }
        return indent;
    }

    // Like has_leading_space_nl in text.c: true if there is a newline before the content.
    static boolean has_leading_space_nl(String b, int start, int stop)
    {
        int i = start;
        boolean found_nl = false;
        while (i < stop && b.charAt(i) == '\n')
        {
            i++;
            found_nl = true;
        }
        while (i < stop)
        {
            char c = b.charAt(i);
            if (c == '\n') found_nl = true;
            if (!is_xml_whitespace(c)) break;
            i++;
        }
        return found_nl;
    }

    // Like has_ending_nl_space in text.c: true if there is a newline after the content.
    static boolean has_ending_nl_space(String b, int start, int stop)
    {
        int i = stop - 1;
        boolean found_nl = false;
        while (i >= start && b.charAt(i) == '\n')
        {
            i--;
            found_nl = true;
        }
        while (i >= start)
        {
            char c = b.charAt(i);
            if (c == '\n') found_nl = true;
            if (!is_xml_whitespace(c)) break;
            i--;
        }
        return found_nl;
    }

    // Like find_line: returns {has_nl (0/1), after_last_non_space, eol}.
    static int[] find_line(String b, int start, int stop)
    {
        boolean has_nl = false;
        int lnws = start;
        int i = start;
        while (i < stop && (b.charAt(i) == ' ' || b.charAt(i) == '\t')) i++;
        while (i < stop)
        {
            char c = b.charAt(i);
            if (c == '\n' || c == '\r')
            {
                if (c == '\r' && i + 1 < b.length() && b.charAt(i + 1) == '\n') i++;
                i++;
                has_nl = true;
                break;
            }
            if (c != ' ' && c != '\t') lnws = i + 1;
            i++;
        }
        return new int[] { has_nl ? 1 : 0, lnws, i };
    }

    /**
     * Like xmq_trim_quote: remove the incidental indentation of a quote or comment.
     * @param b The buffer.
     * @param start Start of the content.
     * @param stop End of the content.
     * @param is_xmq The last line indentation counts.
     * @param is_comment The content is a comment.
     * @return The trimmed content.
     */
    static String trimQuote(String b, int start, int stop, boolean is_xmq, boolean is_comment)
    {
        int append_newlines = 0;
        long last_line_spaces = -1;

        if (has_ending_nl_space(b, start, stop))
        {
            while (stop > start)
            {
                char c = b.charAt(stop - 1);
                if (c == '\n') append_newlines++;
                if (c == ' ' && append_newlines == 0)
                {
                    if (last_line_spaces == -1) last_line_spaces = 0;
                    last_line_spaces++;
                }
                if (c != ' ' && c != '\t' && c != '\n' && c != '\r') break;
                stop--;
            }
        }
        // If there are ending newlines, one is dropped, the rest are kept.
        if (append_newlines > 0) append_newlines--;

        if (stop == start) return "\n".repeat(append_newlines);

        boolean leads_space_nl = check_leading_space_nl(b, start, stop);

        long incidental = calculate_incidental_indent(b, start, stop);
        // In C the incidental is a size_t where -1 is the largest value.
        long incidental_cmp = incidental == -1 ? Long.MAX_VALUE : incidental;
        long last_cmp = last_line_spaces == -1 ? Long.MAX_VALUE : last_line_spaces;
        if (is_xmq && last_cmp < incidental_cmp)
        {
            incidental = last_line_spaces;
            if (!leads_space_nl)
            {
                incidental++;
                if (is_comment) incidental += 2;
            }
        }

        if (incidental == -1)
        {
            // No newline was found, do not trim, but re-add the ending newlines.
            return b.substring(start, stop) + "\n".repeat(append_newlines);
        }

        int prepend_newlines = 0;
        boolean first_line = true;
        int[] line = find_line(b, start, stop);

        if (has_leading_space_nl(b, start, stop))
        {
            // There is no first line, start removing incidental indentation immediately.
            first_line = false;
            start = line[2];
            int i = start;
            while (i < stop)
            {
                char c = b.charAt(i);
                if (c == '\n')
                {
                    start = i + 1;
                    prepend_newlines++;
                }
                else if (c != '\r') break;
                i++;
            }
        }

        StringBuilder o = new StringBuilder();
        o.append("\n".repeat(prepend_newlines));
        int i = start;
        while (i < stop)
        {
            line = find_line(b, i, stop);
            boolean has_nl = line[0] == 1;
            int after_last_non_space = line[1];
            int eol = line[2];
            if (first_line)
            {
                first_line = false;
            }
            else if (b.charAt(i) == ' ')
            {
                // Skip the incidental indentation, empty lines do not count.
                long n = incidental;
                while (n > 0 && i < stop && b.charAt(i) == ' ')
                {
                    i++;
                    n--;
                }
            }
            // Copy the content, but not line ending whitespace.
            if (i < after_last_non_space) o.append(b, i, after_last_non_space);
            if (after_last_non_space > i) i = after_last_non_space;
            if (has_nl)
            {
                o.append('\n');
            }
            else
            {
                // The final line has no newline, copy any ending spaces as well.
                if (i < eol) o.append(b, i, eol);
            }
            i = eol;
        }
        o.append("\n".repeat(append_newlines));
        return o.toString();
    }
}
