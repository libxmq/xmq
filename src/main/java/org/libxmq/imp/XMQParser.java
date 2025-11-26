/* libxmq - Copyright (C) 2023 Fredrik Öhrström (spdx: MIT)

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

public abstract class XMQParser extends XMQParseCallbacks
{
    String source_name_; // Used for generating error messages.
    String buffer_; // XMQ source to parse.
    String implicit_root_; // Assume that this is the first element name

    int buffer_len_; // Buffer length.

    // These three: i_, line_, col_, are only updated using the increment function.
    int i_; // Current parsing position.
    int line_; // Current line.
    int col_; // Current col.

    XMQParseError error_nr_;
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

    int last_equals_start_;
    int last_equals_start_line_;
    int last_equals_start_col_;

    int last_suspicios_quote_end_;
    int last_suspicios_quote_end_line_;
    int last_suspicios_quote_end_col_;

    public boolean parse(String buf, String name)
    {
        source_name_ = name;
        buffer_ = buf;
        buffer_len_ = buf.length();
        i_ = 0;
        line_ = 1;
        col_ = 1;

        try
        {
            setup();
            parse_xmq();
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
        col_++;
        if (c == '\n')
        {
            line_++;
            col_ = 1;
        }
        i_++;
    }

    static private final boolean is_xmq_token_whitespace(char c)
    {
        if (c == ' ' || c == '\n' || c == '\t' || c == '\r')
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

    private Pair<Integer,Integer> eat_xmq_token_whitespace()
    {
        int start = i_;

        while (i_ < buffer_len_)
        {
            char c = currentChar();
            if (!is_xmq_token_whitespace(c)) break;
            // Tabs are not permitted as xmq token whitespace.
            if (c == '\t') break;
            // Pass the first char, needed to detect '\n' which increments line and set cols to 1.
            increment(c);
        }

        int stop = i_;

        return new Pair<>(start, stop);
    }

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
                error_nr_ = XMQParseError.XMQ_ERROR_QUOTE_CLOSED_WITH_TOO_MANY_QUOTES;
                throw new XMQParseException(error_nr_);
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
            error_nr_ = XMQParseError.XMQ_ERROR_QUOTE_NOT_CLOSED;
            throw new XMQParseException(error_nr_);
        }

        /*
          if (possibly_need_more_quotes(state))
          {
          state->last_suspicios_quote_end = state->i-1;
          state->last_suspicios_quote_end_line = state->line;
          state->last_suspicios_quote_end_col = state->col-1;
          }*/

        return new Pair<>(start, stop);
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
            error_nr_ = XMQParseError.XMQ_ERROR_ENTITY_NOT_CLOSED;
            throw new XMQParseException(error_nr_);
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
                error_nr_ = XMQParseError.XMQ_ERROR_COMMENT_CLOSED_WITH_TOO_MANY_SLASHES;
                throw new XMQParseException(error_nr_);
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
        error_nr_ = XMQParseError.XMQ_ERROR_COMMENT_NOT_CLOSED;
        throw new XMQParseException(error_nr_);
    }

    void parse_xmq_comment(char cc)
    {
        int start = i_;
        int start_line = line_;
        int start_col = col_;
        int comment_start;
        int comment_stop;

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
            // TODOparse_xmq_compound(state, level);
        }
        else
        {
            char cc = currentChar();
            if (unsafe_value_start(c, cc))
            {
                error_nr_ = XMQParseError.XMQ_ERROR_VALUE_CANNOT_START_WITH;
                throw new XMQParseException(error_nr_);
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
            else if (is_xmq_attribute_key_start(c)) parse_xmq_attribute();
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

        if (ns_start == 0)
        {
            // No colon found, we have either a normal: key=123
            // or a default namespace declaration xmlns=...
            int len = name_stop - name_start;
            if (len == 5 && !buffer_.substring(name_start, name_stop).equals("xmlns"))
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
            if (ns_len == 5 && buffer_.substring(ns_start, ns_stop).equals("xmlns"))
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

    char currentChar()
    {
        if (i_ < buffer_len_) return buffer_.charAt(i_);
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

        /*
        if (doctype)
        {
            Pair<Integer,Integer> p = eat_xmq_doctype();
            name_start = p.left();
            name_stop = p.right();
        }
        else if (pi)
        {
            Pair<Integer,Integer> p = eat_xmq_pi(state);
            name_start = p.left();
            name_stop = p.right();
        }
        else*/
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

        if (ns_start == 0)
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
            do_colon(start_line, start_col+ns_len, ns_stop, ns_stop+1, ns_stop+1);

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
                error_nr_ = XMQParseError.XMQ_ERROR_ATTRIBUTES_NOT_CLOSED;
                throw new XMQParseException(error_nr_);
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

            parse_xmq();

            c = currentChar();

            if (is_xmq_token_whitespace(c)) { parse_xmq_whitespace(); c = currentChar(); }
            if (c != '}')
            {
                error_nr_ = XMQParseError.XMQ_ERROR_BODY_NOT_CLOSED;
                throw new XMQParseException(error_nr_);
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
            /*
            else if (is_xmq_doctype_start(state->i, end)) parse_xmq_doctype(state);
            else if (is_xmq_pi_start(state->i, end)) parse_xmq_pi(state);
            */
            else
            {
                /*if (possibly_lost_content_after_equals(state))
                {
                    error_nr_ = XMQ_ERROR_EXPECTED_CONTENT_AFTER_EQUALS;
                }
                else */if (c == '\t')
                {
                    error_nr_ = XMQParseError.XMQ_ERROR_UNEXPECTED_TAB;
                }
                else
                {
                    error_nr_ = XMQParseError.XMQ_ERROR_INVALID_CHAR;
                    throw new XMQParseException(error_nr_);
                }
                System.err.println("Internal error.");
                System.exit(1);
            }
        }
    }
}
