/* libxmq - Copyright (C) 2026 Fredrik Öhrström (spdx: MIT) */

package org.libxmq.imp;

import java.util.ArrayList;
import java.util.List;

import org.libxmq.ParseErrorCode;
import org.libxmq.ParseException;

/**
 * Parse (a subset of) yaml into the xmq DOM, a port of the parser in yaml.c.
 *
 * Like the C code, the yaml is translated into calls of the same parse
 * callbacks as the xmq parser uses, so the DOM is built by XMQParseIntoDOM.
 * The input is copied into a char array with a 0 sentinel, lines are 0
 * terminated when they are read and quoted scalars are decoded in place,
 * exactly like the private buffer of the C parser, so that the scanning code
 * can be kept close to the C code (indexes instead of pointers).
 *
 * Supported: nested mappings, nested sequences, plain, single and double
 * quoted scalars, comments, flow collections, block scalars | and >, anchors
 * (id attribute), aliases (ref attribute) and tags (tag and ltag attributes).
 * All content is text, see yaml.c.
 */
public class YamlParser extends XMQParseIntoDOM
{
    /**
     * Creates the yaml parser.
     */
    public YamlParser()
    {
        super();
        setNoTrimQuotes(true);
    }

    char[] buf;     // The input with a 0 sentinel, lines are 0 terminated when read.
    String src;     // The original input, for error messages.
    int cursor;     // Start of the next line to read.
    int line_start; // Start of the current line.
    int text;       // First non-space char of the current line.
    int indent;     // Distance from line_start to text.
    int line;       // Current line number.
    boolean eof;
    int bend;       // The position of the sentinel.

    /** A buffered full line comment awaiting placement. */
    static final class YComment
    {
        int indent;
        final String text;
        YComment(int indent, String text) { this.indent = indent; this.text = text; }
    }
    final List<YComment> cmts = new ArrayList<>();

    interface Callback
    {
        void call(int start_line, int start_col, int start, int stop, int stop_suffix);
    }

    // Invoke a callback with the string s, like DO_CALLBACK_SIM in C.
    void sim(Callback cb, String s)
    {
        String saved = buffer_;
        buffer_ = s;
        try
        {
            cb.call(line_, col_, 0, s.length(), s.length());
        }
        finally
        {
            buffer_ = saved;
        }
    }

    /**
     * Parse yaml.
     * @param input The yaml source.
     * @param source A description of where the buffer comes from, for error messages.
     * @return True on success.
     * @throws ParseException If the yaml cannot be parsed.
     */
    public boolean parseYaml(String input, String source) throws ParseException
    {
        source_ = source;
        src = input;
        buffer_ = input;
        buffer_len_ = input.length();
        buf = new char[input.length() + 1];
        input.getChars(0, input.length(), buf, 0);
        buf[input.length()] = 0;
        bend = input.length();
        cursor = 0;
        line = 0;
        line_ = 1;
        col_ = 1;
        try
        {
            setup();
            yparse_document();
        }
        catch (XMQRuntimeException e)
        {
            throw e.getParseException();
        }
        return true;
    }

    // ------------------------------------------------------------ helpers

    char at(int i)
    {
        return i >= 0 && i < buf.length ? buf[i] : 0;
    }

    static boolean yspace(char c)
    {
        return c == ' ' || c == '\t';
    }

    // The 0 terminated string at i.
    String str(int i)
    {
        int e = i;
        while (e < buf.length && buf[e] != 0) e++;
        return new String(buf, i, e - i);
    }

    String str(int i, int e)
    {
        return new String(buf, i, e - i);
    }

    static int yhex(char c)
    {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return 0;
    }

    // Like yfail: an invalid char error at the start of the current line text,
    // reported with the indentation as the column.
    void yfail(String msg)
    {
        int pos = Math.min(text, src.length());
        int ls = pos == 0 ? 0 : src.lastIndexOf('\n', pos - 1) + 1;
        int le = src.indexOf('\n', pos);
        if (le < 0) le = src.length();
        String l = src.substring(ls, le);
        int char_index = pos < src.length() ? pos - ls : -1;
        throw new XMQRuntimeException(new ParseException(ParseErrorCode.XMQ_ERROR_INVALID_CHAR, l, line, indent,
                                                         source_, msg, char_index));
    }

