/* libxmq - Copyright (C) 2023-2026 Fredrik Öhrström (spdx: MIT)

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

import org.libxmq.ParseException;
import org.libxmq.ParseErrorCode;

/**
 * Base class for xmq parsers.
 */
public abstract class XMQParser extends XMQParseCallbacks
{
    /** The parsers recurse for every nested xmq body or json object or array, deeper
        nesting is an error (XMQ_ERROR_TOO_DEEP) instead of a stack overflow. Like
        XMQ_MAX_NESTING_DEPTH in C. */
    public static final int MAX_NESTING_DEPTH = 10000;

    /** The depth of the xmq bodies, or the json objects and arrays, being parsed. */
    int nesting_depth_;

    /**
     * Creates the parser.
     */
    protected XMQParser()
    {
    }

    String source_; // Used for generating error messages.
    String buffer_; // XMQ source to parse.
    String implicit_root_; // Assume that this is the first element name

    int buffer_len_; // Buffer length.

    // These three: i_, line_, col_, are only updated using the increment function.
    int i_; // Current parsing position.
    int line_start_; // Position of most recent \n found.
    int line_; // Current line.
    int col_; // Current col.

    ParseErrorCode error_nr_;
    String generated_error_msg;

    boolean simulated; // When true, this is generated from JSON parser to simulate an xmq element name.

    // These are used for better error reporting.
    int last_body_start_;
    int last_body_start_line_;
    int last_body_start_col_;

    int last_attr_start_;
    int last_attr_start_line_;
    int last_attr_start_col_;

    int last_quote_start_;
    int last_quote_start_line_;
    int last_quote_start_col_;

    int last_compound_start_;
    int last_compound_start_line_;
    int last_compound_start_col_;

    int last_comment_start_;
    int last_comment_start_line_;
    int last_comment_start_col_;

    int last_equals_start_;
    int last_equals_start_line_;
    int last_equals_start_col_;

    // A quote that perhaps ended too early, eg 'There's a man.', -1 if none (like C).
    int last_suspicios_quote_end_ = -1;
    int last_suspicios_quote_end_line_;
    int last_suspicios_quote_end_col_;

    /**
     * Parses an xmq buffer.
     * @param buf The xmq source buffer to parse.
     * @param source A description of where the buffer comes from, for error messages.
     * @return True if the input ends with a single root element.
     * @throws ParseException If the input cannot be parsed.
     */
    public boolean parse(String buf, String source) throws ParseException
    {
        source_ = source;
        buffer_ = buf;
        buffer_len_ = buf.length();
        i_ = 0;
        line_ = 1;
        col_ = 1;

        line_start_ = 0;
        nesting_depth_ = 0;
        last_suspicios_quote_end_ = -1;
        try
        {
            setup();
            parse_xmq();
            if (i_ < buffer_len_)
            {
                // Like xmqTokenizeBuffer in C: parse_xmq returned at a } on the top level.
                throw error_here(ParseErrorCode.XMQ_ERROR_UNEXPECTED_CLOSING_BRACE);
            }
        }
        catch(XMQRuntimeException e)
        {
            throw e.getParseException();
        }
        catch(Exception pe)
        {
            pe.printStackTrace();
            return false;
        }
        return true;
    }

    void setup()
    {
    }

    void increment(char c)
    {
        assert(c == buffer_.charAt(i_));
        // Like C the column counts characters (code points), the second half of a
        // surrogate pair does not count.
        if (!Character.isLowSurrogate(c)) col_++;
        if (c == '\n')
        {
            line_start_ = i_+1;
            line_++;
            col_ = 1;
        }
        i_++;
    }

    /** Like is_xmq_doctype_start in C: !DOCTYPE followed by = or whitespace. */
    private boolean is_xmq_doctype_start()
    {
        int i = i_;
        if (buffer_.charAt(i) != '!') return false;
        if (i + 8 > buffer_len_) return false;
        if (!buffer_.startsWith("!DOCTYPE", i)) return false;
        // Ooups, !DOCTYPE must have some value.
        if (i + 8 == buffer_len_) return false;
        char c = buffer_.charAt(i + 8);
        // !DOCTYPE= or !DOCTYPE = etc
        return c == '=' || c == ' ' || c == '\t' || c == '\n' || c == '\r';
    }

    /** Like is_xmq_pi_start in C: a ? followed by at least one character, eg ?x */
    private boolean is_xmq_pi_start()
    {
        return buffer_.charAt(i_) == '?' && i_ + 2 <= buffer_len_;
    }

    /** A parse error at the current position. */
    XMQRuntimeException error_here(ParseErrorCode code)
    {
        return error_at(code, i_, line_, col_);
    }

