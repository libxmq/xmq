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

package org.libxmq.imp.xml;

/**
 * printf formatting with the semantics of the C library (glibc), for messages that must be
 * identical to the C version: a null string prints as "(null)", a null pointer as "(nil)".
 * Supported: %s %.*s %d %i %u %ld %lu %zu %c %x %02x %g %p %%.
 */
public final class CFormat
{
    private CFormat()
    {
    }

    /**
     * Format like snprintf.
     * @param fmt The C format.
     * @param args The arguments (Integer/Long for %d, String for %s, Double for %g, ...).
     * @return The formatted string.
     */
    public static String format(String fmt, Object... args)
    {
        StringBuilder sb = new StringBuilder();
        int ai = 0;
        int n = fmt.length();
        for (int i = 0; i < n; i++)
        {
            char c = fmt.charAt(i);
            if (c != '%')
            {
                sb.append(c);
                continue;
            }
            i++;
            if (i >= n) break;
            // Flags and width.
            boolean zero = false;
            if (fmt.charAt(i) == '0')
            {
                zero = true;
                i++;
            }
            int width = 0;
            while (i < n && Character.isDigit(fmt.charAt(i))) width = width * 10 + (fmt.charAt(i++) - '0');
            int precision = -1;
            if (i < n && fmt.charAt(i) == '.')
            {
                i++;
                if (i < n && fmt.charAt(i) == '*')
                {
                    precision = ((Number)args[ai++]).intValue();
                    i++;
                }
                else
                {
                    precision = 0;
                    while (i < n && Character.isDigit(fmt.charAt(i))) precision = precision * 10 + (fmt.charAt(i++) - '0');
                }
            }
            while (i < n && (fmt.charAt(i) == 'l' || fmt.charAt(i) == 'z' || fmt.charAt(i) == 'h')) i++;
            if (i >= n) break;
            char conv = fmt.charAt(i);
            String out;
            switch (conv)
            {
            case '%' -> out = "%";
            case 's' ->
            {
                Object o = args[ai++];
                String s = o == null ? "(null)" : o.toString();
                if (precision >= 0 && precision < s.length()) s = s.substring(0, precision);
                out = s;
            }
            case 'd', 'i', 'u' ->
            {
                Object o = args[ai++];
                out = o instanceof Boolean b ? (b ? "1" : "0") : Long.toString(((Number)o).longValue());
            }
            case 'c' ->
            {
                Object o = args[ai++];
                out = o instanceof Character ch ? ch.toString() : new String(Character.toChars(((Number)o).intValue()));
            }
            case 'x' -> out = Long.toHexString(((Number)args[ai++]).longValue());
            case 'p' ->
            {
                Object o = args[ai++];
                out = o == null ? "(nil)" : "0x" + Integer.toHexString(System.identityHashCode(o));
            }
            case 'g' -> out = formatG(((Number)args[ai++]).doubleValue());
            default -> out = "%" + conv;
            }
            for (int k = out.length(); k < width; k++) sb.append(zero ? '0' : ' ');
            sb.append(out);
        }
        return sb.toString();
    }

    /** C %g: 6 significant digits, trailing zeros removed, exponent form for small/large numbers. */
    static String formatG(double d)
    {
        if (Double.isNaN(d)) return "nan";
        if (Double.isInfinite(d)) return d > 0 ? "inf" : "-inf";
        if (d == 0) return (1 / d < 0) ? "-0" : "0";
        int exp = (int)Math.floor(Math.log10(Math.abs(d)));
        String s = String.format(java.util.Locale.ROOT, "%.5e", d);
        exp = Integer.parseInt(s.substring(s.indexOf('e') + 1));
        if (exp < -4 || exp >= 6)
        {
            String mant = s.substring(0, s.indexOf('e'));
            mant = stripZeros(mant);
            return mant + "e" + (exp < 0 ? "-" : "+") + (Math.abs(exp) < 10 ? "0" : "") + Math.abs(exp);
        }
        return stripZeros(String.format(java.util.Locale.ROOT, "%." + Math.max(0, 5 - exp) + "f", d));
    }

    private static String stripZeros(String s)
    {
        if (s.indexOf('.') < 0) return s;
        int e = s.length();
        while (s.charAt(e - 1) == '0') e--;
        if (s.charAt(e - 1) == '.') e--;
        return s.substring(0, e);
    }
}