    // ------------------------------------------------------------ comments

    void yemit_comment(String s)
    {
        int a = 0, b = s.length();
        while (a < b && yspace(s.charAt(a))) a++;
        while (b > a && yspace(s.charAt(b - 1))) b--;
        if (a == b) return;
        sim(this::do_comment, s.substring(a, b));
    }

    void ybuf_comment(int ind, String s)
    {
        if (s.isEmpty()) return;
        cmts.add(new YComment(ind, s));
    }

    void yflush_comments(int ind)
    {
        for (int i = 0; i < cmts.size();)
        {
            YComment c = cmts.get(i);
            if (c.indent == ind)
            {
                yemit_comment(c.text);
                cmts.remove(i);
            }
            else i++;
        }
    }

    void yretag_comments(int ind)
    {
        for (YComment c : cmts) c.indent = ind;
    }

    // A trailing ' #' comment not inside a quoted scalar, -1 if none.
    int yclfind_comment(int lineStart)
    {
        char q = 0;
        for (int i = lineStart; buf[i] != 0; i++)
        {
            char c = buf[i];
            if (q == '\'')
            {
                if (c == '\'')
                {
                    if (at(i + 1) == '\'') i++;
                    else q = 0;
                }
            }
            else if (q == '"')
            {
                if (c == '\\' && at(i + 1) != 0) i++;
                else if (c == '"') q = 0;
            }
            else
            {
                if (c == '\'' || c == '"') q = c;
                else if (c == '#' && (i == lineStart || buf[i - 1] == ' ' || buf[i - 1] == '\t')) return i;
            }
        }
        return -1;
    }

    void yinline()
    {
        int c = yclfind_comment(text);
        if (c >= 0) yemit_comment(str(c + 1));
    }

    // ------------------------------------------------------------ lines

    // Advance to the next significant line, skipping blank and comment lines.
    void ynext()
    {
        for (;;)
        {
            int s = cursor;
            int e = s;
            while (buf[e] != 0 && buf[e] != '\n') ++e;
            boolean at_end = buf[e] == 0 && e == bend;
            buf[e] = 0;
            if (e > s && buf[e - 1] == '\r') buf[e - 1] = 0;
            if (!at_end) ++e;
            cursor = e;

            line_start = s;
            line++;
            text = s;
            while (buf[text] == ' ') ++text;
            indent = text - s;

            if (buf[text] == '\t') yfail("YAML: tab indentation is not supported");
            if (buf[text] == '%'
                || (buf[text] == '-' && at(text + 1) == '-' && at(text + 2) == '-' && (at(text + 3) == 0 || yspace(at(text + 3))))
                || (buf[text] == '.' && at(text + 1) == '.' && at(text + 2) == '.' && at(text + 3) == 0))
            {
                yfail("YAML: document markers and directives are not supported");
            }

            line_ = line;
            col_ = indent;

            if (buf[text] != 0 && buf[text] != '#') return;
            if (buf[text] == '#') ybuf_comment(indent, str(text + 1));
            if (at_end)
            {
                eof = true;
                return;
            }
        }
    }

    // The colon in a "key: value" line, -1 if none.
    int ycolon(int s)
    {
        boolean sq = false, dq = false;
        while (buf[s] != 0)
        {
            char c = buf[s];
            switch (c)
            {
            case ' ', '\t' -> { ++s; continue; }
            case '#' -> { return -1; }
            case '\'' -> {
                if (sq && at(s + 1) == '\'') { s += 2; continue; }
                sq = !sq;
                ++s;
                continue;
            }
            case '"' -> { dq = !dq; ++s; continue; }
            case '\\' -> { s += at(s + 1) != 0 ? 2 : 1; continue; }
            case ':' -> {
                if (!sq && !dq && (at(s + 1) == ' ' || at(s + 1) == '\t' || at(s + 1) == 0)) return s;
                ++s;
                continue;
            }
            default -> { }
            }
            ++s;
        }
        return -1;
    }

    boolean ydash(int s)
    {
        return at(s) == '-' && (at(s + 1) == ' ' || at(s + 1) == '\t' || at(s + 1) == 0);
    }

