/* libxmq - Copyright (C) 2023-2026 Fredrik Öhrström (spdx: MIT) */

package org.libxmq.imp;

import org.libxmq.ParseErrorCode;
import org.libxmq.ParseException;
import org.libxmq.imp.xml.XmlElement;
import org.libxmq.imp.xml.XmlNode;
import org.libxmq.imp.xml.XmlNs;

/**
 * Parse json into the xmq DOM, a port of the parser in json.c.
 *
 * Like the C code the json is translated into calls of the same parse
 * callbacks as the xmq parser uses (element_key, apar_left, attr_key,
 * element_value_text...), so the DOM is built by XMQParseIntoDOM. The C code
 * invokes the callbacks with pointers to decoded strings (DO_CALLBACK_SIM),
 * here the callback buffer is temporarily replaced by the string, see sim.
 *
 * The json conventions: the key "_" holds the element name of an object
 * that has no key (the root, objects in arrays), "_name" is an attribute,
 * "_xmlns" a namespace declaration, "|" a text node, "//" a comment, "_//"
 * a comment outside of the root, "!DOCTYPE" the doctype, an array is an
 * element with the attribute A, a string that looks like a number or a
 * keyword gets the attribute S and a key that is not a valid element name
 * becomes _(_=key).
 */
public class JsonParser extends XMQParseIntoDOM
{
    /**
     * Creates the json parser.
     */
    public JsonParser()
    {
        super();
        setNoTrimQuotes(true);
    }

    boolean root_found_; // Set when the name of the root has been found.

    interface Callback
    {
        void call(int start_line, int start_col, int start, int stop, int stop_suffix);
    }

    // Invoke a callback with the string s[start..stop] (and the suffix up to
    // stop_suffix), like DO_CALLBACK_SIM in C.
    void sim(Callback cb, String s, int start, int stop, int stop_suffix)
    {
        String saved = buffer_;
        buffer_ = s;
        try
        {
            cb.call(line_, col_, start, stop, stop_suffix);
        }
        finally
        {
            buffer_ = saved;
        }
    }

    void sim(Callback cb, String s)
    {
        sim(cb, s, 0, s.length(), s.length());
    }

    /**
     * Parse json.
     * @param buf The json source.
     * @param source A description of where the buffer comes from, for error messages.
     * @return True on success.
     * @throws ParseException If the json cannot be parsed.
     */
    public boolean parseJson(String buf, String source) throws ParseException
    {
        source_ = source;
        buffer_ = buf;
        buffer_len_ = buf.length();
        i_ = 0;
        line_ = 1;
        col_ = 1;
        try
        {
            setup();
            parse_json(null);
            if (i_ < buffer_len_) error(ParseErrorCode.XMQ_ERROR_UNEXPECTED_CLOSING_BRACE);
        }
        catch (XMQRuntimeException e)
        {
            throw e.getParseException();
        }
        return true;
    }

    void error(ParseErrorCode code)
    {
        throw new XMQRuntimeException(new ParseException(code, buffer_.substring(line_start_, findNL(i_, 20)), line_, col_, source_));
    }

    char cur()
    {
        return i_ < buffer_len_ ? buffer_.charAt(i_) : 0;
    }

    void eat_whitespace()
    {
        while (i_ < buffer_len_)
        {
            char c = buffer_.charAt(i_);
            if (c != ' ' && c != '\t' && c != '\n' && c != '\r') break;
            increment(c);
        }
    }

    static boolean is_hex(char c)
    {
        return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
    }