    /**
     * A parse error at a position, printed like the C xmq (xmqTokenizeBuffer and
     * generate_state_error_message): an unexpected character after a quote that perhaps
     * ended too early first gets a warning about it.
     * @param code The error.
     * @param pos The position of the error in the buffer.
     * @param line The line of the position.
     * @param col The column of the position.
     * @return The exception to throw.
     */
    XMQRuntimeException error_at(ParseErrorCode code, int pos, int line, int col)
    {
        error_nr_ = code;
        ParseException warning = null;
        if (code == ParseErrorCode.XMQ_ERROR_INVALID_CHAR && last_suspicios_quote_end_ >= 0)
        {
            warning = state_error(ParseErrorCode.XMQ_WARNING_QUOTES_NEEDED, last_suspicios_quote_end_,
                                  last_suspicios_quote_end_line_, last_suspicios_quote_end_col_, null);
        }
        return new XMQRuntimeException(state_error(code, pos, line, col, warning));
    }

    /**
     * Like generate_state_error_message in C: the line of the position (at most 1024 utf8
     * bytes) and the number of utf8 bytes before the position in the line.
     */
    private ParseException state_error(ParseErrorCode code, int pos, int line, int col, ParseException warning)
    {
        String b = buffer_;
        int line_length = 0;
        int indent = 0;
        int line_start = pos;
        while (line_start > 0 && b.charAt(line_start - 1) != '\n' && line_length < 1024)
        {
            // Step back one code point.
            int k = line_start - 1;
            if (Character.isLowSurrogate(b.charAt(k)) && k > 0 && Character.isHighSurrogate(b.charAt(k - 1))) k--;
            int n = utf8_length(b, k, line_start);
            line_start = k;
            line_length += n;
            indent += n;
        }
        int line_stop = pos;
        while (line_stop < b.length() && b.charAt(line_stop) != '\0' && b.charAt(line_stop) != '\n' && line_length < 1024)
        {
            int w = Character.charCount(b.codePointAt(line_stop));
            line_length += utf8_length(b, line_stop, line_stop + w);
            line_stop += w;
        }
        boolean inside = pos < b.length();
        int cp = inside ? b.codePointAt(pos) : 0;
        return new ParseException(code, b.substring(Math.min(line_start, b.length()), Math.min(line_stop, b.length())),
                                  line, col, source_, inside ? indent : -1, cp, warning);
    }

    /** The number of utf8 bytes of the chars from start to stop. */
    static int utf8_length(String s, int start, int stop)
    {
        int n = 0;
        for (int i = start; i < stop; i++)
        {
            char c = s.charAt(i);
            if (c < 0x80) n += 1;
            else if (c < 0x800) n += 2;
            else if (Character.isHighSurrogate(c) && i + 1 < stop) { n += 4; i++; }
            else n += 3;
        }
        return n;
    }

    static private final boolean is_xmq_token_whitespace(char c)
    {
        if (c == ' ' || c == '\n' || c == '\r')
        {
            return true;
        }
        return false;
    }

    static private final boolean is_xmq_text_name(char c)
    {
        if (c >= 'a' && c <= 'z') return true;
        if (c >= 'A' && c <= 'Z') return true;
        if (c >= '0' && c <= '9') return true;
        if (c == '-' || c == '_' || c == '.' || c == ':' || c == '#') return true;
        return false;
    }

    boolean is_xmq_quote_start(char c)
    {
        return c == '\'' || c == '"';
    }

    boolean is_xmq_entity_start(char c)
    {
        return c == '&';
    }

    boolean is_xmq_compound_start(char c)
    {
        return c == '(';
    }

    boolean is_xmq_attribute_key_start(char c)
    {
        boolean t =
            c == '\'' ||
            c == '"' ||
            c == '(' ||
            c == ')' ||
            c == '{' ||
            c == '}' ||
            c == '/' ||
            c == '=' ||
            c == '&';

        return !t;
    }

    /** Check if a value can start with these two characters. */
    boolean unsafe_value_start(char c, char cc)
    {
        return c == '&' || c == '=' || (c == '/' && (cc == '/' || cc == '*'));
    }

    int count_whitespace()
    {
        if (i_ > buffer_len_) return 0;

        char c = currentChar();

        if (c == ' ' ||
            c == '\n' ||
            c == '\t' ||
            c == '\r' ||
            c == 0xa0 || // Unbreakable space
            c == 0x2000 || // EN quad. &#8192; U+2000 utf8 E2 80 80
            c == 0x2001 || // EM quad. &#8193; U+2001 utf8 E2 80 81
            c == 0x2002 || // EN space. &#8194; U+2002 utf8 E2 80 82
            c == 0x2003 // EM space. &#8195; U+2003 utf8 E2 80 83
            )
        {
            return 1;
        }

        // If, in the future, there is any whitespace outside of the BMP
        // (Basic Multilingual Plane) that does not fit in a single char.
        // I.e. needs two surrogate paris, then we will return 2 here.

        // But for now.
        return 0;
    }

    boolean is_safe_value_char()
    {
        char c = currentChar();
        boolean is_ws = count_whitespace() > 0 ||
        c == '\n' ||
        c == '\t' ||
        c == '\r' ||
        c == '(' ||
        c == ')' ||
        c == '{' ||
        c == '}' ||
        c == '\'' ||
        c == '"'
        ;
        return !is_ws;
    }