    // ------------------------------------------------------------ scalars

    // Parse the scalar at s, decoding quoted scalars in place, return the value.
    String yscalar(int s)
    {
        char c0 = buf[s];
        if (c0 == '{' || c0 == '[') yfail("YAML: flow style collections [ ] and { } are not supported");
        if (c0 == '|' || c0 == '>') yfail("YAML: block scalars (| and >) are not supported");
        if (c0 == '\'')
        {
            int o = s + 1, e = s + 1;
            for (;;)
            {
                char c = buf[e];
                if (c == 0) yfail("YAML: single quoted scalar is not closed");
                if (c == '\'')
                {
                    if (at(e + 1) == '\'')
                    {
                        buf[o++] = '\'';
                        e += 2;
                        continue;
                    }
                    break;
                }
                buf[o++] = c;
                e++;
            }
            return str(s + 1, o);
        }
        if (c0 == '"')
        {
            int o = s + 1, e = s + 1;
            for (;;)
            {
                char c = buf[e++];
                if (c == 0) yfail("YAML: double quoted scalar is not closed");
                if (c == '"') break;
                if (c == '\\')
                {
                    char d = buf[e++];
                    if (d == 0) yfail("YAML: incomplete escape sequence");
                    switch (d)
                    {
                    case 'b' -> buf[o++] = 8;
                    case 'f' -> buf[o++] = 12;
                    case 'n' -> buf[o++] = '\n';
                    case 'r' -> buf[o++] = '\r';
                    case 't' -> buf[o++] = '\t';
                    case 'u' -> {
                        if (at(e) != 0 && at(e + 1) != 0 && at(e + 2) != 0 && at(e + 3) != 0)
                        {
                            int v = (yhex(buf[e]) << 12) | (yhex(buf[e + 1]) << 8) | (yhex(buf[e + 2]) << 4) | yhex(buf[e + 3]);
                            e += 4;
                            buf[o++] = (char)v;
                        }
                        else
                        {
                            yfail("YAML: \\uXXXX needs 4 hex digits");
                        }
                    }
                    default -> buf[o++] = d; // Also / " and \\, an unknown escape passes through.
                    }
                    continue;
                }
                buf[o++] = c;
            }
            return str(s + 1, o);
        }
        // A plain scalar runs to the end of the line or a comment.
        int e = s;
        while (buf[e] != 0)
        {
            if (buf[e] == '#' && (e == s || yspace(buf[e - 1]))) break;
            ++e;
        }
        while (e > s && yspace(buf[e - 1])) --e;
        return str(s, e);
    }

    void ykey_attr(String ak, String v)
    {
        sim(this::do_apar_left, "(");
        sim(this::do_attr_key, ak);
        sim(this::do_attr_value_quote, v);
        sim(this::do_apar_right, ")");
    }

    void ykey(String k)
    {
        String us = null;
        int colon = -1;
        if (k == null || k.isEmpty())
        {
            k = "_";
        }
        else
        {
            colon = JsonParser.element_name_colon(k);
            if (colon == -2)
            {
                us = k;
                k = "_";
                colon = -1;
            }
        }
        if (us == null && colon >= 0)
        {
            sim(this::do_element_ns, k.substring(0, colon));
            k = k.substring(colon + 1);
        }
        sim(this::do_element_key, k);
        if (us != null) ykey_attr("_", us);
    }

    // The raw length of a closed quoted scalar at s (quotes included), 0 if not closed.
    int yquote_raw(int s)
    {
        int e = s + 1;
        if (buf[s] == '\'')
        {
            for (;;)
            {
                char c = buf[e];
                if (c == 0) return 0;
                if (c == '\'')
                {
                    if (at(e + 1) == '\'') { e += 2; continue; }
                    return e - s + 1;
                }
                e++;
            }
        }
        for (;;)
        {
            if (buf[e] == 0) return 0;
            if (buf[e] == '\\')
            {
                if (at(e + 1) == 0) return 0;
                e += 2;
                continue;
            }
            if (buf[e] == '"') return e - s + 1;
            e++;
        }
    }

