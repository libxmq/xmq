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

package org.libxmq;

class XMQParseState
{
    String source_name_; // Only used for generating any error messages.
    String buffer_; // XMQ source to parse.
    int buffer_len_; // Current parsing position.

    // These three: i_, line_, col_, are only updated using the increment function.
    int i_; // Current parsing position.
    int line_; // Current line.
    int col_; // Current col.

    // After an eat has completed, the positions are stored here.
    int start_;
    int stop_;

    XMQParseError error_nr_;
    String generated_error_msg;

    boolean simulated; // When true, this is generated from JSON parser to simulate an xmq element name.
    XMQParseCallbacks parse;
    XMQDoc doq;
    String implicit_root; // Assume that this is the first element name
    /*
    Stack *element_stack; // Top is last created node
    void *element_last; // Last added sibling to stack top node.
    bool parsing_doctype; // True when parsing a doctype.
    void *add_doctype_before; // Used when retrofitting a doctype found in json.
    bool doctype_found; // True after a doctype has been parsed.
    bool parsing_pi; // True when parsing a processing instruction, pi.
    bool merge_text; // Merge text nodes and character entities.
    bool no_trim_quotes; // No trimming if quotes, used when reading json strings.
    const char *pi_name; // Name of the pi node just started.
    XMQOutputSettings *output_settings; // Used when coloring existing text using the tokenizer.
    int magic_cookie; // Used to check that the state has been properly initialized.

    char *element_namespace; // The element namespace is found before the element name. Remember the namespace name here.
    char *attribute_namespace; // The attribute namespace is found before the attribute key. Remember the namespace name here.
    bool declaring_xmlns; // Set to true when the xmlns declaration is found, the next attr value will be a href
    void *declaring_xmlns_namespace; // The namespace to be updated with attribute value, eg. xmlns=uri or xmlns:prefix=uri

    void *default_namespace; // If xmlns=http... has been set, then a pointer to the namespace object is stored here.

    // These are used for better error reporting.
    const char *last_body_start;
    size_t last_body_start_line;
    size_t last_body_start_col;
    const char *last_attr_start;
    size_t last_attr_start_line;
    size_t last_attr_start_col;
    */
    int last_quote_start_;
    int last_quote_start_line_;
    int last_quote_start_col_;
    /*
    const char *last_compound_start;
    size_t last_compound_start_line;
    size_t last_compound_start_col;
    const char *last_equals_start;
    size_t last_equals_start_line;
    size_t last_equals_start_col;
    */
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
        start_ = 0;
        stop_ = 0;

        try
        {
            parse_xmq();
        }
        catch(XMQParseException pe)
        {
            return false;
        }
        return true;
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

