/* libxmq - Copyright (C) 2026 Fredrik Öhrström (spdx: MIT)

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

/**
 * Join the command line arguments into a single line that is parsed with the
 * xmq command line grammar xmqcli.ixml. This must be identical to the C
 * function xmq_cli_join_argv in src/main/c/xmq-cli-join.h.
 *
 * The line starts with xmq, then every argument follows separated by a single
 * space. An argument that is empty or contains whitespace, a quote, a double
 * quote or a backslash is written inside double quotes, where " and \ are
 * escaped with a backslash. For an option with a value, --name=value, only
 * the value is quoted (an empty value is not quoted).
 */
public final class CliJoin
{
    private CliJoin()
    {
    }

    static boolean needs_quotes(String s, boolean is_option_value)
    {
        if (s.isEmpty()) return !is_option_value;
        for (int i = 0; i < s.length(); i++)
        {
            char c = s.charAt(i);
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\'' || c == '"' || c == '\\') return true;
        }
        return false;
    }

    static void append_quoted(StringBuilder sb, String s)
    {
        sb.append('"');
        for (int i = 0; i < s.length(); i++)
        {
            char c = s.charAt(i);
            if (c == '"' || c == '\\') sb.append('\\');
            sb.append(c);
        }
        sb.append('"');
    }

    /**
     * Join the arguments (without the program name) into a command line.
     * @param args The arguments, ie argv[1..].
     * @return The command line, starting with xmq.
     */
    public static String join(String[] args)
    {
        StringBuilder sb = new StringBuilder("xmq");
        for (String arg : args)
        {
            sb.append(' ');
            int eq = arg.indexOf('=');
            if (arg.startsWith("--") && eq >= 0)
            {
                // An option --name=value, quote only the value.
                sb.append(arg, 0, eq + 1);
                String v = arg.substring(eq + 1);
                if (needs_quotes(v, true)) append_quoted(sb, v);
                else sb.append(v);
            }
            else if (needs_quotes(arg, false))
            {
                append_quoted(sb, arg);
            }
            else
            {
                sb.append(arg);
            }
        }
        return sb.toString();
    }

    /**
     * Print the joined command line, used by the tests.
     * @param args The arguments to join.
     */
    public static void main(String[] args)
    {
        System.out.println(join(args));
    }
}
