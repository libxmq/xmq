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

package org.libxmq;

import java.util.Arrays;
import java.util.Locale;

/**
   Print log lines as xmq, like xmqLinePrintf in xmq.h.
   <pre>
   LineConfig lc = new LineConfig();
   lc.printf("car{", "nw=", "%d", 36, "model=", "%s", "car go", "}");  // car{nw=36 model='car go'}
   lc.printf("work=", "pi is %f", 3.14159);                           // work='pi is 3.141590'
   lc.printf("Fix me!");                                              // log='Fix me!'
   lc.setHumanReadable(true);
   lc.printf("work=", "pi is %f", 3.14159);                           // (work) pi is 3.141590
   </pre>
   After an element name ending with { follows a key ending with =, a format and the
   arguments the format uses, repeated, and finally "}". The formats are java formats
   (String.format, with Locale.ROOT like C printf), the values are quoted as needed.
*/
public class LineConfig
{
    private boolean human_readable;
    private final XMQ xmq = new XMQ();

    /**
       Create a line config that prints xmq.
     */
    public LineConfig()
    {
    }

    /**
       Print human readable lines instead of xmq, eg (work) pi is 3.141590.
       @param enabled True for human readable lines.
       @return This line config.
     */
    public LineConfig setHumanReadable(boolean enabled)
    {
        human_readable = enabled;
        return this;
    }

    /**
       Is the output human readable?
       @return True if human readable.
     */
    public boolean humanReadable()
    {
        return human_readable;
    }

    /**
       Print a line.
       @param element_name The element name, ending with { for keys and values,
                           with = for a single value, otherwise it is the text to log.
       @param args The keys, formats and arguments.
       @return The line.
       @throws IllegalArgumentException if a key does not end with =.
     */
    public String printf(String element_name, Object... args)
    {
        char c = element_name.isEmpty() ? ' ' : element_name.charAt(element_name.length() - 1);
        String name = element_name.isEmpty() ? "" : element_name.substring(0, element_name.length() - 1);

        if (c != '{')
        {
            String text;
            if (c != '=')
            {
                // The element name is the text to log.
                text = String.format(Locale.ROOT, element_name, args);
                name = "log";
            }
            else
            {
                text = args.length > 0 ? String.format(Locale.ROOT, (String)args[0], Arrays.copyOfRange(args, 1, args.length)) : "";
            }
            if (human_readable) return prefixLines("(" + name + ") ", text);
            return name + "=" + xmq.compactQuote(text);
        }

        StringBuilder b = new StringBuilder();
        if (!human_readable) b.append(element_name);
        int i = 0;
        while (i < args.length)
        {
            String key = (String)args[i++];
            if (key.startsWith("}")) break;
            if (!key.endsWith("="))
            {
                throw new IllegalArgumentException("Line printf key \"" + key + "\" does not end with = sign!");
            }
            String format = i < args.length ? (String)args[i++] : "";
            int n = Math.min(numArgs(format), args.length - i);
            String value = String.format(Locale.ROOT, format, Arrays.copyOfRange(args, i, i + n));
            i += n;

            if (human_readable)
            {
                if (b.length() > 0) b.append(' ');
                b.append(key, 0, key.length() - 1).append(':').append(value);
                continue;
            }
            char last = b.charAt(b.length() - 1);
            if (last != '"' && last != '\'' && last != '(' && last != ')' && last != '{' && last != '}') b.append(' ');
            b.append(key).append(xmq.compactQuote(value));
        }
        if (human_readable) return prefixLines("(" + name + ") ", b.toString());
        return b.append('}').toString();
    }

    // The number of arguments used by a format, %% and %n use none.
    private static int numArgs(String format)
    {
        int n = 0;
        for (int i = 0; i < format.length() - 1; i++)
        {
            if (format.charAt(i) != '%') continue;
            char next = format.charAt(i + 1);
            if (next != '%' && next != 'n') n++;
            i++;
        }
        return n;
    }

    private static String prefixLines(String prefix, String text)
    {
        return prefix + text.replace("\n", "\n" + prefix);
    }
}