    static boolean ylooks_like_native(String s)
    {
        if (s.equals("true") || s.equals("false") || s.equals("null")) return true;
        int len = s.length(), i = 0;
        if (i < len && (s.charAt(i) == '+' || s.charAt(i) == '-')) ++i;
        boolean any_digit = false;
        while (i < len && s.charAt(i) >= '0' && s.charAt(i) <= '9') { any_digit = true; ++i; }
        if (i < len && s.charAt(i) == '.')
        {
            ++i;
            while (i < len && s.charAt(i) >= '0' && s.charAt(i) <= '9') { any_digit = true; ++i; }
        }
        if (!any_digit) return false;
        if (i < len && (s.charAt(i) == 'e' || s.charAt(i) == 'E'))
        {
            ++i;
            if (i < len && (s.charAt(i) == '+' || s.charAt(i) == '-')) ++i;
            boolean exp_digit = false;
            while (i < len && s.charAt(i) >= '0' && s.charAt(i) <= '9') { exp_digit = true; ++i; }
            if (!exp_digit) return false;
        }
        return i == len;
    }

    void yforce_string()
    {
        sim(this::do_apar_left, "(");
        sim(this::do_attr_key, "S");
        sim(this::do_apar_right, ")");
    }

    void value_text(String v)
    {
        sim(this::do_element_value_text, v);
    }

    void seq_open()
    {
        sim(this::do_apar_left, "(");
        sim(this::do_attr_key, "A");
        sim(this::do_apar_right, ")");
        sim(this::do_brace_left, "{");
    }

    void brace_left()
    {
        sim(this::do_brace_left, "{");
    }

    void brace_right()
    {
        sim(this::do_brace_right, "}");
    }

    // A yaml tag !!name or !name becomes a tag or ltag attribute, returns the position after it.
    int ytag(int v)
    {
        if (buf[v] != '!' || at(v + 1) == 0) return v;
        boolean global = buf[v + 1] == '!';
        int name = global ? v + 2 : v + 1;
        int end = name;
        while (true)
        {
            char c = at(end);
            if (c == 0 || yspace(c) || c == '#' || c == ',' || c == '[' || c == ']' || c == '{' || c == '}') break;
            ++end;
        }
        if (end == name) return v;
        ykey_attr(global ? "tag" : "ltag", str(name, end));
        while (yspace(at(end))) ++end;
        return end;
    }

    // ------------------------------------------------------------ flow

    // Parse one flow entry value at e, returns the position after it.
    int yflow_value(int e)
    {
        e = ytag(e);
        if (buf[e] == '[' || buf[e] == '{') return yflow(e);
        if (buf[e] == '\'' || buf[e] == '"')
        {
            String v = yscalar(e);
            int after = e + yquote_raw(e);
            if (after <= e) yfail("YAML: a quoted scalar in a flow collection is not closed");
            if (ylooks_like_native(v)) yforce_string();
            value_text(v);
            return after;
        }
        // A plain scalar runs to the next top level , ] or }
        int e2 = e;
        int quote = 0;
        while (buf[e2] != 0)
        {
            char c = buf[e2];
            if (quote == 1)
            {
                if (c == '\'')
                {
                    if (at(e2 + 1) == '\'') { e2 += 2; continue; }
                    quote = 0;
                }
            }
            else if (quote == 2)
            {
                if (c == '\\' && at(e2 + 1) != 0) { e2 += 2; continue; }
                if (c == '"') quote = 0;
            }
            else if (c == '\'') quote = 1;
            else if (c == '"') quote = 2;
            else if (c == ',' || c == ']' || c == '}') break;
            e2++;
        }
        if (quote != 0) yfail("YAML: a quoted scalar in a flow collection is not closed");
        while (e2 > e && yspace(buf[e2 - 1])) e2--;
        value_text(str(e, e2));
        return e2;
    }