    private boolean is_xmq_token_whitespace(char c)
    {
        if (c == ' ' || c == '\n' || c == '\t' || c == '\r')
        {
            return true;
        }
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

    private void eat_xmq_token_whitespace()
    {
        start_ = i_;

        while (i_ < buffer_len_)
        {
            char c = buffer_.charAt(i_);
            if (!is_xmq_token_whitespace(c)) break;
            // Tabs are not permitted as xmq token whitespace.
            if (c == '\t') break;
            // Pass the first char, needed to detect '\n' which increments line and set cols to 1.
            increment(c);
        }

        stop_ = i_;
    }

    protected void parse_xmq_whitespace()
    {
        int start_line = line_;
        int start_col = col_;
        eat_xmq_token_whitespace();
        do_whitespace(start_line, start_col, start_, stop_, stop_);
    }

    void do_whitespace(int start_line, int start_col, int start, int stop, int stop_suffix)
    {
        System.out.print("[whitespace "+
                         Util.xmq_quote_as_c(buffer_, start, stop, true)+" "+start_line+":"+start_col+"]");
    }

    void do_quote(int start_line, int start_col, int start, int stop, int stop_suffix)
    {
        System.out.print("[quote "+
                         Util.xmq_quote_as_c(buffer_, start, stop, true)+" "+start_line+":"+start_col+"]");
    }

    void do_element_value_quote(int start_line, int start_col, int start, int stop, int stop_suffix)
    {
        System.out.print("[element_value_quote "+
                         Util.xmq_quote_as_c(buffer_, start, stop, true)+" "+start_line+":"+start_col+"]");
    }

    void do_element_value_compound_quote(int start_line, int start_col, int start, int stop, int stop_suffix)
    {
        System.out.print("[element_value_compound_quote "+
                         Util.xmq_quote_as_c(buffer_, start, stop, true)+" "+start_line+":"+start_col+"]");
    }

    void do_attr_value_quote(int start_line, int start_col, int start, int stop, int stop_suffix)
    {
        System.out.print("[attr_value_quote "+
                         Util.xmq_quote_as_c(buffer_, start, stop, true)+" "+start_line+":"+start_col+"]");
    }

    void do_attr_value_compound_quote(int start_line, int start_col, int start, int stop, int stop_suffix)
    {
        System.out.print("[attr_value_compound_quote "+
                         Util.xmq_quote_as_c(buffer_, start, stop, true)+" "+start_line+":"+start_col+"]");
    }

    void do_entity(int start_line, int start_col, int start, int stop, int stop_suffix)
    {
        System.out.print("[entity "+
                         Util.xmq_quote_as_c(buffer_, start, stop, true)+" "+start_line+":"+start_col+"]");
    }

    void do_element_value_entity(int start_line, int start_col, int start, int stop, int stop_suffix)
    {
        System.out.print("[element_value_entity "+
                         Util.xmq_quote_as_c(buffer_, start, stop, true)+" "+start_line+":"+start_col+"]");
    }

    void do_element_value_compound_entity(int start_line, int start_col, int start, int stop, int stop_suffix)
    {
        System.out.print("[element_value_compound_entity "+
                         Util.xmq_quote_as_c(buffer_, start, stop, true)+" "+start_line+":"+start_col+"]");
    }

    void do_attr_value_entity(int start_line, int start_col, int start, int stop, int stop_suffix)
    {
        System.out.print("[attr_value_entity "+
                         Util.xmq_quote_as_c(buffer_, start, stop, true)+" "+start_line+":"+start_col+"]");
    }

    void do_attr_value_compound_entity(int start_line, int start_col, int start, int stop, int stop_suffix)
    {
        System.out.print("[attr_value_compound_entity "+
                         Util.xmq_quote_as_c(buffer_, start, stop, true)+" "+start_line+":"+start_col+"]");
    }

    void do_comment(int start_line, int start_col, int start, int stop, int stop_suffix)
    {
        System.out.print("[comment "+
                         Util.xmq_quote_as_c(buffer_, start, stop, true)+" "+start_line+":"+start_col+"]");
    }

    void do_comment_continuation(int start_line, int start_col, int start, int stop, int stop_suffix)
    {
        System.out.print("[comment_continuation "+
                         Util.xmq_quote_as_c(buffer_, start, stop, true)+" "+start_line+":"+start_col+"]");
    }

    int count_xmq_quotes(char q)
    {
        int i = i_;
        while (i < buffer_len_ && buffer_.charAt(i) == q) i++;
        return i-i_;
    }

    void eat_xmq_quote()
    {
        // Grab ' or " to know which kind of quote it is.
        char q = buffer_.charAt(i_);

        assert(q == '\'' || q == '"');

        int depth = count_xmq_quotes(q);
        int count = depth;

        last_quote_start_ = i_;
        last_quote_start_line_ = line_;
        last_quote_start_col_ = col_;

        start_ = i_;

        while (count > 0)
        {
            increment(q);
            count--;
        }

        if (depth == 2)
        {
            // The empty quote ''
            stop_ = i_;
            return;
        }

        while (i_ < buffer_len_)
        {
            char c = buffer_.charAt(i_);
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
                stop_ = i_;
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
    }

    void parse_xmq_quote(XMQLevel level)
    {
        int start_line = line_;
        int start_col = col_;

        eat_xmq_quote();

        switch(level)
        {
        case LEVEL_XMQ:
            do_quote(start_line, start_col, start_, stop_, stop_);
            break;
        case LEVEL_ELEMENT_VALUE:
            do_element_value_quote(start_line, start_col, start_, stop_, stop_);
        break;
        case LEVEL_ELEMENT_VALUE_COMPOUND:
            do_element_value_compound_quote(start_line, start_col, start_, stop_, stop_);
            break;
        case LEVEL_ATTR_VALUE:
            do_attr_value_quote(start_line, start_col, start_, stop_, stop_);
        break;
        case LEVEL_ATTR_VALUE_COMPOUND:
            do_attr_value_compound_quote(start_line, start_col, start_, stop_, stop_);
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
            c = buffer_.charAt(i_);
            if (!Text.is_xmq_text_name(c)) break;
            if (!Text.is_lowercase_hex(c)) expect_semicolon = true;
            increment(c);
        }
        if (c == ';')
        {
            increment(c);
            c = buffer_.charAt(i_);
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
        return new Pair(n, fa);
    }

    void eat_xmq_comment_to_eol()
    {
        increment('/');
        increment('/');

        char c = 0;
        while (i_ < buffer_len_ && c != '\n')
        {
            c = buffer_.charAt(i_);
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
            c = buffer_.charAt(i_);
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
                c = buffer_.charAt(i_);
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

    void parse_xmq()
    {
        while (i_ < buffer_len_)
        {
            char c = buffer_.charAt(i_);
            char cc = 0;
            if ((c == '/' || c == '(') && i_+1 < buffer_len_) cc = buffer_.charAt(i_+1);
            if (is_xmq_token_whitespace(c)) parse_xmq_whitespace();
            else if (is_xmq_quote_start(c)) parse_xmq_quote(XMQLevel.LEVEL_XMQ);
            else if (is_xmq_entity_start(c)) parse_xmq_entity(XMQLevel.LEVEL_XMQ);
            else if (is_xmq_comment_start(c, cc)) parse_xmq_comment(cc);
            /*
            else if (is_xmq_element_start(c)) parse_xmq_element(state);
            else if (is_xmq_doctype_start(state->i, end)) parse_xmq_doctype(state);
            else if (is_xmq_pi_start(state->i, end)) parse_xmq_pi(state);
            else if (c == '}') return;
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
                }
                // longjmp(state->error_handler, 1);
            }
        }
    }
}