    Pair<Integer,Integer> eat_xmq_doctype()
    {
        char c = currentChar();
        assert(c == '!');
        int start = i_; // Mirrors C: the name includes the bang, ie. !DOCTYPE.
        increment(c);

        while (i_ < buffer_len_)
        {
            c = currentChar();
            if (!is_xmq_text_name(c)) break;
            increment(c);
        }

        return new Pair<>(start, i_);
    }

    Pair<Integer,Integer> eat_xmq_pi()
    {
        char c = currentChar();
        assert(c == '?');
        // Like C eat_xmq_pi the token includes the leading ?, which tells
        // create_node that this is a processing instruction.
        int start = i_;
        increment(c);

        while (i_ < buffer_len_)
        {
            c = currentChar();
            if (!is_xmq_text_name(c)) break;
            increment(c);
        }

        return new Pair<>(start, i_);
    }

    private Pair<Integer,Integer> eat_xmq_token_whitespace()
    {
        int start = i_;

        // Like C: a run of whitespace that starts with xmq token whitespace (space, newline,
        // cr) continues over any whitespace, eg nbsp, but not a tab.
        while (i_ < buffer_len_)
        {
            char c = currentChar();
            if (count_whitespace() == 0) break;
            // Tabs are not permitted as xmq token whitespace.
            if (c == '\t') break;
            // Pass the first char, needed to detect '\n' which increments line and set cols to 1.
            increment(c);
        }

        int stop = i_;

        return new Pair<>(start, stop);
    }

    /**
     * Parses xmq whitespace and invokes the whitespace callback.
     */
    protected void parse_xmq_whitespace()
    {
        int start_line = line_;
        int start_col = col_;
        Pair<Integer,Integer> p = eat_xmq_token_whitespace();

        do_whitespace(start_line, start_col, p.left(), p.right(), p.right());
    }

    int count_xmq_quotes(char q)
    {
        int i = i_;
        while (i < buffer_len_ && buffer_.charAt(i) == q) i++;
        return i-i_;
    }

    int findNLBackwards(int i, int max)
    {
        // An error at the end of the buffer starts looking at the last char.
        if (i >= buffer_len_) i = buffer_len_ - 1;
        if (i < 0) return 0;
        while (max > 0 && i > 0 && buffer_.charAt(i) != '\n') { i--; max--; }
        if (buffer_.charAt(i) == '\n') i++;
        return i;
    }

    int findNL(int i, int max)
    {
        while (max > 0 && i < buffer_len_ && buffer_.charAt(i) != '\n') { i++; max--; }
        return i;
    }

    Pair<Integer,Integer> eat_xmq_quote()
    {
        // Grab ' or " to know which kind of quote it is.
        char q = currentChar();

        assert(q == '\'' || q == '"');

        int depth = count_xmq_quotes(q);
        int count = depth;

        last_quote_start_ = i_;
        last_quote_start_line_ = line_;
        last_quote_start_col_ = col_;

        int start = i_;
        int stop = i_;

        while (count > 0)
        {
            increment(q);
            count--;
        }

        if (depth == 2)
        {
            // The empty quote ''
            stop = i_;
            return new Pair<>(start, stop);
        }

        while (i_ < buffer_len_)
        {
            char c = currentChar();
            if (c != q)
            {
                increment(c);
                continue;
            }

            count = count_xmq_quotes(q);
            if (count > depth)
            {
                // Like C the error is reported where the quote starts.
                throw error_at(ParseErrorCode.XMQ_ERROR_QUOTE_CLOSED_WITH_TOO_MANY_QUOTES, last_quote_start_,
                               last_quote_start_line_, last_quote_start_col_);
            }
            else if (count < depth)
            {
                while (count > 0)
                {
                    increment(q);
                    count--;
                }
                continue;
            }
            else if (count == depth)
            {
                while (count > 0)
                {
                    increment(q);
                    count--;
                }
                depth = 0;
                stop = i_;
                break;
            }
        }

        if (depth != 0)
        {
            throw error_at(ParseErrorCode.XMQ_ERROR_QUOTE_NOT_CLOSED, last_quote_start_,
                           last_quote_start_line_, last_quote_start_col_);
        }

        if (possibly_need_more_quotes())
        {
            last_suspicios_quote_end_ = i_ - 1;
            last_suspicios_quote_end_line_ = line_;
            last_suspicios_quote_end_col_ = col_ - 1;
        }

        return new Pair<>(start, stop);
    }

    /**
     * Like possibly_need_more_quotes in C: after a quote, a quote that ends inside a word,
     * eg the s in 'There's a man.', perhaps needed more quotes. It is remembered and
     * reported as a warning if the parse then fails with an unexpected character.
     */
    private boolean possibly_need_more_quotes()
    {
        if (i_ - 2 < 0 || i_ >= buffer_len_) return false;
        char c0 = buffer_.charAt(i_ - 2);
        char c1 = buffer_.charAt(i_ - 1); // This is the apostrophe.
        char c2 = currentChar();
        return c0 != '\'' && c0 != ' ' && c1 == '\'' && c2 != '\'' && c2 != 0 &&
            " \t\r\n(){}".indexOf(c2) < 0;
    }