    // A flow mapping key in k..ke with an optional tag and quotes.
    void yflow_key(int k, int ke)
    {
        int tk = k;
        String tagname = null;
        boolean global = false;
        if (buf[tk] == '!' && at(tk + 1) != 0)
        {
            int name = buf[tk + 1] == '!' ? tk + 2 : tk + 1;
            global = buf[tk + 1] == '!';
            int end = name;
            while (true)
            {
                char c = at(end);
                if (c == 0 || end >= ke || yspace(c) || c == '#' || c == ',' || c == '[' || c == ']' || c == '{' || c == '}') break;
                ++end;
            }
            if (end != name)
            {
                tagname = str(name, end);
                while (yspace(at(end))) ++end;
                tk = end;
            }
        }
        String ktxt;
        if (buf[tk] == '\'' || buf[tk] == '"') ktxt = yscalar(tk);
        else ktxt = tk < ke ? str(tk, ke) : "";
        ykey(ktxt.isEmpty() ? null : ktxt);
        if (tagname != null) ykey_attr(global ? "tag" : "ltag", tagname);
    }

    // Parse a flow collection at s, returns the position after the closing bracket.
    int yflow(int s)
    {
        boolean is_seq = buf[s] == '[';
        char closeb = is_seq ? ']' : '}';
        int e = s + 1;
        if (is_seq)
        {
            sim(this::do_apar_left, "(");
            sim(this::do_attr_key, "A");
            sim(this::do_apar_right, ")");
        }
        brace_left();
        int after;
        for (;;)
        {
            for (;;)
            {
                while (yspace(buf[e])) e++;
                if (buf[e] != 0) break;
                ynext();
                if (eof) yfail("YAML: a flow collection is not closed");
                e = text;
            }
            if (buf[e] == closeb)
            {
                after = e + 1;
                break;
            }
            if (buf[e] == ',')
            {
                e++;
                continue;
            }
            if (is_seq)
            {
                ykey(null);
                e = yflow_value(e);
                continue;
            }
            int e0 = e;
            boolean explicit_key = false;
            if (buf[e0] == '?' && (at(e0 + 1) == 0 || yspace(at(e0 + 1))))
            {
                explicit_key = true;
                e0++;
                while (yspace(buf[e0])) e0++;
            }
            int c = e0;
            int quote = 0;
            while (buf[c] != 0)
            {
                char ch = buf[c];
                if (quote == 1)
                {
                    if (ch == '\'')
                    {
                        if (at(c + 1) == '\'') { c += 2; continue; }
                        quote = 0;
                    }
                }
                else if (quote == 2)
                {
                    if (ch == '\\' && at(c + 1) != 0) { c += 2; continue; }
                    if (ch == '"') quote = 0;
                }
                else if (ch == ':') break;
                else if (ch == '\'') quote = 1;
                else if (ch == '"') quote = 2;
                c++;
            }
            boolean colon_here = buf[c] != 0;
            int ks = c;
            while (ks > e0 && yspace(buf[ks - 1])) ks--;
            if (colon_here)
            {
                if (explicit_key) yflow_key(e0, ks);
                else ykey(ks > e0 ? str(e0, ks) : null);
                e = c + 1;
            }
            else
            {
                if (!explicit_key) yfail("YAML: a flow mapping entry needs the form 'key: value'");
                yflow_key(e0, ks);
                ynext();
                if (eof) yfail("YAML: a flow collection is not closed");
                e = text;
                if (buf[e] != ':') yfail("YAML: an explicit flow key must be followed by a ':'");
                e++;
            }
            while (yspace(buf[e])) e++;
            if (buf[e] == closeb || buf[e] == ',')
            {
                // An empty value.
                value_text("");
                if (buf[e] == closeb)
                {
                    after = e + 1;
                    break;
                }
                continue;
            }
            if (buf[e] == 0)
            {
                // An empty value, the collection may continue on the next line.
                value_text("");
                ynext();
                if (eof) yfail("YAML: a flow collection is not closed");
                e = text;
                continue;
            }
            e = yflow_value(e);
        }
        brace_right();
        return after;
    }

    // ------------------------------------------------------------ block scalars

