package org.libxmq;

public class Util {

    final boolean is_xmq_quote_start(char c)
    {
        return c == '\'';
    }

    final boolean is_xmq_entity_start(char c)
    {
        return c == '&';
    }

    final boolean is_xmq_attribute_key_start(char c)
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

    final boolean is_xmq_compound_start(char c)
    {
        return (c == '(');
    }

    final boolean is_xmq_comment_start(char c, char cc)
    {
        return c == '/' && (cc == '/' || cc == '*');
    }

    /*
    private boolean is_xmq_doctype_start(const char *start, const char *stop)
    {
        if (*start != '!') return false;
        // !DOCTYPE
        if (start+8 > stop) return false;
        if (strncmp(start, "!DOCTYPE", 8)) return false;
        // Ooups, !DOCTYPE must have some value.
        if (start+8 == stop) return false;
        char c = *(start+8);
        // !DOCTYPE= or !DOCTYPE = etc
        if (c != '=' && c != ' ' && c != '\t' && c != '\n' && c != '\r') return false;
        return true;
    }
*/

    static String xmq_quote_as_c(String s, int start, int stop, boolean add_quotes)
    {
        if (start == stop)
        {
            // The empty string.
            if (!add_quotes) return "";
            return "\"\"";
        }
        StringBuilder sb = new StringBuilder();

        if (add_quotes) sb.append('"');
        if (stop > s.length()) stop = s.length();

        for (int i = start; i < stop; ++i)
        {
            char c = s.charAt(i);
            if (c == '\\') sb.append("\\\\");
            else if (c == '"') sb.append("\\\"");
            else if (c == '\n') sb.append("\\n");
            else if (c == '\t') sb.append("\\t");
            else if (c == '\r') sb.append("\\r");
            else sb.append(c);
        }

        if (add_quotes) sb.append('"');
        return sb.toString();
    }
}