    void parse_xmq_quote(XMQLevel level)
    {
        int start_line = line_;
        int start_col = col_;

        Pair<Integer,Integer> p = eat_xmq_quote();
        int start = p.left();
        int stop = p.right();

        switch(level)
        {
        case LEVEL_XMQ:
            do_quote(start_line, start_col, start, stop, stop);
            break;
        case LEVEL_ELEMENT_VALUE:
            do_element_value_quote(start_line, start_col, start, stop, stop);
        break;
        case LEVEL_ELEMENT_VALUE_COMPOUND:
            do_element_value_compound_quote(start_line, start_col, start, stop, stop);
            break;
        case LEVEL_ATTR_VALUE:
            do_attr_value_quote(start_line, start_col, start, stop, stop);
        break;
        case LEVEL_ATTR_VALUE_COMPOUND:
            do_attr_value_compound_quote(start_line, start_col, start, stop, stop);
            break;
        }
    }

    void eat_xmq_entity()
    {
        // Like C an error is reported where the entity starts.
        int start = i_, start_line = line_, start_col = col_;
        increment('&');
        char c = 0;
        boolean expect_semicolon = false;

        while (i_ < buffer_len_)
        {
            c = currentChar();
            if (!is_xmq_text_name(c)) break;
            if (!Text.is_lowercase_hex(c)) expect_semicolon = true;
            increment(c);
        }
        if (c == ';')
        {
            increment(c);
            c = currentChar();
            expect_semicolon = false;
        }
        if (expect_semicolon)
        {
            throw error_at(ParseErrorCode.XMQ_ERROR_ENTITY_NOT_CLOSED, start, start_line, start_col);
        }
    }

    void parse_xmq_entity(XMQLevel level)
    {
        int start_line = line_;
        int start_col = col_;
        int start = i_;

        eat_xmq_entity();

        int stop = i_;

        switch (level)
        {
        case LEVEL_XMQ:
            do_entity(start_line, start_col, start, stop, stop);
            break;
        case LEVEL_ELEMENT_VALUE:
            do_element_value_entity(start_line, start_col, start, stop, stop);
            break;
        case LEVEL_ELEMENT_VALUE_COMPOUND:
            do_element_value_compound_entity(start_line, start_col, start,  stop, stop);
            break;
        case LEVEL_ATTR_VALUE:
            do_attr_value_entity(start_line, start_col, start, stop, stop);
            break;
        case LEVEL_ATTR_VALUE_COMPOUND:
            do_attr_value_compound_entity(start_line, start_col, start, stop, stop);
            break;
        }
    }

    /** Parse a compound value, ie:  = ( '   ' &#10; '  info ' )
        a compound can only occur after an = (equals) character.
        The normal quoting with single quotes, is enough for all quotes except:
        1) An attribute value with leading/ending whitespace including leading/ending newlines.
        2) An attribute with a mix of quotes and referenced entities.
        3) Compact xmq where actual newlines have to be replaced with &#10;

        Note that an element key = ( ... ) can always  be replaced with key { ... }
        so compound values are not strictly necessary for element key values.
        However they are permitted for symmetry reasons.
    */
    void parse_xmq_compound(XMQLevel level)
    {
        int start = i_;
        int start_line = line_;
        int start_col = col_;

        last_compound_start_ = start;
        last_compound_start_line_ = start_line;
        last_compound_start_col_ = start_col;
        increment('(');
        int stop = i_;

        do_cpar_left(start_line, start_col, start, stop, stop);

        parse_xmq_compound_children(Util.enter_compound_level(level));

        char c = currentChar();
        if (is_xmq_token_whitespace(c)) { parse_xmq_whitespace(); c = currentChar(); }

        if (c != ')')
        {
            // Like C, the error is reported where the compound should have been closed.
            throw error_here(ParseErrorCode.XMQ_ERROR_COMPOUND_NOT_CLOSED);
        }

        start = i_;
        start_line = line_;
        start_col = col_;
        increment(')');
        stop = i_;

        do_cpar_right(start_line, start_col, start, stop, stop);
    }

    /** Parse each compound child (quote or entity) until end of file or a ')' is found. */
    void parse_xmq_compound_children(XMQLevel level)
    {
        while (i_ < buffer_len_)
        {
            char c = currentChar();
            if (is_xmq_token_whitespace(c)) parse_xmq_whitespace();
            else if (c == ')') break;
            else if (is_xmq_quote_start(c)) parse_xmq_quote(level);
            else if (is_xmq_entity_start(c)) parse_xmq_entity(level);
            else
            {
                throw error_here(ParseErrorCode.XMQ_ERROR_COMPOUND_MAY_NOT_CONTAIN);
            }
        }
    }

    boolean is_xmq_comment_start(char c, char cc)
    {
        return c == '/' && (cc == '/' || cc == '*');
    }