    // Decode a json string, the position is at the starting quote.
    String eat_json_quote()
    {
        StringBuilder b = new StringBuilder();
        // Like C, an error is reported at the start of the string.
        int si = i_, sl = line_, sc = col_, sls = line_start_;
        increment('"');
        while (i_ < buffer_len_)
        {
            char c = buffer_.charAt(i_);
            if (c == '"')
            {
                increment(c);
                break;
            }
            if (c == '\\')
            {
                increment(c);
                c = cur();
                if (c == '"' || c == '\\' || c == 'b' || c == 'f' || c == 'n' || c == 'r' || c == 't' || c == '/')
                {
                    increment(c);
                    switch (c)
                    {
                    case 'b' -> c = 8;
                    case 'f' -> c = 12;
                    case 'n' -> c = 10;
                    case 'r' -> c = 13;
                    case 't' -> c = 9;
                    default -> { }
                    }
                    b.append(c);
                    continue;
                }
                else if (c == 'u')
                {
                    increment(c);
                    if (i_ + 3 < buffer_len_ && is_hex(buffer_.charAt(i_)) && is_hex(buffer_.charAt(i_ + 1))
                        && is_hex(buffer_.charAt(i_ + 2)) && is_hex(buffer_.charAt(i_ + 3)))
                    {
                        int uc = Integer.parseInt(buffer_.substring(i_, i_ + 4), 16);
                        for (int k = 0; k < 4; k++) increment(buffer_.charAt(i_));
                        b.append((char)uc);
                        continue;
                    }
                }
                i_ = si; line_ = sl; col_ = sc; line_start_ = sls;
                error(ParseErrorCode.XMQ_ERROR_JSON_INVALID_ESCAPE);
            }
            b.append(c);
            increment(c);
        }
        return b.toString();
    }

    // "path[32]" is the 32:nd element named path, drop the index suffix.
    static String trim_index_suffix(String key)
    {
        if (key == null || key.isEmpty() || key.charAt(key.length() - 1) != ']') return key;
        int i = key.length() - 2;
        while (i > 0 && key.charAt(i) >= '0' && key.charAt(i) <= '9' && key.charAt(i) != '[') i--;
        if (i > 0 && key.charAt(i) == '[') return key.substring(0, i);
        return key;
    }

    static boolean json_element_start(char c)
    {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
    }

    static boolean json_text_name(char c)
    {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')
            || c == '-' || c == '_' || c == '.' || c == ':' || c == '#';
    }

    // Like is_xmq_element_name, returns -2 if not a name, else the position of the (last) colon or -1.
    static int element_name_colon(String s)
    {
        if (s.isEmpty() || !json_element_start(s.charAt(0))) return -2;
        int colon = -1;
        for (int i = 1; i < s.length(); i++)
        {
            char c = s.charAt(i);
            if (!json_text_name(c)) return -2;
            if (c == ':') colon = i;
        }
        return colon;
    }

    /** The key as an element name: the name, the namespace prefix and the unsafe key, see key(). */
    record Key(String name, String prefix, String unsafe) { }

    // No key or the empty key is _, a key that is not an element name is _ with the
    // key stored in the attribute _ (unsafe).
    static Key key(String key)
    {
        if (key == null || key.isEmpty()) return new Key("_", null, null);
        int colon = element_name_colon(key);
        if (colon == -2) return new Key("_", null, key);
        if (colon >= 0) return new Key(key.substring(colon + 1), key.substring(0, colon), null);
        return new Key(key, null, null);
    }

    // element_ns (if any) and element_key.
    void element(Key k)
    {
        if (k.prefix() != null) sim(this::do_element_ns, k.prefix());
        sim(this::do_element_key, k.name());
    }

    // The attribute _=unsafe key.
    void unsafe_attr(Key k)
    {
        sim(this::do_attr_key, "_");
        sim(this::do_attr_value_quote, k.unsafe());
    }

    void parse_json(String key)
    {
        eat_whitespace();
        char c = cur();
        if (c == '"') parse_json_quote(key);
        else if (buffer_.startsWith("true", i_) || buffer_.startsWith("false", i_)) parse_json_value(key, buffer_.charAt(i_) == 't' ? 4 : 5);
        else if (buffer_.startsWith("null", i_)) parse_json_null(key);
        else if (is_jnumber(buffer_, i_, buffer_len_) >= 0) parse_json_value(key, is_jnumber(buffer_, i_, buffer_len_) - i_);
        else if (c == '{') parse_json_object(key);
        else if (c == '[') parse_json_array(key);
        else error(ParseErrorCode.XMQ_ERROR_JSON_INVALID_CHAR);
        eat_whitespace();
    }