    // Parse a | or > block scalar, see yblock in yaml.c.
    void yblock(char style, char chomp, int key_indent, int digit)
    {
        List<int[]> lines = new ArrayList<>(); // {start, count}
        int block_indent = digit != 0 ? key_indent + digit : 1;
        boolean content_seen = false;
        while (!eof)
        {
            int s = cursor;
            int e = s;
            while (buf[e] != 0 && buf[e] != '\n') ++e;
            boolean at_end = buf[e] == 0 && e == bend;
            int llen = e - s;
            if (e > s && buf[e - 1] == '\r') --llen;
            int ind = 0;
            while (ind < llen && buf[s + ind] == ' ') ++ind;
            boolean blank = e == s;
            boolean spc = e > s && ind == llen && block_indent != 0 && ind >= block_indent;
            if (!blank && !spc && ind <= key_indent) break; // A terminator.
            buf[s + llen] = 0;
            cursor = at_end ? e : e + 1;
            line++;
            line_ = line;
            col_ = 1;
            if (at_end) eof = true;
            if (blank) { lines.add(new int[] { s, 0 }); continue; }
            if (spc) { lines.add(new int[] { s, ind - block_indent }); continue; }
            if (buf[s + ind] == '\t')
            {
                text = s + ind;
                yfail("YAML: a tab is not allowed in block scalar indentation");
            }
            if (!content_seen)
            {
                if (digit == 0 && ind > block_indent) block_indent = ind;
                content_seen = true;
            }
            if (ind < block_indent)
            {
                text = s + ind;
                yfail("YAML: a block scalar line is indented less than the block indent");
            }
            lines.add(new int[] { s + block_indent, llen - block_indent });
        }

        StringBuilder o = new StringBuilder();
        int count = lines.size();
        if (style == '|')
        {
            for (int i = 0; i < count; ++i)
            {
                if (i > 0) o.append('\n');
                o.append(buf, lines.get(i)[0], lines.get(i)[1]);
            }
        }
        else
        {
            boolean prev_blank = false, prev_ws = false, prev_ind = false;
            for (int i = 0; i < count; ++i)
            {
                int ls = lines.get(i)[0], cnt = lines.get(i)[1];
                boolean allspace = true;
                for (int j = 0; j < cnt; ++j) if (buf[ls + j] != ' ') { allspace = false; break; }
                boolean blnk = cnt == 0;
                boolean ws = allspace && !blnk;
                boolean indented = !allspace && buf[ls] == ' ';
                if (blnk)
                {
                    o.append('\n');
                }
                else if (ws)
                {
                    if (i > 0 && !prev_blank) o.append('\n');
                    o.append('\n');
                    o.append(buf, ls, cnt);
                }
                else if (i == 0 || prev_blank)
                {
                    o.append(buf, ls, cnt);
                }
                else if (prev_ws || indented || prev_ind)
                {
                    o.append('\n');
                    o.append(buf, ls, cnt);
                }
                else
                {
                    o.append(' ');
                    o.append(buf, ls, cnt);
                }
                prev_blank = blnk;
                prev_ws = ws;
                prev_ind = indented;
            }
        }
        if (chomp != '+')
        {
            // Clip ends non empty content with exactly one line break, strip with none.
            int vlen = o.length();
            while (vlen > 0 && o.charAt(vlen - 1) == '\n') --vlen;
            o.setLength(vlen);
            if (chomp == 0 && vlen > 0) o.append('\n');
        }
        else
        {
            int blanks = 0;
            for (int i = count; i > 0; --i)
            {
                if (lines.get(i - 1)[1] != 0) break;
                ++blanks;
            }
            for (int i = 0; i < blanks; ++i) o.append('\n');
        }
        value_text(o.toString());
    }

    // Parse an optional block scalar header at v: returns {style, chomp, digit} or null.
    int[] block_header(int v)
    {
        char style = buf[v];
        char chomp = 0;
        int digit = 0;
        int r = v + 1;
        if (buf[r] == '-' || buf[r] == '+') { chomp = buf[r]; ++r; }
        if (buf[r] >= '1' && buf[r] <= '9') { digit = buf[r] - '0'; ++r; }
        if (buf[r] == '-' || buf[r] == '+') { chomp = buf[r]; ++r; }
        if (buf[r] != 0 && buf[r] != '#' && !yspace(buf[r]))
        {
            yfail("YAML: a block scalar header needs the form 'style' [chomp] [indent]");
        }
        return new int[] { style, chomp, digit };
    }

    // ------------------------------------------------------------ nodes

