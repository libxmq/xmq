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

import org.w3c.dom.Node;

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
        if (start == -1 && stop == -1)
        {
            start = 0;
            stop = s.length();
        }
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

    public static boolean isValidElementName(String s)
    {
        if (s.length() == 0) return false;

        if (!isNameStartChar(s.charAt(0))) return false;

        for (int i = 1; i < s.length(); ++i)
        {
            if (!isNameChar(s.charAt(i))) return false;
        }

        return true;
    }

    private static boolean isNameStartChar(char c) {
        return (c == ':' || c == '_' ||
                (c >= 'A' && c <= 'Z') ||
                (c >= 'a' && c <= 'z') ||
                (c >= 0xC0 && c <= 0xD6) ||
                (c >= 0xD8 && c <= 0xF6) ||
                (c >= 0xF8 && c <= 0x2FF) ||
                (c >= 0x370 && c <= 0x37D) ||
                (c >= 0x37F && c <= 0x1FFF) ||
                (c >= 0x200C && c <= 0x200D) ||
                (c >= 0x2070 && c <= 0x218F) ||
                (c >= 0x2C00 && c <= 0x2FEF) ||
                (c >= 0x3001 && c <= 0xD7FF) ||
                (c >= 0xF900 && c <= 0xFDCF) ||
                (c >= 0xFDF0 && c <= 0xFFFD));
    }

    private static boolean isNameChar(char c) {
        return (isNameStartChar(c) ||
                c == '-' || c == '.' ||
                (c >= '0' && c <= '9') ||
                c == 0xB7 ||
                (c >= 0x0300 && c <= 0x036F) ||
                (c >= 0x203F && c <= 0x2040));
    }

    public static String getXPath(Node node)
    {
        if (node == null)
        {
            return null;
        }

        if (node.getNodeType() == Node.DOCUMENT_NODE)
        {
            return "/";
        }

        StringBuilder path = new StringBuilder();
        Node parent = node.getParentNode();

        if (parent != null && parent.getNodeType() != Node.DOCUMENT_NODE)
        {
            path.append(getXPath(parent));
        }

        if (path.length() > 0 && path.charAt(path.length() - 1) != '/')
        {
            path.append("/");
        }

        String nodeName = node.getNodeName();
        int index = getNodeIndex(node);
        path.append(nodeName);
        if (index > 1)
        {
            path.append("[").append(index).append("]");
        }

        return path.toString();
    }

    private static int getNodeIndex(Node node)
    {
        int index = 1;
        Node prev = node.getPreviousSibling();
        while (prev != null)
        {
            if (prev.getNodeType() == node.getNodeType() &&
                prev.getNodeName().equals(node.getNodeName()))
            {
                index++;
            }
            prev = prev.getPreviousSibling();
        }
        return index;
    }

    String xmq_trim_quote(String s, boolean is_xmq, boolean is_comment)
    {
        return "";
    }
}