    Pair<Integer,Boolean> count_xmq_slashes()
    {
        int i = i_;
        int start = i_;

        while (i < buffer_len_ && buffer_.charAt(i) == '/') i++;

        boolean fa = buffer_.charAt(i) == '*'; // Found asterisk?
        int n = i-start;
        return new Pair<>(n, fa);
    }

    void eat_xmq_comment_to_eol()
    {
        increment('/');
        increment('/');

        char c = 0;
        while (i_ < buffer_len_ && c != '\n')
        {
            c = currentChar();
            increment(c);
        }
    }

    boolean eat_xmq_comment_to_close(int num_slashes)
    {
        // Like C an error is reported where this part of the comment starts.
        int part_start = i_, part_line = line_, part_col = col_;
        int n = num_slashes;

        if (buffer_.charAt(i_) == '/')
        {
            // Comment starts from the beginning ////* ....
            // Otherwise this is a continuation and *i == '*'
            while (n > 0)
            {
                increment('/');
                n--;
            }
        }

        increment('*');

        char c = 0;
        char cc = 0;
        n = 0;
        while (i_ < buffer_len_)
        {
            cc = c;
            c = currentChar();
            if (cc != '*' || c != '/')
            {
                // Not a possible end marker */ or *///// continue eating.
                increment(c);
                continue;
            }
            // We have found */ or *//// not count the number of slashes.
            Pair<Integer,Boolean> nw = count_xmq_slashes();
            n = nw.left();
            boolean found_asterisk = nw.right();

            if (n < num_slashes) continue; // Not a balanced end marker continue eating,

            if (n > num_slashes)
            {
                // Oups, too many slashes.
                throw error_at(ParseErrorCode.XMQ_ERROR_COMMENT_CLOSED_WITH_TOO_MANY_SLASHES, part_start, part_line, part_col);
            }

            assert(n == num_slashes);
            // Found the ending slashes!
            while (n > 0)
            {
                cc = c;
                c = currentChar();
                increment('/');
                n--;
            }
            return found_asterisk;
        }
        // We reached the end of the xmq and no */ was found!
        throw error_at(ParseErrorCode.XMQ_ERROR_COMMENT_NOT_CLOSED, part_start, part_line, part_col);
    }

    void parse_xmq_comment(char cc)
    {
        int start = i_;
        int start_line = line_;
        int start_col = col_;

        last_comment_start_ = start;
        last_comment_start_line_ = line_;
        last_comment_start_col_ = col_;

        Pair<Integer,Boolean> r = count_xmq_slashes();
        int n = r.left();
        boolean found_asterisk = r.right();
        if (!found_asterisk)
        {
            // This is a single line asterisk.
            eat_xmq_comment_to_eol();
            int stop = i_;
            do_comment(start_line, start_col, start, stop, stop);
        }
        else
        {
            // This is a /* ... */ or ////*  ... *//// comment.
            found_asterisk = eat_xmq_comment_to_close(n);
            int stop = i_;
            do_comment(start_line, start_col, start, stop, stop);

            while (found_asterisk)
            {
                // Aha, this is a comment continuation /* ... */* ...
                start = i_;
                start_line = line_;
                start_col = col_;
                found_asterisk = eat_xmq_comment_to_close(n);
                stop = i_;
                do_comment_continuation(start_line, start_col, start, stop, stop);
            }
        }
    }

    boolean is_xmq_element_start(char c)
    {
        if (c >= 'a' && c <= 'z') return true;
        if (c >= 'A' && c <= 'Z') return true;
        if (c == '_') return true;
        return false;
    }

    Quad<Integer,Integer,Integer,Integer> eat_xmq_text_name()
    {
        int text_start;
        int text_stop;
        int namespace_start;
        int namespace_stop;
        int colon = 0;

        text_start = i_;

        while (i_ < buffer_len_)
        {
            char c = currentChar();
            if (!is_xmq_text_name(c)) break;
            if (c == ':') colon = i_;
            increment(c);
        }

        if (colon > 0)
        {
            namespace_start = text_start;
            namespace_stop = colon;
            text_start = colon+1;
        }
        else
        {
            namespace_start = 0;
            namespace_stop = 0;
        }
        text_stop = i_;

        return new Quad<>(text_start, text_stop, namespace_start, namespace_stop);
    }

    void eat_xmq_text_value()
    {
        while (i_ < buffer_len_)
        {
            char c = currentChar();
            if (!is_safe_value_char()) break;
            increment(c);
        }
    }

    boolean peek_xmq_next_is_equal()
    {
        int i = i_;
        char c = 0;

        while (i < buffer_len_)
        {
            c = buffer_.charAt(i);
            if (!is_xmq_token_whitespace(c)) break;
            i++;
        }
        return c == '=';
    }

    void parse_xmq_text_value(XMQLevel level)
    {
        int start = i_;
        int start_line = line_;
        int start_col = col_;

        eat_xmq_text_value();

        int stop = i_;

        assert(level != XMQLevel.LEVEL_XMQ);

        if (level == XMQLevel.LEVEL_ATTR_VALUE)
        {
            do_attr_value_text(start_line, start_col, start, stop, stop);
        }
        else
        {
            do_element_value_text(start_line, start_col, start, stop, stop);
        }
    }