    static boolean has_number_ended(char c)
    {
        return c == ' ' || c == '\n' || c == ',' || c == '}' || c == ']';
    }

    /**
     * Like is_jnumber in json.c.
     * @param s The string.
     * @param start Start position.
     * @param stop Stop position.
     * @return The position after the number, or -1 if not a number.
     */
    static int is_jnumber(String s, int start, int stop)
    {
        if (start == stop) return -1;
        boolean found_e = false, found_e_sign = false, leading_zero = false, last_is_digit = false, found_dot = false;
        int i;
        for (i = start; i < stop; ++i)
        {
            char c = s.charAt(i);
            last_is_digit = false;
            boolean current_is_not_digit = (c < '0' || c > '9');
            if (i == start)
            {
                if (current_is_not_digit && c != '-') return -1;
                if (c == '0') leading_zero = true;
                if (c != '-') last_is_digit = true;
                continue;
            }
            if (leading_zero)
            {
                leading_zero = false;
                if (has_number_ended(c)) return i;
                if (c != '.') return -1;
                found_dot = true;
            }
            else if (c == '.')
            {
                if (found_dot) return -1;
                found_dot = true;
            }
            else if (c == 'e' || c == 'E')
            {
                if (found_e) return -1;
                found_e = true;
            }
            else if (found_e && !found_e_sign)
            {
                if (has_number_ended(c)) return i;
                if (current_is_not_digit && c != '-' && c != '+') return -1;
                if (c == '+' || c == '-') found_e_sign = true;
                else last_is_digit = true;
            }
            else
            {
                found_e_sign = false;
                if (has_number_ended(c)) return i;
                if (current_is_not_digit) return -1;
                last_is_digit = true;
            }
        }
        if (!last_is_digit) return -1;
        return i;
    }

    /**
     * Is the whole string a json number, like json_is_number.
     * @param s The string.
     * @return True if s is a json number.
     */
    static boolean is_number(String s)
    {
        return s.length() == is_jnumber(s, 0, s.length());
    }

    static boolean is_keyword(String s)
    {
        return s.equals("true") || s.equals("false") || s.equals("null");
    }

    void parse_json_quote(String key)
    {
        int start_line = line_;
        int start_col = col_;
        String content = eat_json_quote();
        key = trim_index_suffix(key);

        if ("|".equals(key))
        {
            // "|":"text" is a pure text node.
            sim(this::do_quote, content);
            return;
        }
        if ("//".equals(key))
        {
            // "//":"text" is a comment.
            sim(this::do_comment, content);
            return;
        }
        if ("_//".equals(key))
        {
            // "_//":"text" is a comment before (or after) the root element.
            if (!root_found_) add_pre_node_before_ = top();
            else add_post_node_after_ = top();
            sim(this::do_comment, content);
            add_pre_node_before_ = null;
            add_post_node_after_ = null;
            return;
        }
        if ("_".equals(key))
        {
            // The element name "_":"name" of an object without a key.
            XmlNode container = top();
            int colon = element_name_colon(content);
            if (colon != -2 && container instanceof XmlElement e)
            {
                if (colon < 0)
                {
                    e.setName(content);
                }
                else
                {
                    String prefix = content.substring(0, colon);
                    e.setName(content.substring(colon + 1));
                    XmlNs ns = e.searchNs(prefix);
                    if (ns == null) ns = e.declareNs(prefix, null);
                    e.setNs(ns);
                }
                root_found_ = true;
                return;
            }
            System.err.print("xmq: Warning! \"_\":\"" + content + "\" cannot be converted into an valid element name!\n");
        }
        if (key != null && key.equals("!DOCTYPE") && !doctype_found_)
        {
            // The one and only !DOCTYPE.
            sim(this::do_element_key, key);
            add_pre_node_before_ = top();
            sim(this::do_element_value_quote, content);
            add_pre_node_before_ = null;
            return;
        }

        Key k = key(key);
        String name = k.unsafe() != null ? "_" : key;
        if (name != null && name.length() > 1 && name.charAt(0) == '_')
        {
            if (name.startsWith("_xmlns"))
            {
                // A namespace declaration "_xmlns:x":"uri" or the default "_xmlns":"uri".
                String decl = name.substring(1);
                int colon = decl.indexOf(':');
                if (colon >= 0) sim(this::do_ns_declaration, decl, 0, colon, decl.length());
                else sim(this::do_ns_declaration, decl);
                sim(this::do_attr_value_quote, content);
            }
            else
            {
                // An attribute stored as "_attr":"value".
                sim(this::do_attr_key, name.substring(1));
                sim(this::do_attr_value_quote, content);
            }
            return;
        }

        element(k);
        boolean need_string_type = content.length() > 0 && (is_keyword(content) || is_number(content));
        if (need_string_type || k.unsafe() != null)
        {
            // The string "false" not the boolean false, mark it with S.
            sim(this::do_apar_left, "(");
            if (k.unsafe() != null) unsafe_attr(k);
            if (need_string_type) sim(this::do_attr_key, "S");
            sim(this::do_apar_right, ")");
        }
        int sl = line_, sc = col_;
        line_ = start_line;
        col_ = start_col;
        sim(this::do_element_value_text, content);
        line_ = sl;
        col_ = sc;
    }