    void ynode(int v, int key_indent)
    {
        if (buf[v] != 0 && buf[v] != '#')
        {
            if (buf[v] == '|' || buf[v] == '>')
            {
                int[] h = block_header(v);
                yblock((char)h[0], (char)h[1], key_indent, h[2]);
                ynext();
                return;
            }
            if (buf[v] == '[' || buf[v] == '{')
            {
                yflow(v);
                ynext();
                return;
            }
            String s = yscalar(v);
            if ((buf[v] == '"' || buf[v] == '\'') && ylooks_like_native(s)) yforce_string();
            value_text(s);
            yinline();
            ynext();
            return;
        }
        ynext();
        if (eof || (indent <= key_indent && !(ydash(text) && indent == key_indent)))
        {
            // Not more indented, the value is empty.
            value_text("");
            return;
        }
        if (ydash(text))
        {
            int seq_indent = indent;
            yretag_comments(seq_indent);
            seq_open();
            yseq(seq_indent);
            yflush_comments(seq_indent);
            brace_right();
            return;
        }
        int map_indent = indent;
        yretag_comments(map_indent);
        brace_left();
        ymap(map_indent);
        yflush_comments(map_indent);
        brace_right();
    }

    int label_end(int lp)
    {
        int ll = 0;
        while (at(lp + ll) != 0 && !yspace(at(lp + ll)) && at(lp + ll) != '#') ++ll;
        return ll;
    }

    void yentry(String k, int v, int key_indent)
    {
        ykey(k);
        v = ytag(v);
        if (buf[v] == '&' && at(v + 1) != 0 && !yspace(at(v + 1)))
        {
            // An anchor becomes an id attribute.
            int ll = label_end(v + 1);
            ykey_attr("id", str(v + 1, v + 1 + ll));
            v += 1 + ll;
            while (yspace(buf[v])) ++v;
            ynode(v, key_indent);
            return;
        }
        if (buf[v] == '*' && at(v + 1) != 0 && !yspace(at(v + 1)))
        {
            // An alias becomes a ref attribute, the value is not expanded.
            int ll = label_end(v + 1);
            ykey_attr("ref", str(v + 1, v + 1 + ll));
            ynext();
            return;
        }
        ynode(v, key_indent);
    }

    void ymap(int ind)
    {
        yflush_comments(ind);
        while (!eof)
        {
            if (ydash(text) || indent != ind) break;
            yflush_comments(ind);
            if (buf[text] == '?' && (at(text + 1) == 0 || yspace(at(text + 1))))
            {
                // A complex key, ? key followed by : value
                int kk = text + 1;
                while (yspace(buf[kk])) ++kk;
                String key = yscalar(kk);
                ynext();
                if (buf[text] != ':' || (at(text + 1) != 0 && !yspace(at(text + 1))))
                {
                    yfail("YAML: a complex key must be followed by a ':' line with the value");
                }
                int v = text + 1;
                while (yspace(buf[v])) ++v;
                yentry(key, v, ind);
                continue;
            }
            int c = ycolon(text);
            if (c < 0) yfail("YAML: a mapping entry needs the form 'key: value'");
            int k = text, ks = c;
            while (ks > k && yspace(buf[ks - 1])) --ks;
            int v = c + 1;
            while (yspace(buf[v])) ++v;
            yentry(str(k, ks), v, ind);
        }
    }

    void yitem_map(int first, int ind)
    {
        int c = ycolon(first);
        if (c < 0) yfail("YAML: a mapping entry needs the form 'key: value'");
        int item_indent = first - line_start;
        brace_left();
        int k = first, ks = c;
        while (ks > k && yspace(buf[ks - 1])) --ks;
        int v = c + 1;
        while (yspace(buf[v])) ++v;
        yentry(str(k, ks), v, ind);
        while (!eof && !ydash(text) && indent == item_indent)
        {
            yflush_comments(item_indent);
            int c2 = ycolon(text);
            if (c2 < 0) yfail("YAML: a mapping entry needs the form 'key: value'");
            int k2 = text, ks2 = c2;
            while (ks2 > k2 && yspace(buf[ks2 - 1])) --ks2;
            int v2 = c2 + 1;
            while (yspace(buf[v2])) ++v2;
            yentry(str(k2, ks2), v2, ind);
        }
        yflush_comments(item_indent);
        brace_right();
    }

