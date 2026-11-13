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

import java.math.BigDecimal;
import java.math.MathContext;
import java.math.RoundingMode;

/**
 * Conversion helpers for the xpath engine: number formatting and parsing,
 * string values of nodes and character classes. Ported from libxml2
 * xpath.c (Copyright (C) 1998-2012 Daniel Veillard, MIT license).
 */
final class XPathUtil
{
    static final String XML_NAMESPACE = "http://www.w3.org/XML/1998/namespace";

    private XPathUtil()
    {
    }

    /** Same as libxml2 IS_BLANK_CH. */
    static boolean isBlank(int c)
    {
        return c == 0x20 || c == 0x09 || c == 0x0A || c == 0x0D;
    }

    static boolean isAsciiDigit(int c)
    {
        return c >= '0' && c <= '9';
    }

    static boolean isAsciiLetter(int c)
    {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
    }

    /** Approximation of libxml2 IS_LETTER (xml 1.0 BaseChar | Ideographic). */
    static boolean isLetter(int c)
    {
        if (c < 0x100)
        {
            return isAsciiLetter(c) || (c >= 0xC0 && c <= 0xD6) || (c >= 0xD8 && c <= 0xF6) || (c >= 0xF8);
        }
        if (c > 0xFFFF) return false;
        return Character.isLetter(c);
    }

    /** Approximation of libxml2 IS_DIGIT. */
    static boolean isDigit(int c)
    {
        if (c < 0x100) return isAsciiDigit(c);
        if (c > 0xFFFF) return false;
        return Character.isDigit(c);
    }

    /** Approximation of libxml2 IS_COMBINING. */
    static boolean isCombining(int c)
    {
        if (c < 0x300 || c > 0xFFFF) return false;
        int t = Character.getType(c);
        return t == Character.NON_SPACING_MARK || t == Character.COMBINING_SPACING_MARK
            || t == Character.ENCLOSING_MARK;
    }

    /** Same as libxml2 IS_EXTENDER. */
    static boolean isExtender(int c)
    {
        return c == 0xB7 || c == 0x2D0 || c == 0x2D1 || c == 0x387 || c == 0x640 || c == 0xE46
            || c == 0xEC6 || c == 0x3005 || (c >= 0x3031 && c <= 0x3035) || (c >= 0x309D && c <= 0x309E)
            || (c >= 0x30FC && c <= 0x30FE);
    }

    /**
     * Format a number like libxml2 xmlXPathFormatNumber, eg 1 gives "1",
     * 0.5 gives "0.5", 1e10 gives "1e+10".
     */
    static String formatNumber(double number)
    {
        if (Double.isInfinite(number)) return number > 0 ? "Infinity" : "-Infinity";
        if (Double.isNaN(number)) return "NaN";
        if (number == 0) return "0";
        if (number > Integer.MIN_VALUE && number < Integer.MAX_VALUE && number == (int)number)
        {
            return Integer.toString((int)number);
        }
        double abs = Math.abs(number);
        StringBuilder work = new StringBuilder();
        String exponent = "";
        if ((abs > 1e9 || abs < 1e-5) && abs != 0.0)
        {
            // Like printf("%.14e"), 15 significant digits.
            BigDecimal bd = new BigDecimal(number).round(new MathContext(15, RoundingMode.HALF_EVEN));
            String digits = bd.unscaledValue().abs().toString();
            int exp = digits.length() - bd.scale() - 1;
            StringBuilder d = new StringBuilder(digits);
            while (d.length() < 15) d.append('0');
            if (d.length() > 15) d.setLength(15);
            if (number < 0) work.append('-');
            work.append(d.charAt(0)).append('.').append(d, 1, 15);
            String e = Integer.toString(Math.abs(exp));
            if (e.length() < 2) e = "0" + e;
            exponent = "e" + (exp < 0 ? "-" : "+") + e;
        }
        else
        {
            int integer_place = (int)Math.log10(abs);
            int fraction_place;
            if (integer_place > 0) fraction_place = 15 - integer_place - 1;
            else fraction_place = 15 - integer_place;
            BigDecimal bd = new BigDecimal(number).setScale(fraction_place, RoundingMode.HALF_EVEN);
            work.append(bd.toPlainString());
        }
        // Remove fractional trailing zeroes.
        int dot = work.indexOf(".");
        if (dot >= 0)
        {
            int end = work.length();
            while (end > dot + 1 && work.charAt(end - 1) == '0') end--;
            if (end == dot + 1) end = dot;
            work.setLength(end);
        }
        return work.append(exponent).toString();
    }

    /**
     * Parse a string into a number like libxml2 xmlXPathStringEvalNumber.
     * Leading and trailing blanks are allowed, as is an exponent.
     */
    static double stringToNumber(String str)
    {
        if (str == null) return 0;
        int n = str.length();
        int cur = 0;
        boolean ok = false;
        boolean isneg = false;
        int exponent = 0;
        boolean is_exponent_negative = false;
        while (cur < n && isBlank(str.charAt(cur))) cur++;
        int c = cur < n ? str.charAt(cur) : 0;
        if (c != '.' && (c < '0' || c > '9') && c != '-') return Double.NaN;
        if (c == '-')
        {
            isneg = true;
            cur++;
        }
        double ret = 0;
        while (cur < n && isAsciiDigit(str.charAt(cur)))
        {
            ret = ret * 10;
            ret = ret + (double)(str.charAt(cur) - '0');
            ok = true;
            cur++;
        }
        if (cur < n && str.charAt(cur) == '.')
        {
            int frac = 0;
            double fraction = 0;
            cur++;
            if ((cur >= n || !isAsciiDigit(str.charAt(cur))) && !ok) return Double.NaN;
            while (cur < n && str.charAt(cur) == '0')
            {
                frac++;
                cur++;
            }
            int max = frac + 20;
            while (cur < n && isAsciiDigit(str.charAt(cur)) && frac < max)
            {
                fraction = fraction * 10 + (str.charAt(cur) - '0');
                frac++;
                cur++;
            }
            fraction /= Math.pow(10.0, frac);
            ret = ret + fraction;
            while (cur < n && isAsciiDigit(str.charAt(cur))) cur++;
        }
        if (cur < n && (str.charAt(cur) == 'e' || str.charAt(cur) == 'E'))
        {
            cur++;
            if (cur < n && str.charAt(cur) == '-')
            {
                is_exponent_negative = true;
                cur++;
            }
            else if (cur < n && str.charAt(cur) == '+')
            {
                cur++;
            }
            while (cur < n && isAsciiDigit(str.charAt(cur)))
            {
                if (exponent < 1000000) exponent = exponent * 10 + (str.charAt(cur) - '0');
                cur++;
            }
        }
        while (cur < n && isBlank(str.charAt(cur))) cur++;
        if (cur != n) return Double.NaN;
        if (isneg) ret = -ret;
        if (is_exponent_negative) exponent = -exponent;
        ret *= Math.pow(10.0, (double)exponent);
        return ret;
    }

    /** Like libxml2 xmlXPathCastNumberToBoolean. */
    static boolean numberToBoolean(double d)
    {
        return !Double.isNaN(d) && d != 0.0;
    }

    /** The string value of a node, like libxml2 xmlNodeGetContent. */
    static String stringValue(XmlNode n)
    {
        String s = n.textContent();
        return s == null ? "" : s;
    }
}