    void parse_xmq_value(XMQLevel level)
    {
        char c = currentChar();

        if (is_xmq_token_whitespace(c)) { parse_xmq_whitespace(); c = currentChar(); }

        if (is_xmq_quote_start(c))
        {
            parse_xmq_quote(level);
        }
        else if (is_xmq_entity_start(c))
        {
            parse_xmq_entity(level);
        }
        else if (is_xmq_compound_start(c))
        {
            parse_xmq_compound(level);
        }
        else
        {
            char cc = nextChar();
            if (unsafe_value_start(c, cc))
            {
                throw error_here(ParseErrorCode.XMQ_ERROR_VALUE_CANNOT_START_WITH);
            }
            parse_xmq_text_value(level);
        }
    }

    /** Parse a list of attribute key = value, or just key children until a ')' is found. */
    void parse_xmq_attributes()
    {
        while (i_ < buffer_len_)
        {
            char c = currentChar();

            if (is_xmq_token_whitespace(c)) parse_xmq_whitespace();
            else if (c == ')') return;
            // A key start that is not a name character (eg ; or å) would give an empty key
            // without advancing, stop here so that the attributes are reported as not closed.
            else if (is_xmq_attribute_key_start(c) && is_xmq_text_name(c)) parse_xmq_attribute();
            else break;
        }
    }

    void parse_xmq_attribute()
    {
        char c = 0;
        int start_line = line_;
        int start_col = col_;

        Quad<Integer,Integer,Integer,Integer> r = eat_xmq_text_name();

        int name_start = r.first();
        int name_stop = r.second();
        int ns_start = r.third();
        int ns_stop = r.fourth();

        int start = i_;

        if (ns_stop == 0)
        {
            // No colon found, we have either a normal: key=123
            // or a default namespace declaration xmlns=...
            int len = name_stop - name_start;
            String part = buffer_.substring(name_start, name_stop);
            if (len == 5 && part.equals("xmlns"))
            {
                // A default namespace declaration, eg: xmlns=uri
                do_ns_declaration(start_line, start_col, name_start, name_stop, name_stop);
            }
            else
            {
                // A normal attribute key, eg: width=123
                do_attr_key(start_line, start_col, name_start, name_stop, name_stop);
            }
        }
        else
        {
            // We have a colon in the attribute key.
            // E.g. alfa:beta where alfa is attr_ns and beta is attr_key
            // However we can also have xmlns:xsl then it gets tokenized as ns_declaration and attr_ns.
            int ns_len = ns_stop - ns_start;
            String xmlns_part = buffer_.substring(ns_start, ns_stop);
            if (ns_len == 5 && xmlns_part.equals("xmlns"))
            {
                // The xmlns signals a declaration of a namespace.
                do_ns_declaration(start_line, start_col, ns_start, ns_stop, name_stop);
                do_ns_colon(start_line, start_col+ns_len, ns_stop, ns_stop+1, ns_stop+1);
                do_attr_ns(start_line, start_col+ns_len+1, name_start, name_stop, name_stop);
            }
            else
            {
                // Normal namespaced attribute. Please try to avoid namespaced attributes
                // because you only need to attach the namespace to the element itself,
                // from that follows automatically the unique namespaced attributes.
                // But if you are adding attributes to an existing xml with schema, then you will need
                // use namespaced attributes to avoid tripping the xml validation.
                // An example of this is: xlink:href.
                do_attr_ns(start_line, start_col, ns_start, ns_stop, ns_stop);
                do_ns_colon(start_line, start_col+ns_len, ns_stop, ns_stop+1, ns_stop+1);
                do_attr_key(start_line, start_col+ns_len+1, name_start, name_stop, name_stop);
            }
        }

        c = currentChar();
        if (is_xmq_token_whitespace(c)) { parse_xmq_whitespace(); c = currentChar(); }

        if (c == '=')
        {
            start = i_;
            start_line = line_;
            start_col = col_;
            increment('=');
            int stop = i_;
            do_equals(start_line, start_col, start, stop, stop);
            parse_xmq_value(XMQLevel.LEVEL_ATTR_VALUE);
            do_equals_done(line_, col_, i_, i_, i_);
            return;
        }
    }

    void parse_xmq_element()
    {
        parse_xmq_element_internal(false, false);
    }

    void parse_xmq_doctype()
    {
        parse_xmq_element_internal(true, false);
    }

    void parse_xmq_pi()
    {
        parse_xmq_element_internal(false, true);
    }

    char currentChar()
    {
        if (i_ < buffer_len_) return buffer_.charAt(i_);
        return 0;
    }

    char nextChar()
    {
        if (i_+1 < buffer_len_) return buffer_.charAt(i_+1);
        return 0;
    }