    void yitem(int dash, int ind)
    {
        int item = dash + 1;
        while (yspace(buf[item])) ++item;

        if (buf[item] == 0 || buf[item] == '#')
        {
            // An empty item, or a node value on more indented lines below.
            ykey(null);
            ynext();
            if (eof || indent <= ind)
            {
                value_text("");
                return;
            }
            if (ydash(text))
            {
                seq_open();
                yseq(indent);
                brace_right();
                return;
            }
            if (ycolon(text) >= 0)
            {
                yitem_map(text, ind);
                return;
            }
            value_text("");
            return;
        }

        if (buf[item] == '&' && at(item + 1) != 0 && !yspace(at(item + 1)))
        {
            // An anchor: - &label [value]
            int ll = label_end(item + 1);
            ykey(null);
            ykey_attr("id", str(item + 1, item + 1 + ll));
            item += 1 + ll;
            while (yspace(buf[item])) ++item;
            if (buf[item] != 0 && buf[item] != '#')
            {
                if (buf[item] == '[' || buf[item] == '{')
                {
                    yflow(item);
                    ynext();
                    return;
                }
                if (buf[item] == '|' || buf[item] == '>')
                {
                    int[] h = block_header(item);
                    yblock((char)h[0], (char)h[1], ind, h[2]);
                    ynext();
                    return;
                }
                if (ycolon(item) >= 0)
                {
                    yitem_map(item, ind);
                    return;
                }
                item = ytag(item);
                String s = yscalar(item);
                if ((buf[item] == '"' || buf[item] == '\'') && ylooks_like_native(s)) yforce_string();
                value_text(s);
                yinline();
                ynext();
                return;
            }
            ynext();
            if (eof || indent <= ind) return;
            if (ydash(text))
            {
                seq_open();
                yseq(indent);
                brace_right();
                return;
            }
            if (ycolon(text) >= 0)
            {
                yitem_map(text, ind);
                return;
            }
            return;
        }

        if (buf[item] == '*' && at(item + 1) != 0 && !yspace(at(item + 1)))
        {
            // An alias: - *label
            int ll = label_end(item + 1);
            ykey(null);
            ykey_attr("ref", str(item + 1, item + 1 + ll));
            item += 1 + ll;
            while (yspace(buf[item])) ++item;
            if (buf[item] != 0 && buf[item] != '#') yfail("YAML: an alias must stand alone");
            ynext();
            return;
        }

        if (buf[item] == '[' || buf[item] == '{')
        {
            ykey(null);
            yflow(item);
            ynext();
            return;
        }

        if (ydash(item))
        {
            // A nested sequence: - - x
            int inner_indent = item - line_start;
            ykey(null);
            seq_open();
            yitem(item, inner_indent);
            yseq(inner_indent);
            brace_right();
            return;
        }

        int c = ycolon(item);
        if (c < 0)
        {
            // - scalar or - |
            if (buf[item] == '|' || buf[item] == '>')
            {
                int[] h = block_header(item);
                ykey(null);
                yblock((char)h[0], (char)h[1], ind, h[2]);
                ynext();
                return;
            }
            ykey(null);
            item = ytag(item);
            String s = yscalar(item);
            if ((buf[item] == '"' || buf[item] == '\'') && ylooks_like_native(s)) yforce_string();
            value_text(s);
            yinline();
            ynext();
            return;
        }
        // A mapping item: - name: 1
        ykey(null);
        yitem_map(item, ind);
    }

    void yseq(int ind)
    {
        yflush_comments(ind);
        while (!eof && ydash(text) && indent == ind) yitem(text, ind);
    }

    void yparse_document()
    {
        ynext();
        if (eof) return; // An empty document.
        if (ydash(text))
        {
            int ti = indent;
            ykey(null);
            yretag_comments(ti);
            seq_open();
            yseq(ti);
            yflush_comments(ti);
            brace_right();
        }
        else if (ycolon(text) >= 0)
        {
            int ti = indent;
            ykey(null);
            yretag_comments(ti);
            brace_left();
            ymap(ti);
            yflush_comments(ti);
            brace_right();
        }
        else
        {
            // A scalar document.
            String s = yscalar(text);
            ykey(null);
            value_text(s);
            ynext();
        }
        if (!eof) yfail("YAML: unexpected content at top level indentation");
    }
}