    void parse_json_null(String key)
    {
        for (char c : "null".toCharArray()) increment(c);
        key = trim_index_suffix(key);
        if (key != null && key.length() > 1 && key.charAt(0) == '_')
        {
            // {"_async":null} is the attribute async without a value.
            sim(this::do_attr_key, key.substring(1));
            return;
        }
        value(key, "null");
    }

    // true false or a number.
    void parse_json_value(String key, int len)
    {
        String v = buffer_.substring(i_, i_ + len);
        for (int k = 0; k < len; k++) increment(buffer_.charAt(i_));
        value(trim_index_suffix(key), v);
    }

    void value(String key, String v)
    {
        Key k = key(key);
        element(k);
        if (k.unsafe() != null)
        {
            sim(this::do_apar_left, "(");
            unsafe_attr(k);
            sim(this::do_apar_right, ")");
        }
        sim(this::do_element_value_text, v);
    }

    void parse_json_array(String key)
    {
        increment('[');
        Key k = key(trim_index_suffix(key));
        element(k);
        sim(this::do_apar_left, "(");
        if (k.unsafe() != null) unsafe_attr(k);
        sim(this::do_attr_key, "A");
        sim(this::do_apar_right, ")");
        sim(this::do_brace_left, "{");
        char c = ',';
        while (i_ < buffer_len_ && c == ',')
        {
            eat_whitespace();
            c = cur();
            if (c == ']') break;
            parse_json(null);
            c = cur();
            if (c == ',') increment(c);
        }
        if (c != ']') error(ParseErrorCode.XMQ_ERROR_JSON_INVALID_CHAR);
        increment(c);
        sim(this::do_brace_right, "}");
    }

    void parse_json_object(String key)
    {
        increment('{');
        Key k = key(trim_index_suffix(key));
        element(k);
        if (k.unsafe() != null)
        {
            sim(this::do_apar_left, "(");
            unsafe_attr(k);
            sim(this::do_apar_right, ")");
        }
        sim(this::do_brace_left, "{");
        char c = ',';
        while (i_ < buffer_len_ && c == ',')
        {
            eat_whitespace();
            c = cur();
            if (c == '}') break;
            if (c != '"') error(ParseErrorCode.XMQ_ERROR_JSON_INVALID_CHAR);
            // The key, ie speed in { "speed":123 }
            String member = eat_json_quote();
            eat_whitespace();
            c = cur();
            if (c != ':') error(ParseErrorCode.XMQ_ERROR_JSON_INVALID_CHAR);
            increment(c);
            parse_json(member);
            c = cur();
            if (c == ',') increment(c);
        }
        if (c != '}') error(ParseErrorCode.XMQ_ERROR_JSON_INVALID_CHAR);
        increment(c);
        sim(this::do_brace_right, "}");
    }
}