    void parse_xmq_element_internal(boolean doctype, boolean pi)
    {
        char c = 0;
        // Name
        int name_start = 0;
        int name_stop = 0;
        // Namespace
        int ns_start = 0;
        int ns_stop = 0;

        int start_line = line_;
        int start_col = col_;

        if (doctype)
        {
            Pair<Integer,Integer> p = eat_xmq_doctype();
            name_start = p.left();
            name_stop = p.right();
        }
        else if (pi)
        {
            Pair<Integer,Integer> p = eat_xmq_pi();
            name_start = p.left();
            name_stop = p.right();
        }
        else
        {
            Quad<Integer,Integer,Integer,Integer> q = eat_xmq_text_name();
            name_start = q.first();
            name_stop = q.second();
            ns_start = q.third();
            ns_stop = q.fourth();
        }
        int stop = i_;

        // The only peek ahead in the whole grammar! And its only for syntax coloring. :-)
        // key = 123   vs    name { '123' }
        boolean is_key = peek_xmq_next_is_equal();

        if (ns_stop == 0)
        {
            // Normal key/name element.
            if (is_key)
            {
                do_element_key(start_line, start_col, name_start, name_stop, stop);
            }
            else
            {
                do_element_name(start_line, start_col, name_start, name_stop, stop);
            }
        }
        else
        {
            // We have a namespace prefixed to the element, eg: abc:working
            int ns_len = ns_stop - ns_start;
            do_element_ns(start_line, start_col, ns_start, ns_stop, ns_stop);
            do_ns_colon(start_line, start_col+ns_len, ns_stop, ns_stop+1, ns_stop+1);

            if (is_key)
            {
                do_element_key(start_line, start_col+ns_len+1, name_start, name_stop, stop);
            }
            else
            {
                do_element_name(start_line, start_col+ns_len+1, name_start, name_stop, stop);
            }
        }


        c = currentChar();
        if (is_xmq_token_whitespace(c)) { parse_xmq_whitespace(); c = currentChar(); }

        if (c == '(')
        {
            int start = i_;
            last_attr_start_ = i_;
            last_attr_start_line_ = line_;
            last_attr_start_col_ = col_;
            start_line = line_;
            start_col = col_;
            increment('(');
            stop = i_;
            do_apar_left(start_line, start_col, start, stop, stop);

            parse_xmq_attributes();

            c = currentChar();
            if (is_xmq_token_whitespace(c)) { parse_xmq_whitespace(); c = currentChar(); }
            if (c != ')')
            {
                // Like C, report the error where the attributes started.
                throw error_at(ParseErrorCode.XMQ_ERROR_ATTRIBUTES_NOT_CLOSED, last_attr_start_,
                               last_attr_start_line_, last_attr_start_col_);
            }

            start = i_;
            int parentheses_right_start = i_;
            int parentheses_right_stop = i_+1;

            start_line = line_;
            start_col = col_;
            increment(')');
            stop = i_;
            do_apar_right(start_line, start_col, parentheses_right_start, parentheses_right_stop, stop);
        }

        c = currentChar();
        if (is_xmq_token_whitespace(c)) { parse_xmq_whitespace(); c = currentChar(); }

        if (c == '=')
        {
            last_equals_start_ = i_;
            last_equals_start_line_ = line_;
            last_equals_start_col_ = col_;
            int start = i_;
            start_line = line_;
            start_col = col_;
            increment('=');
            stop = i_;

            do_equals(start_line, start_col, start, stop, stop);
            parse_xmq_value(XMQLevel.LEVEL_ELEMENT_VALUE);
            do_equals_done(line_, col_, i_, i_, i_);
            return;
        }

        if (c == '{')
        {
            int start = i_;
            last_body_start_ = i_;
            last_body_start_line_ = line_;
            last_body_start_col_ = col_;
            start_line = line_;
            start_col = col_;
            increment('{');
            stop = i_;
            do_brace_left(start_line, start_col, start, stop, stop);

            if (++nesting_depth_ > MAX_NESTING_DEPTH)
            {
                throw error_here(ParseErrorCode.XMQ_ERROR_TOO_DEEP);
            }
            parse_xmq();
            nesting_depth_--;

            c = currentChar();

            if (is_xmq_token_whitespace(c)) { parse_xmq_whitespace(); c = currentChar(); }
            if (c != '}')
            {
                throw error_at(ParseErrorCode.XMQ_ERROR_BODY_NOT_CLOSED, last_body_start_,
                               last_body_start_line_, last_body_start_col_);
            }

            start = i_;
            start_line = line_;
            start_col = col_;
            increment('}');
            stop = i_;
            do_brace_right(start_line, start_col, start, stop, stop);
        }
    }

    void parse_xmq()
    {
        while (i_ < buffer_len_)
        {
            char c = currentChar();
            char cc = 0;
            if ((c == '/' || c == '(') && i_+1 < buffer_len_) cc = buffer_.charAt(i_+1);
            if (is_xmq_token_whitespace(c)) parse_xmq_whitespace();
            else if (is_xmq_quote_start(c)) parse_xmq_quote(XMQLevel.LEVEL_XMQ);
            else if (is_xmq_entity_start(c)) parse_xmq_entity(XMQLevel.LEVEL_XMQ);
            else if (is_xmq_comment_start(c, cc)) parse_xmq_comment(cc);
            else if (is_xmq_element_start(c)) parse_xmq_element();
            else if (c == '}') { return; }
            else if (is_xmq_doctype_start()) parse_xmq_doctype();
            else if (is_xmq_pi_start()) parse_xmq_pi();
            else if (c == '<') parse_xmq_embedded_xml();
            else if (possibly_lost_content_after_equals())
            {
                throw error_at(ParseErrorCode.XMQ_ERROR_EXPECTED_CONTENT_AFTER_EQUALS, last_equals_start_,
                               last_equals_start_line_, last_equals_start_col_);
            }
            else if (c == '\t')
            {
                throw error_here(ParseErrorCode.XMQ_ERROR_UNEXPECTED_TAB);
            }
            else
            {
                throw error_here(ParseErrorCode.XMQ_ERROR_INVALID_CHAR);
            }
        }
    }

    /**
     * Like possibly_lost_content_after_equals in C: an unexpected { ( or = after a name on
     * a line that follows a line ending with an equals sign, eg
     *     alfa =
     *     beta { 'more' }
     * where beta became the value of alfa.
     */
    private boolean possibly_lost_content_after_equals()
    {
        char c = currentChar();
        if (!(c == '{' || c == '(' || c == '=')) return false;
        String b = buffer_;
        // Nothing before, eg the buffer starts with ( or {.
        if (i_ <= 0) return false;
        int i = i_ - 1;
        // Scan backwards for newline accepting only texts and xml whitespace.
        while (i > 0 && b.charAt(i) != '\n' && (is_xmq_text_name(b.charAt(i)) || is_xml_ws(b.charAt(i)))) i--;
        if (i == 0 || b.charAt(i) != '\n') return false;
        // We found the newline, see if the next character backwards is an equals sign.
        while (i > 0 && is_xml_ws(b.charAt(i))) i--;
        return b.charAt(i) == '=';
    }

    private static boolean is_xml_ws(char c)
    {
        return c == ' ' || c == '\t' || c == '\n' || c == '\r';
    }

    /**
     * Find the end of an embedded xml fragment, like find_xmq_embedded_xml_stop in C:
     * a single xml element (possibly self closing) ending with its matching closing tag.
     * @param start Position of the starting less than char.
     * @return The position after the fragment or -1 if not closed.
     */
    int find_xmq_embedded_xml_stop(int start)
    {
        int i = start, end = buffer_len_, depth = 0;
        while (i < end)
        {
            char c = buffer_.charAt(i);
            if (c == '"' || c == '\'')
            {
                // A quoted string, skip it.
                i++;
                while (i < end && buffer_.charAt(i) != c) i++;
                if (i < end) i++;
                continue;
            }
            if (c != '<') { i++; continue; }
            if (i + 1 >= end) return -1;
            char nc = buffer_.charAt(i + 1);
            if (nc == '!')
            {
                if (i + 4 < end && buffer_.startsWith("<!--", i))
                {
                    // An xml comment, skip to -->
                    while (i + 3 < end && !buffer_.startsWith("-->", i)) i++;
                    if (i + 3 >= end) return -1;
                    i += 3;
                    continue;
                }
                // Cdata or doctype, skip to >
                while (i < end && buffer_.charAt(i) != '>') i++;
                if (i >= end) return -1;
                i++;
                continue;
            }
            if (nc == '?' || nc == '/')
            {
                // A processing instruction or a closing tag, skip to >
                while (i < end && buffer_.charAt(i) != '>') i++;
                if (i >= end) return -1;
                i++;
                if (nc == '/')
                {
                    depth--;
                    if (depth <= 0) return i;
                }
                continue;
            }
            // An opening tag <name ...>
            int tag_start = i;
            while (i < end && buffer_.charAt(i) != '>') i++;
            if (i >= end) return -1;
            int k = i - 1;
            while (k > tag_start && (buffer_.charAt(k) == ' ' || buffer_.charAt(k) == '\t' || buffer_.charAt(k) == '\n' || buffer_.charAt(k) == '\r')) k--;
            boolean self_closing = k > tag_start && buffer_.charAt(k) == '/';
            i++;
            if (self_closing && depth == 0) return i;
            if (!self_closing) depth++;
        }
        return -1;
    }

    /** An embedded xml fragment in xmq, like parse_xmq_embedded_xml in C. */
    void parse_xmq_embedded_xml()
    {
        int start = i_;
        int stop = find_xmq_embedded_xml_stop(start);
        if (stop < 0)
        {
            throw error_here(ParseErrorCode.XMQ_ERROR_XML_FRAGMENT_NOT_CLOSED);
        }
        do_embedded_xml(start, stop);
        while (i_ < stop) increment(buffer_.charAt(i_));
    }

    /**
     * An embedded xml fragment was found, by default it is ignored.
     * @param start Start of the fragment.
     * @param stop End of the fragment.
     */
    void do_embedded_xml(int start, int stop)
    {
    }

    String xmq_un_comment(int start, int stop)
    {
        return XmqTrim.unComment(buffer_, start, stop);
    }

}
