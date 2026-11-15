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

/* Portions of this file are ported from libxml2 (uri.c).
   libxml2 is covered by the following notice (from libxml2/Copyright):

 Copyright (C) 1998-2012 Daniel Veillard.  All Rights Reserved.
 Copyright (C) The Libxml2 Contributors.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is fur-
nished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FIT-
NESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/


package org.libxmq.imp.xml;

import java.nio.charset.StandardCharsets;
import java.util.Arrays;

/**
 * URI references (RFC 3986): parsing, saving and resolving against a base.
 *
 * This is a port of xml_uri.c, which in turn is a port of libxml2 uri.c.
 * It is used to resolve xml:base, xsl:import, xsl:include and document()
 * the way libxml2 xmlBuildURI does, and gives byte identical results to
 * the C version.
 *
 * The C code works on UTF-8 bytes, so internally everything is done on
 * UTF-8 byte arrays with indexes that mirror the pointer arithmetic of
 * the C code. Strings are converted with {@link #toUtf8(String)} and
 * {@link #fromUtf8(byte[])} at the public API. Unescaping %XX can produce
 * byte sequences that are not valid UTF-8 (eg "%FF"). Such bytes are kept
 * losslessly in the Java strings: every byte that is not part of a well
 * formed UTF-8 sequence is mapped to the lone surrogate U+DC00+byte (the
 * "surrogateescape" convention) and mapped back to the same byte when
 * the string is converted to UTF-8 again. Thus converting a result with
 * {@link #toUtf8(String)} always gives exactly the bytes the C version
 * produces, and for valid UTF-8 results the string is the normal decoding.
 * Other lone surrogates in input strings become '?'. A C string ends at
 * the first NUL byte, therefore input strings are cut at the first U+0000
 * and results are cut at the first NUL (eg from unescaping "%00").
 */
public final class XmlUri
{
    /** The scheme, eg "http", or null. */
    public String scheme;
    /** The opaque part, or null. */
    public String opaque;
    /** The authority part, or null. */
    public String authority;
    /** The server part, or null. */
    public String server;
    /** The user part, or null. */
    public String user;
    /** The port number, 0 if empty and -1 for an empty server. */
    public int port;
    /** The path string, or null. */
    public String path;
    /** The query string (unescaped), or null. */
    public String query;
    /** The fragment identifier, or null. */
    public String fragment;
    /** The parsing flags, 2 disables unescaping of the parts. */
    public int cleanup;
    /** The query string (as it appears in the URI), or null. */
    public String queryRaw;

    private static final int MAX_URI_LENGTH = 1024 * 1024;

    private static final int PORT_EMPTY = 0;
    private static final int PORT_EMPTY_SERVER = -1;

    private static final int XML_URI_ALLOW_UNWISE = 1;
    private static final int XML_URI_NO_UNESCAPE = 2;
    private static final int XML_URI_ALLOW_UCSCHAR = 4;

    private XmlUri()
    {
    }

    /* **********************************************************************
     *                                                                      *
     *                  UTF-8 and C string helpers                          *
     *                                                                      *
     ************************************************************************/

    /**
     * Convert a string to the UTF-8 bytes the C code works on. Lone
     * surrogates U+DC80..U+DCFF become the bytes 0x80..0xFF (see the class
     * documentation), other lone surrogates become '?'. The string is cut
     * at the first U+0000.
     *
     * @param s  the string, may be null
     * @return the UTF-8 bytes or null if s is null
     */
    public static byte[] toUtf8(String s)
    {
        if (s == null) return null;
        byte[] r = new byte[s.length() * 3];
        int o = 0;
        int n = s.length();
        for (int i = 0; i < n; i++)
        {
            char c = s.charAt(i);
            if (c == 0) break;
            if (c < 0x80)
            {
                r[o++] = (byte)c;
            }
            else if (c < 0x800)
            {
                r[o++] = (byte)(0xC0 | (c >> 6));
                r[o++] = (byte)(0x80 | (c & 0x3F));
            }
            else if (Character.isHighSurrogate(c) && i + 1 < n && Character.isLowSurrogate(s.charAt(i + 1)))
            {
                int cp = Character.toCodePoint(c, s.charAt(i + 1));
                i++;
                r[o++] = (byte)(0xF0 | (cp >> 18));
                r[o++] = (byte)(0x80 | ((cp >> 12) & 0x3F));
                r[o++] = (byte)(0x80 | ((cp >> 6) & 0x3F));
                r[o++] = (byte)(0x80 | (cp & 0x3F));
            }
            else if (c >= 0xDC80 && c <= 0xDCFF)
            {
                r[o++] = (byte)(c & 0xFF);
            }
            else if (Character.isSurrogate(c))
            {
                r[o++] = '?';
            }
            else
            {
                r[o++] = (byte)(0xE0 | (c >> 12));
                r[o++] = (byte)(0x80 | ((c >> 6) & 0x3F));
                r[o++] = (byte)(0x80 | (c & 0x3F));
            }
        }
        return Arrays.copyOf(r, o);
    }

    /**
     * Convert UTF-8 bytes to a string. Bytes that are not part of a well
     * formed UTF-8 sequence become the lone surrogates U+DC80..U+DCFF so
     * that {@link #toUtf8(String)} gives back exactly the same bytes.
     *
     * @param b  the bytes, may be null
     * @return the string or null if b is null
     */
    public static String fromUtf8(byte[] b)
    {
        if (b == null) return null;
        StringBuilder sb = new StringBuilder(b.length);
        int n = b.length;
        int i = 0;
        while (i < n)
        {
            int b0 = b[i] & 0xFF;
            if (b0 < 0x80)
            {
                sb.append((char)b0);
                i++;
                continue;
            }
            int need;
            int cp;
            if (b0 >= 0xC2 && b0 <= 0xDF) { need = 1; cp = b0 & 0x1F; }
            else if (b0 >= 0xE0 && b0 <= 0xEF) { need = 2; cp = b0 & 0x0F; }
            else if (b0 >= 0xF0 && b0 <= 0xF4) { need = 3; cp = b0 & 0x07; }
            else
            {
                sb.append((char)(0xDC00 + b0));
                i++;
                continue;
            }
            boolean ok = i + need < n;
            for (int k = 1; ok && k <= need; k++)
            {
                int bk = b[i + k] & 0xFF;
                if (bk < 0x80 || bk > 0xBF) ok = false;
                else cp = (cp << 6) | (bk & 0x3F);
            }
            if (ok)
            {
                if (need == 2) ok = cp >= 0x800 && (cp < 0xD800 || cp > 0xDFFF);
                else if (need == 3) ok = cp >= 0x10000 && cp <= 0x10FFFF;
            }
            if (!ok)
            {
                sb.append((char)(0xDC00 + b0));
                i++;
                continue;
            }
            sb.appendCodePoint(cp);
            i += need + 1;
        }
        return sb.toString();
    }

    /** The char at index i as a signed C char, 0 at or beyond the end (the terminating NUL). */
    private static int at(byte[] s, int i)
    {
        return i < s.length ? s[i] : 0;
    }

    /** The C string in buf starting at 0 and ending before the first NUL (or at len). */
    private static byte[] cstr(byte[] buf, int len)
    {
        for (int i = 0; i < len; i++)
        {
            if (buf[i] == 0) return Arrays.copyOf(buf, i);
        }
        return Arrays.copyOf(buf, len);
    }

    private static byte[] xmlUriStrndup(byte[] s, int start, int len)
    {
        if (s == null || len < 0) return null;
        return Arrays.copyOfRange(s, start, start + len);
    }

    /** Like xmlStrcat: append add to s, s may be null. */
    private static byte[] xmlUriStrcat(byte[] s, byte[] add, int addStart)
    {
        if (add == null) return s;
        if (s == null) return Arrays.copyOfRange(add, addStart, add.length);
        byte[] r = Arrays.copyOf(s, s.length + add.length - addStart);
        System.arraycopy(add, addStart, r, s.length, add.length - addStart);
        return r;
    }

    /** Port of xmlGrowCapacity (private/memory.h). */
    private static int xmlUriGrowCapacity(int capacity, int min, int max)
    {
        if (capacity <= 0) return min;
        if (capacity >= max) return -1;
        int extra = (capacity + 1) / 2;
        if (capacity > max - extra) return max;
        return capacity + extra;
    }

    /*
     * Old rule from 2396 used in legacy handling code
     * alpha    = lowalpha | upalpha
     */
    private static boolean isAlpha(int x)
    {
        return isLowalpha(x) || isUpalpha(x);
    }

    /*
     * lowalpha = "a" | "b" | "c" | "d" | "e" | "f" | "g" | "h" | "i" | "j" |
     *            "k" | "l" | "m" | "n" | "o" | "p" | "q" | "r" | "s" | "t" |
     *            "u" | "v" | "w" | "x" | "y" | "z"
     */
    private static boolean isLowalpha(int x)
    {
        return x >= 'a' && x <= 'z';
    }

    /*
     * upalpha = "A" | "B" | "C" | "D" | "E" | "F" | "G" | "H" | "I" | "J" |
     *           "K" | "L" | "M" | "N" | "O" | "P" | "Q" | "R" | "S" | "T" |
     *           "U" | "V" | "W" | "X" | "Y" | "Z"
     */
    private static boolean isUpalpha(int x)
    {
        return x >= 'A' && x <= 'Z';
    }

    /*
     * digit = "0" | "1" | "2" | "3" | "4" | "5" | "6" | "7" | "8" | "9"
     */
    private static boolean isDigit(int x)
    {
        return x >= '0' && x <= '9';
    }

    /*
     * alphanum = alpha | digit
     */
    private static boolean isAlphanum(int x)
    {
        return isAlpha(x) || isDigit(x);
    }

    /*
     * mark = "-" | "_" | "." | "!" | "~" | "*" | "'" | "(" | ")"
     */
    private static boolean isMark(int x)
    {
        return x == '-' || x == '_' || x == '.' || x == '!' || x == '~' || x == '*' || x == '\'' ||
            x == '(' || x == ')';
    }

    /*
     * unwise = "{" | "}" | "|" | "\" | "^" | "`"
     */
    private static boolean isUnwise(int x)
    {
        return x == '{' || x == '}' || x == '|' || x == '\\' || x == '^' || x == '[' || x == ']' ||
            x == '`';
    }

    /*
     * reserved = ";" | "/" | "?" | ":" | "@" | "&" | "=" | "+" | "$" | "," |
     *            "[" | "]"
     */
    private static boolean isReserved(int x)
    {
        return x == ';' || x == '/' || x == '?' || x == ':' || x == '@' || x == '&' || x == '=' ||
            x == '+' || x == '$' || x == ',' || x == '[' || x == ']';
    }

    /*
     * unreserved = alphanum | mark
     */
    private static boolean isUnreserved(int x)
    {
        return isAlphanum(x) || isMark(x);
    }

    /*
     * Skip to next pointer char, handle escaped sequences
     */
    private static int next(byte[] s, int p)
    {
        return at(s, p) == '%' ? p + 3 : p + 1;
    }

    /* **********************************************************************
     *                                                                      *
     *                         RFC 3986 parser                              *
     *                                                                      *
     ************************************************************************/

    private static boolean isaDigit(int c)
    {
        return c >= '0' && c <= '9';
    }

    private static boolean isaAlpha(int c)
    {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
    }

    private static boolean isaHexdig(int c)
    {
        return isaDigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
    }

    /*
     *    sub-delims    = "!" / "$" / "&" / "'" / "(" / ")"
     *                     / "*" / "+" / "," / ";" / "="
     */
    private static boolean isaSubDelim(int c)
    {
        return c == '!' || c == '$' || c == '&' || c == '(' || c == ')' || c == '*' || c == '+' ||
            c == ',' || c == ';' || c == '=' || c == '\'';
    }

    /*
     *    unreserved    = ALPHA / DIGIT / "-" / "." / "_" / "~"
     */
    private static boolean isaStrictlyUnreserved(int c)
    {
        return isaAlpha(c) || isaDigit(c) || c == '-' || c == '.' || c == '_' || c == '~';
    }

    /*
     *    pct-encoded   = "%" HEXDIG HEXDIG
     */
    private static boolean isaPctEncoded(byte[] s, int p)
    {
        return at(s, p) == '%' && isaHexdig(at(s, p + 1)) && isaHexdig(at(s, p + 2));
    }

    /*
     *    pchar         = unreserved / pct-encoded / sub-delims / ":" / "@"
     */
    private static boolean isaPchar(XmlUri u, byte[] s, int p)
    {
        int c = at(s, p);
        return xmlUriIsUnreserved(u, s, p) || isaPctEncoded(s, p) || isaSubDelim(c) || c == ':' || c == '@';
    }

    /*
     * From https://www.w3.org/TR/leiri/
     *
     * " " / "<" / ">" / '"' / "{" / "}" / "|"
     * / "\" / "^" / "`" / %x0-1F / %x7F-D7FF
     * / %xE000-FFFD / %x10000-10FFFF
     */
    private static boolean isaUcschar(int c)
    {
        return c <= 0x20 || c >= 0x7F || c == '<' || c == '>' || c == '"' || c == '{' || c == '}' ||
            c == '|' || c == '\\' || c == '^' || c == '`';
    }

    private static boolean xmlUriIsUnreserved(XmlUri uri, byte[] s, int cur)
    {
        if (uri == null) return false;

        int c = at(s, cur);
        if (isaStrictlyUnreserved(c)) return true;

        if ((uri.cleanup & XML_URI_ALLOW_UNWISE) != 0)
        {
            if (isUnwise(c)) return true;
        }
        else if ((uri.cleanup & XML_URI_ALLOW_UCSCHAR) != 0)
        {
            if (isaUcschar(c)) return true;
        }

        return false;
    }

    /** STRNDUP or unescape of a part, depending on XML_URI_NO_UNESCAPE. */
    private static String part(XmlUri uri, byte[] s, int start, int len)
    {
        if ((uri.cleanup & XML_URI_NO_UNESCAPE) != 0)
            return fromUtf8(xmlUriStrndup(s, start, len));
        return fromUtf8(xmlUriUnescapeString(s, start, len));
    }

    /**
     * Parse an URI scheme
     *
     * ALPHA *( ALPHA / DIGIT / "+" / "-" / "." )
     */
    private static int xmlUriParseScheme(XmlUri uri, byte[] s, int[] str)
    {
        int cur = str[0];
        if (!isaAlpha(at(s, cur))) return 1;
        cur++;

        while (isaAlpha(at(s, cur)) || isaDigit(at(s, cur)) ||
               at(s, cur) == '+' || at(s, cur) == '-' || at(s, cur) == '.') cur++;
        if (uri != null)
        {
            uri.scheme = fromUtf8(xmlUriStrndup(s, str[0], cur - str[0]));
        }
        str[0] = cur;
        return 0;
    }

    /**
     * Parse the query part of an URI
     *
     * fragment      = *( pchar / "/" / "?" )
     * NOTE: the strict syntax as defined by 3986 does not allow '[' and ']'
     *       in the fragment identifier but this is used very broadly for
     *       xpointer scheme selection, so we are allowing it here to not break
     *       for example all the DocBook processing chains.
     */
    private static int xmlUriParseFragment(XmlUri uri, byte[] s, int[] str)
    {
        int cur = str[0];

        while (isaPchar(uri, s, cur) || at(s, cur) == '/' || at(s, cur) == '?' ||
               at(s, cur) == '[' || at(s, cur) == ']')
            cur = next(s, cur);
        if (uri != null)
        {
            uri.fragment = part(uri, s, str[0], cur - str[0]);
        }
        str[0] = cur;
        return 0;
    }

    /**
     * Parse the query part of an URI
     *
     * query = *uric
     */
    private static int xmlUriParseQuery(XmlUri uri, byte[] s, int[] str)
    {
        int cur = str[0];

        while (isaPchar(uri, s, cur) || at(s, cur) == '/' || at(s, cur) == '?')
            cur = next(s, cur);
        if (uri != null)
        {
            uri.query = part(uri, s, str[0], cur - str[0]);

            /* Save the raw bytes of the query as well.
             * See: http://mail.gnome.org/archives/xml/2007-April/thread.html#00114
             */
            uri.queryRaw = fromUtf8(xmlUriStrndup(s, str[0], cur - str[0]));
        }
        str[0] = cur;
        return 0;
    }

    /**
     * Parse a port part and fills in the appropriate fields
     * of the uri structure
     *
     * port          = *DIGIT
     */
    private static int xmlUriParsePort(XmlUri uri, byte[] s, int[] str)
    {
        int cur = str[0];
        int port = 0;

        if (isaDigit(at(s, cur)))
        {
            while (isaDigit(at(s, cur)))
            {
                int digit = at(s, cur) - '0';

                if (port > Integer.MAX_VALUE / 10) return 1;
                port *= 10;
                if (port > Integer.MAX_VALUE - digit) return 1;
                port += digit;

                cur++;
            }
            if (uri != null) uri.port = port;
            str[0] = cur;
            return 0;
        }
        return 1;
    }

    /**
     * Parse an user information part and fills in the appropriate fields
     * of the uri structure
     *
     * userinfo      = *( unreserved / pct-encoded / sub-delims / ":" )
     */
    private static int xmlUriParseUserinfo(XmlUri uri, byte[] s, int[] str)
    {
        int cur = str[0];
        while (xmlUriIsUnreserved(uri, s, cur) || isaPctEncoded(s, cur) ||
               isaSubDelim(at(s, cur)) || at(s, cur) == ':')
            cur = next(s, cur);
        if (at(s, cur) == '@')
        {
            if (uri != null)
            {
                uri.user = part(uri, s, str[0], cur - str[0]);
            }
            str[0] = cur;
            return 0;
        }
        return 1;
    }

    /**
     *    dec-octet     = DIGIT                 ; 0-9
     *                  / %x31-39 DIGIT         ; 10-99
     *                  / "1" 2DIGIT            ; 100-199
     *                  / "2" %x30-34 DIGIT     ; 200-249
     *                  / "25" %x30-35          ; 250-255
     *
     * Skip a dec-octet, returns 0 if found and skipped, 1 otherwise.
     */
    private static int xmlUriParseDecOctet(byte[] s, int[] str)
    {
        int cur = str[0];

        if (!isaDigit(at(s, cur))) return 1;
        if (!isaDigit(at(s, cur + 1)))
            cur++;
        else if (at(s, cur) != '0' && isaDigit(at(s, cur + 1)) && !isaDigit(at(s, cur + 2)))
            cur += 2;
        else if (at(s, cur) == '1' && isaDigit(at(s, cur + 1)) && isaDigit(at(s, cur + 2)))
            cur += 3;
        else if (at(s, cur) == '2' && at(s, cur + 1) >= '0' &&
                 at(s, cur + 1) <= '4' && isaDigit(at(s, cur + 2)))
            cur += 3;
        else if (at(s, cur) == '2' && at(s, cur + 1) == '5' &&
                 at(s, cur + 2) >= '0' && at(s, cur + 1) <= '5')
            cur += 3;
        else
            return 1;
        str[0] = cur;
        return 0;
    }

    /**
     * Parse an host part and fills in the appropriate fields
     * of the uri structure
     *
     * host          = IP-literal / IPv4address / reg-name
     * IP-literal    = "[" ( IPv6address / IPvFuture  ) "]"
     * IPv4address   = dec-octet "." dec-octet "." dec-octet "." dec-octet
     * reg-name      = *( unreserved / pct-encoded / sub-delims )
     */
    private static int xmlUriParseHost(XmlUri uri, byte[] s, int[] str)
    {
        int cur = str[0];
        int host = cur;
        boolean found = false;

        /*
         * IPv6 and future addressing scheme are enclosed between brackets
         */
        if (at(s, cur) == '[')
        {
            cur++;
            while (at(s, cur) != ']' && at(s, cur) != 0) cur++;
            if (at(s, cur) != ']') return 1;
            cur++;
            found = true;
        }
        /*
         * try to parse an IPv4
         */
        if (!found && isaDigit(at(s, cur)))
        {
            int[] c = { cur };
            notIpv4:
            {
                if (xmlUriParseDecOctet(s, c) != 0) break notIpv4;
                if (at(s, c[0]) != '.') break notIpv4;
                c[0]++;
                if (xmlUriParseDecOctet(s, c) != 0) break notIpv4;
                if (at(s, c[0]) != '.') break notIpv4;
                if (xmlUriParseDecOctet(s, c) != 0) break notIpv4;
                if (at(s, c[0]) != '.') break notIpv4;
                if (xmlUriParseDecOctet(s, c) != 0) break notIpv4;
                cur = c[0];
                found = true;
            }
            if (!found) cur = str[0];
        }
        /*
         * then this should be a hostname which can be empty
         */
        if (!found)
        {
            while (xmlUriIsUnreserved(uri, s, cur) || isaPctEncoded(s, cur) || isaSubDelim(at(s, cur)))
                cur = next(s, cur);
        }
        // found:
        if (uri != null)
        {
            uri.authority = null;
            if (cur != host)
            {
                uri.server = part(uri, s, host, cur - host);
            }
            else
            {
                uri.server = null;
            }
        }
        str[0] = cur;
        return 0;
    }

    /**
     * Parse an authority part and fills in the appropriate fields
     * of the uri structure
     *
     * authority     = [ userinfo "@" ] host [ ":" port ]
     */
    private static int xmlUriParseAuthority(XmlUri uri, byte[] s, int[] str)
    {
        int[] cur = { str[0] };
        int ret;

        /*
         * try to parse an userinfo and check for the trailing @
         */
        ret = xmlUriParseUserinfo(uri, s, cur);
        if (ret < 0) return ret;
        if (ret != 0 || at(s, cur[0]) != '@')
            cur[0] = str[0];
        else
            cur[0]++;
        ret = xmlUriParseHost(uri, s, cur);
        if (ret != 0) return ret;
        if (at(s, cur[0]) == ':')
        {
            cur[0]++;
            ret = xmlUriParsePort(uri, s, cur);
            if (ret != 0) return ret;
        }
        str[0] = cur[0];
        return 0;
    }

    /**
     * Parse a segment and fills in the appropriate fields
     * of the uri structure
     *
     * segment       = *pchar
     * segment-nz    = 1*pchar
     * segment-nz-nc = 1*( unreserved / pct-encoded / sub-delims / "@" )
     *               ; non-zero-length segment without any colon ":"
     */
    private static int xmlUriParseSegment(XmlUri uri, byte[] s, int[] str, int forbid, boolean empty)
    {
        int cur = str[0];
        if (!isaPchar(uri, s, cur) || at(s, cur) == forbid)
        {
            if (empty) return 0;
            return 1;
        }
        cur = next(s, cur);

        // Windows drive letters are only allowed in the C version on _WIN32.

        while (isaPchar(uri, s, cur) && at(s, cur) != forbid)
            cur = next(s, cur);
        str[0] = cur;
        return 0;
    }

    /** Set uri.path to the part between start and cur, or null if empty. */
    private static void setPath(XmlUri uri, byte[] s, int start, int cur)
    {
        if (uri != null)
        {
            if (start != cur)
                uri.path = part(uri, s, start, cur - start);
            else
                uri.path = null;
        }
    }

    /**
     * Parse an path absolute or empty and fills in the appropriate fields
     * of the uri structure
     *
     * path-abempty  = *( "/" segment )
     */
    private static int xmlUriParsePathAbEmpty(XmlUri uri, byte[] s, int[] str)
    {
        int[] cur = { str[0] };
        int ret;

        while (at(s, cur[0]) == '/')
        {
            cur[0]++;
            ret = xmlUriParseSegment(uri, s, cur, 0, true);
            if (ret != 0) return ret;
        }
        setPath(uri, s, str[0], cur[0]);
        str[0] = cur[0];
        return 0;
    }

    /**
     * Parse an path absolute and fills in the appropriate fields
     * of the uri structure
     *
     * path-absolute = "/" [ segment-nz *( "/" segment ) ]
     */
    private static int xmlUriParsePathAbsolute(XmlUri uri, byte[] s, int[] str)
    {
        int[] cur = { str[0] };
        int ret;

        if (at(s, cur[0]) != '/') return 1;
        cur[0]++;
        ret = xmlUriParseSegment(uri, s, cur, 0, false);
        if (ret == 0)
        {
            while (at(s, cur[0]) == '/')
            {
                cur[0]++;
                ret = xmlUriParseSegment(uri, s, cur, 0, true);
                if (ret != 0) return ret;
            }
        }
        setPath(uri, s, str[0], cur[0]);
        str[0] = cur[0];
        return 0;
    }

    /**
     * Parse an path without root and fills in the appropriate fields
     * of the uri structure
     *
     * path-rootless = segment-nz *( "/" segment )
     */
    private static int xmlUriParsePathRootless(XmlUri uri, byte[] s, int[] str)
    {
        int[] cur = { str[0] };
        int ret;

        ret = xmlUriParseSegment(uri, s, cur, 0, false);
        if (ret != 0) return ret;
        while (at(s, cur[0]) == '/')
        {
            cur[0]++;
            ret = xmlUriParseSegment(uri, s, cur, 0, true);
            if (ret != 0) return ret;
        }
        setPath(uri, s, str[0], cur[0]);
        str[0] = cur[0];
        return 0;
    }

    /**
     * Parse an path which is not a scheme and fills in the appropriate fields
     * of the uri structure
     *
     * path-noscheme = segment-nz-nc *( "/" segment )
     */
    private static int xmlUriParsePathNoScheme(XmlUri uri, byte[] s, int[] str)
    {
        int[] cur = { str[0] };
        int ret;

        ret = xmlUriParseSegment(uri, s, cur, ':', false);
        if (ret != 0) return ret;
        while (at(s, cur[0]) == '/')
        {
            cur[0]++;
            ret = xmlUriParseSegment(uri, s, cur, 0, true);
            if (ret != 0) return ret;
        }
        setPath(uri, s, str[0], cur[0]);
        str[0] = cur[0];
        return 0;
    }

    /**
     * Parse an hierarchical part and fills in the appropriate fields
     * of the uri structure
     *
     * hier-part     = "//" authority path-abempty
     *                / path-absolute
     *                / path-rootless
     *                / path-empty
     */
    private static int xmlUriParseHierPart(XmlUri uri, byte[] s, int[] str)
    {
        int[] cur = { str[0] };
        int ret;

        if (at(s, cur[0]) == '/' && at(s, cur[0] + 1) == '/')
        {
            cur[0] += 2;
            ret = xmlUriParseAuthority(uri, s, cur);
            if (ret != 0) return ret;
            /*
             * An empty server is marked with a special URI value.
             */
            if (uri.server == null && uri.port == PORT_EMPTY)
                uri.port = PORT_EMPTY_SERVER;
            ret = xmlUriParsePathAbEmpty(uri, s, cur);
            if (ret != 0) return ret;
            str[0] = cur[0];
            return 0;
        }
        else if (at(s, cur[0]) == '/')
        {
            ret = xmlUriParsePathAbsolute(uri, s, cur);
            if (ret != 0) return ret;
        }
        else if (isaPchar(uri, s, cur[0]))
        {
            ret = xmlUriParsePathRootless(uri, s, cur);
            if (ret != 0) return ret;
        }
        else
        {
            /* path-empty is effectively empty */
            if (uri != null) uri.path = null;
        }
        str[0] = cur[0];
        return 0;
    }

    /**
     * Parse an URI string and fills in the appropriate fields
     * of the uri structure
     *
     * relative-ref  = relative-part [ "?" query ] [ "\#" fragment ]
     * relative-part = "//" authority path-abempty
     *               / path-absolute
     *               / path-noscheme
     *               / path-empty
     */
    private static int xmlUriParseRelativeRef(XmlUri uri, byte[] s)
    {
        int[] str = { 0 };
        int ret;

        if (at(s, str[0]) == '/' && at(s, str[0] + 1) == '/')
        {
            str[0] += 2;
            ret = xmlUriParseAuthority(uri, s, str);
            if (ret != 0) return ret;
            ret = xmlUriParsePathAbEmpty(uri, s, str);
            if (ret != 0) return ret;
        }
        else if (at(s, str[0]) == '/')
        {
            ret = xmlUriParsePathAbsolute(uri, s, str);
            if (ret != 0) return ret;
        }
        else if (isaPchar(uri, s, str[0]))
        {
            ret = xmlUriParsePathNoScheme(uri, s, str);
            if (ret != 0) return ret;
        }
        else
        {
            /* path-empty is effectively empty */
            if (uri != null) uri.path = null;
        }

        if (at(s, str[0]) == '?')
        {
            str[0]++;
            ret = xmlUriParseQuery(uri, s, str);
            if (ret != 0) return ret;
        }
        if (at(s, str[0]) == '#')
        {
            str[0]++;
            ret = xmlUriParseFragment(uri, s, str);
            if (ret != 0) return ret;
        }
        if (at(s, str[0]) != 0)
        {
            xmlUriClean(uri);
            return 1;
        }
        return 0;
    }

    /**
     * Parse an URI string and fills in the appropriate fields
     * of the uri structure
     *
     * scheme ":" hier-part [ "?" query ] [ "\#" fragment ]
     */
    private static int xmlUriParseUri(XmlUri uri, byte[] s)
    {
        int[] str = { 0 };
        int ret;

        ret = xmlUriParseScheme(uri, s, str);
        if (ret != 0) return ret;
        if (at(s, str[0]) != ':')
        {
            return 1;
        }
        str[0]++;
        ret = xmlUriParseHierPart(uri, s, str);
        if (ret != 0) return ret;
        if (at(s, str[0]) == '?')
        {
            str[0]++;
            ret = xmlUriParseQuery(uri, s, str);
            if (ret != 0) return ret;
        }
        if (at(s, str[0]) == '#')
        {
            str[0]++;
            ret = xmlUriParseFragment(uri, s, str);
            if (ret != 0) return ret;
        }
        if (at(s, str[0]) != 0)
        {
            xmlUriClean(uri);
            return 1;
        }
        return 0;
    }

    /**
     * Parse an URI reference string and fills in the appropriate fields
     * of the uri structure
     *
     * URI-reference = URI / relative-ref
     */
    private static int xmlUriParseUriReference(XmlUri uri, byte[] s)
    {
        int ret;

        if (s == null) return -1;
        xmlUriClean(uri);

        /*
         * Try first to parse absolute refs, then fallback to relative if
         * it fails.
         */
        ret = xmlUriParseUri(uri, s);
        if (ret < 0) return ret;
        if (ret != 0)
        {
            xmlUriClean(uri);
            ret = xmlUriParseRelativeRef(uri, s);
            if (ret != 0)
            {
                xmlUriClean(uri);
                return ret;
            }
        }
        return 0;
    }

    private static int xmlUriParseSafe(byte[] str, XmlUri[] uriOut)
    {
        if (uriOut == null) return 1;
        uriOut[0] = null;
        if (str == null) return 1;

        XmlUri uri = create();
        int ret = xmlUriParseUriReference(uri, str);
        if (ret != 0) return ret;

        uriOut[0] = uri;
        return 0;
    }

    /**
     * Parse an URI based on RFC 3986.
     *
     * URI-reference = [ absoluteURI | relativeURI ] [ "\#" fragment ]
     *
     * @param str  the URI string to analyze
     * @param uriOut  uriOut[0] is set to the parsed URI or null on error
     * @return 0 on success, an error code (typically 1) if the URI is invalid
     * or -1 if a memory allocation failed.
     */
    public static int parseSafe(String str, XmlUri[] uriOut)
    {
        return xmlUriParseSafe(toUtf8(str), uriOut);
    }

    /**
     * Parse an URI based on RFC 3986.
     *
     * URI-reference = [ absoluteURI | relativeURI ] [ "\#" fragment ]
     *
     * @param str  the URI string to analyze
     * @return a newly built XmlUri or null in case of error
     */
    public static XmlUri parse(String str)
    {
        XmlUri[] uri = { null };
        parseSafe(str, uri);
        return uri[0];
    }

    /**
     * Parse an URI reference string based on RFC 3986 and fills in the
     * appropriate fields of this structure. The fields are cleared first,
     * except for port and cleanup (like in the C version).
     *
     * URI-reference = URI / relative-ref
     *
     * @param str  the string to analyze
     * @return 0 or the error code
     */
    public int parseReference(String str)
    {
        return xmlUriParseUriReference(this, toUtf8(str));
    }

    /**
     * Parse an URI but allows to keep intact the original fragments.
     *
     * URI-reference = URI / relative-ref
     *
     * @param str  the URI string to analyze
     * @param raw  if 1 unescaping of URI pieces are disabled
     * @return a newly built XmlUri or null in case of error
     */
    public static XmlUri parseRaw(String str, int raw)
    {
        if (str == null) return null;
        XmlUri uri = create();
        if (raw != 0)
        {
            uri.cleanup |= XML_URI_NO_UNESCAPE;
        }
        int ret = uri.parseReference(str);
        if (ret != 0) return null;
        return uri;
    }

    /* **********************************************************************
     *                                                                      *
     *                  Generic URI structure functions                     *
     *                                                                      *
     ************************************************************************/

    /**
     * Simply creates an empty XmlUri.
     *
     * @return the new structure
     */
    public static XmlUri create()
    {
        XmlUri ret = new XmlUri();
        ret.port = PORT_EMPTY;
        return ret;
    }

    /**
     * The output buffer of xml_uri_save, with the same growth (and limit
     * on the length of an URI string output) as the C version.
     */
    private static final class SaveBuf
    {
        byte[] ret;
        int max;

        SaveBuf()
        {
            max = 80;
            ret = new byte[max + 1 + 16];
        }

        /** Function to handle properly a reallocation when saving an URI. */
        boolean xmlUriSaveRealloc()
        {
            int newSize = xmlUriGrowCapacity(max, 80, MAX_URI_LENGTH);
            if (newSize < 0) return false;
            ret = Arrays.copyOf(ret, newSize + 1 + 16);
            max = newSize;
            return true;
        }
    }

    private static int hexDigit(int v)
    {
        return v + (v > 9 ? 'A' - 10 : '0');
    }

    /**
     * Save the URI as an escaped string.
     *
     * @return the escaped string, or null if the result would be longer than
     * the maximum URI length (1MiB)
     */
    public String save()
    {
        return fromUtf8(xmlUriSave(this));
    }

    private static byte[] xmlUriSave(XmlUri uri)
    {
        if (uri == null) return null;

        SaveBuf b = new SaveBuf();
        int len = 0;
        byte[] p;
        int pi;

        if (uri.scheme != null)
        {
            p = toUtf8(uri.scheme);
            pi = 0;
            while (at(p, pi) != 0)
            {
                if (len >= b.max)
                {
                    if (!b.xmlUriSaveRealloc()) return null;
                }
                b.ret[len++] = p[pi++];
            }
            if (len >= b.max)
            {
                if (!b.xmlUriSaveRealloc()) return null;
            }
            b.ret[len++] = ':';
        }
        if (uri.opaque != null)
        {
            p = toUtf8(uri.opaque);
            pi = 0;
            while (at(p, pi) != 0)
            {
                if (len + 3 >= b.max)
                {
                    if (!b.xmlUriSaveRealloc()) return null;
                }
                if (isReserved(p[pi]) || isUnreserved(p[pi]))
                    b.ret[len++] = p[pi++];
                else
                {
                    int val = p[pi++] & 0xFF;
                    int hi = val / 0x10, lo = val % 0x10;
                    b.ret[len++] = '%';
                    b.ret[len++] = (byte)hexDigit(hi);
                    b.ret[len++] = (byte)hexDigit(lo);
                }
            }
        }
        else
        {
            if (uri.server != null || uri.port != PORT_EMPTY)
            {
                if (len + 3 >= b.max)
                {
                    if (!b.xmlUriSaveRealloc()) return null;
                }
                b.ret[len++] = '/';
                b.ret[len++] = '/';
                if (uri.user != null)
                {
                    p = toUtf8(uri.user);
                    pi = 0;
                    while (at(p, pi) != 0)
                    {
                        if (len + 3 >= b.max)
                        {
                            if (!b.xmlUriSaveRealloc()) return null;
                        }
                        int c = p[pi];
                        if (isUnreserved(c) || c == ';' || c == ':' || c == '&' || c == '=' ||
                            c == '+' || c == '$' || c == ',')
                            b.ret[len++] = p[pi++];
                        else
                        {
                            int val = p[pi++] & 0xFF;
                            int hi = val / 0x10, lo = val % 0x10;
                            b.ret[len++] = '%';
                            b.ret[len++] = (byte)hexDigit(hi);
                            b.ret[len++] = (byte)hexDigit(lo);
                        }
                    }
                    if (len + 3 >= b.max)
                    {
                        if (!b.xmlUriSaveRealloc()) return null;
                    }
                    b.ret[len++] = '@';
                }
                if (uri.server != null)
                {
                    p = toUtf8(uri.server);
                    pi = 0;
                    while (at(p, pi) != 0)
                    {
                        if (len >= b.max)
                        {
                            if (!b.xmlUriSaveRealloc()) return null;
                        }
                        /* TODO: escaping? */
                        b.ret[len++] = p[pi++];
                    }
                }
                if (uri.port > 0)
                {
                    if (len + 10 >= b.max)
                    {
                        if (!b.xmlUriSaveRealloc()) return null;
                    }
                    // Like snprintf(&ret[len], max - len, ":%d", port): truncated
                    // output (still NUL terminated) but len grows with the full length.
                    byte[] ps = (":" + uri.port).getBytes(StandardCharsets.US_ASCII);
                    int size = b.max - len;
                    int n = Math.min(ps.length, size - 1);
                    System.arraycopy(ps, 0, b.ret, len, n);
                    b.ret[len + n] = 0;
                    len += ps.length;
                }
            }
            else if (uri.authority != null)
            {
                if (len + 3 >= b.max)
                {
                    if (!b.xmlUriSaveRealloc()) return null;
                }
                b.ret[len++] = '/';
                b.ret[len++] = '/';
                p = toUtf8(uri.authority);
                pi = 0;
                while (at(p, pi) != 0)
                {
                    if (len + 3 >= b.max)
                    {
                        if (!b.xmlUriSaveRealloc()) return null;
                    }
                    int c = p[pi];
                    if (isUnreserved(c) || c == '$' || c == ',' || c == ';' || c == ':' || c == '@' ||
                        c == '&' || c == '=' || c == '+')
                        b.ret[len++] = p[pi++];
                    else
                    {
                        int val = p[pi++] & 0xFF;
                        int hi = val / 0x10, lo = val % 0x10;
                        b.ret[len++] = '%';
                        b.ret[len++] = (byte)hexDigit(hi);
                        b.ret[len++] = (byte)hexDigit(lo);
                    }
                }
            }
            else if (uri.scheme != null)
            {
                if (len + 3 >= b.max)
                {
                    if (!b.xmlUriSaveRealloc()) return null;
                }
            }
            if (uri.path != null)
            {
                p = toUtf8(uri.path);
                pi = 0;
                /*
                 * the colon in file:///d: should not be escaped or
                 * Windows accesses fail later.
                 */
                if (uri.scheme != null &&
                    at(p, 0) == '/' &&
                    ((at(p, 1) >= 'a' && at(p, 1) <= 'z') ||
                     (at(p, 1) >= 'A' && at(p, 1) <= 'Z')) &&
                    at(p, 2) == ':' &&
                    uri.scheme.equals("file"))
                {
                    if (len + 3 >= b.max)
                    {
                        if (!b.xmlUriSaveRealloc()) return null;
                    }
                    b.ret[len++] = p[pi++];
                    b.ret[len++] = p[pi++];
                    b.ret[len++] = p[pi++];
                }
                while (at(p, pi) != 0)
                {
                    if (len + 3 >= b.max)
                    {
                        if (!b.xmlUriSaveRealloc()) return null;
                    }
                    int c = p[pi];
                    if (isUnreserved(c) || c == '/' || c == ';' || c == '@' || c == '&' ||
                        c == '=' || c == '+' || c == '$' || c == ',')
                        b.ret[len++] = p[pi++];
                    else
                    {
                        int val = p[pi++] & 0xFF;
                        int hi = val / 0x10, lo = val % 0x10;
                        b.ret[len++] = '%';
                        b.ret[len++] = (byte)hexDigit(hi);
                        b.ret[len++] = (byte)hexDigit(lo);
                    }
                }
            }
            if (uri.queryRaw != null)
            {
                if (len + 1 >= b.max)
                {
                    if (!b.xmlUriSaveRealloc()) return null;
                }
                b.ret[len++] = '?';
                p = toUtf8(uri.queryRaw);
                pi = 0;
                while (at(p, pi) != 0)
                {
                    if (len + 1 >= b.max)
                    {
                        if (!b.xmlUriSaveRealloc()) return null;
                    }
                    b.ret[len++] = p[pi++];
                }
            }
            else if (uri.query != null)
            {
                if (len + 3 >= b.max)
                {
                    if (!b.xmlUriSaveRealloc()) return null;
                }
                b.ret[len++] = '?';
                p = toUtf8(uri.query);
                pi = 0;
                while (at(p, pi) != 0)
                {
                    if (len + 3 >= b.max)
                    {
                        if (!b.xmlUriSaveRealloc()) return null;
                    }
                    if (isUnreserved(p[pi]) || isReserved(p[pi]))
                        b.ret[len++] = p[pi++];
                    else
                    {
                        int val = p[pi++] & 0xFF;
                        int hi = val / 0x10, lo = val % 0x10;
                        b.ret[len++] = '%';
                        b.ret[len++] = (byte)hexDigit(hi);
                        b.ret[len++] = (byte)hexDigit(lo);
                    }
                }
            }
        }
        if (uri.fragment != null)
        {
            if (len + 3 >= b.max)
            {
                if (!b.xmlUriSaveRealloc()) return null;
            }
            b.ret[len++] = '#';
            p = toUtf8(uri.fragment);
            pi = 0;
            while (at(p, pi) != 0)
            {
                if (len + 3 >= b.max)
                {
                    if (!b.xmlUriSaveRealloc()) return null;
                }
                if (isUnreserved(p[pi]) || isReserved(p[pi]))
                    b.ret[len++] = p[pi++];
                else
                {
                    int val = p[pi++] & 0xFF;
                    int hi = val / 0x10, lo = val % 0x10;
                    b.ret[len++] = '%';
                    b.ret[len++] = (byte)hexDigit(hi);
                    b.ret[len++] = (byte)hexDigit(lo);
                }
            }
        }
        if (len >= b.max)
        {
            if (!b.xmlUriSaveRealloc()) return null;
        }
        b.ret[len] = 0;
        return cstr(b.ret, len);
    }

    /**
     * Make sure the XmlUri struct is free of content (port and cleanup are kept).
     */
    private static void xmlUriClean(XmlUri uri)
    {
        if (uri == null) return;

        uri.scheme = null;
        uri.server = null;
        uri.user = null;
        uri.path = null;
        uri.fragment = null;
        uri.opaque = null;
        uri.authority = null;
        uri.query = null;
        uri.queryRaw = null;
    }

    /* **********************************************************************
     *                                                                      *
     *                      Helper functions                                *
     *                                                                      *
     ************************************************************************/

    private static boolean xmlUriIsPathSeparator(int c, boolean isFile)
    {
        if (c == '/') return true;

        // A backslash is a separator for file paths only on _WIN32 in the C version.

        return false;
    }

    /**
     * Normalize a filesystem path or URI in place. The path is a NUL
     * terminated buffer, the result is NUL terminated in the same buffer.
     *
     * @return 0 or an error code
     */
    private static int xmlUriNormalizePath(byte[] path, boolean isFile)
    {
        int cur, out;
        int numSeg = 0;

        if (path == null) return -1;

        cur = 0;
        out = 0;

        if (path[cur] == 0) return 0;

        if (xmlUriIsPathSeparator(path[cur], isFile))
        {
            cur++;
            path[out++] = '/';
        }

        while (path[cur] != 0)
        {
            /*
             * At this point, out is either empty or ends with a separator.
             * Collapse multiple separators first.
             */
            while (xmlUriIsPathSeparator(path[cur], isFile))
            {
                cur++;
            }

            if (path[cur] == '.')
            {
                if (path[cur + 1] == 0)
                {
                    /* Ignore "." at end of path */
                    break;
                }
                else if (xmlUriIsPathSeparator(path[cur + 1], isFile))
                {
                    /* Skip "./" */
                    cur += 2;
                    continue;
                }
                else if (path[cur + 1] == '.' &&
                         (path[cur + 2] == 0 || xmlUriIsPathSeparator(path[cur + 2], isFile)))
                {
                    if (numSeg > 0)
                    {
                        /* Handle ".." by removing last segment */
                        do
                        {
                            out--;
                        } while (out > 0 && !xmlUriIsPathSeparator(path[out - 1], isFile));
                        numSeg--;

                        if (path[cur + 2] == 0) break;
                        cur += 3;
                        continue;
                    }
                    else if (path[out] == '/')
                    {
                        /* Ignore extraneous ".." in absolute paths */
                        if (path[cur + 2] == 0) break;
                        cur += 3;
                        continue;
                    }
                    else
                    {
                        /* Keep "../" at start of relative path */
                        numSeg--;
                    }
                }
            }

            /* Copy segment */
            while (path[cur] != 0 && !xmlUriIsPathSeparator(path[cur], isFile))
            {
                path[out++] = path[cur++];
            }

            /* Copy separator */
            if (path[cur] != 0)
            {
                cur++;
                path[out++] = '/';
            }

            numSeg++;
        }

        /* Keep "." if output is empty and it's a file */
        if (isFile && out <= 0) path[out++] = '.';
        path[out] = 0;

        return 0;
    }

    /** Copy s into a NUL terminated buffer (one extra byte) for in place normalization. */
    private static byte[] terminated(byte[] s)
    {
        return Arrays.copyOf(s, s.length + 1);
    }

    /**
     * Applies the 5 normalization steps to a path string--that is, RFC 2396
     * Section 5.2, steps 6.c through 6.g. The C version normalizes the
     * string in place, here the normalized path is returned.
     *
     * @param path  the path string
     * @return the normalized path, or null if path is null
     */
    public static String normalizeUriPath(String path)
    {
        if (path == null) return null;
        byte[] buf = terminated(toUtf8(path));
        xmlUriNormalizePath(buf, false);
        return fromUtf8(cstr(buf, buf.length));
    }

    private static boolean xmlUriIsHex(int c)
    {
        return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
    }

    private static int hexValue(int c)
    {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return 0;
    }

    /**
     * Unescaping routine, but does not check that the string is an URI. The
     * output is a direct byte translation of %XX values (no encoding).
     * Unescapes len bytes from start, or the rest of the string if len is
     * &lt;= 0. The result ends at the first NUL (like the C string would).
     */
    private static byte[] xmlUriUnescapeString(byte[] str, int start, int len)
    {
        if (str == null) return null;
        int avail = str.length - start;
        if (len <= 0 || len > avail) len = avail;

        byte[] ret = new byte[len];
        int in = start;
        int out = 0;
        while (len > 0)
        {
            if (len > 2 && str[in] == '%' && xmlUriIsHex(str[in + 1]) && xmlUriIsHex(str[in + 2]))
            {
                int c = hexValue(str[in + 1]) * 16 + hexValue(str[in + 2]);
                in += 3;
                len -= 3;
                /* Explicit sign change */
                ret[out++] = (byte)c;
            }
            else
            {
                ret[out++] = str[in++];
                len--;
            }
        }
        return cstr(ret, out);
    }

    /**
     * Unescaping routine, but does not check that the string is an URI. The
     * output is a direct byte translation of %XX values (no encoding), see
     * the class documentation for bytes that are not valid UTF-8. The result
     * ends at the first NUL (eg from "%00").
     *
     * @param str  the string to unescape
     * @param len  the length in UTF-8 bytes to unescape (or &lt;= 0 to indicate full string)
     * @return a copy of the string, but unescaped, null if str is null
     */
    public static String unescapeString(String str, int len)
    {
        byte[] s = toUtf8(str);
        if (s == null) return null;
        return fromUtf8(xmlUriUnescapeString(s, 0, len));
    }

    /* **********************************************************************
     *                                                                      *
     *                      Public functions                                *
     *                                                                      *
     ************************************************************************/

    private static boolean xmlUriIsAbsolutePath(byte[] path)
    {
        int c = at(path, 0);

        if (xmlUriIsPathSeparator(c, true)) return true;

        // Windows drive letters are absolute only on _WIN32 in the C version.

        return false;
    }

    private static int indexOf(byte[] s, byte[] what)
    {
        outer:
        for (int i = 0; i + what.length <= s.length; i++)
        {
            for (int k = 0; k < what.length; k++)
            {
                if (s[i + k] != what[k]) continue outer;
            }
            return i;
        }
        return -1;
    }

    private static final byte[] COLON_SLASH_SLASH = { ':', '/', '/' };
    private static final byte[] HASH = { '#' };

    /**
     * Resolves a filesystem path from a base path.
     *
     * @return 0 on success, -1 if a memory allocation failed or an error
     * code if URI or base are invalid.
     */
    private static int xmlUriResolvePath(byte[] escRef, byte[] base, byte[][] out)
    {
        int fragment;
        byte[] ref;
        byte[] result = null;
        int i;

        if (out == null) return 1;
        out[0] = null;

        if (escRef == null || escRef.length == 0)
        {
            if (base == null || base.length == 0) return 1;
            out[0] = base;
            return 0;
        }

        /*
         * If a URI is resolved, we can assume it is a valid URI and not
         * a filesystem path. This means we have to unescape the part
         * before the fragment.
         */
        fragment = indexOf(escRef, HASH);
        byte[] esc = escRef;
        if (fragment >= 0)
        {
            esc = Arrays.copyOf(escRef, fragment);
        }

        ref = xmlUriUnescapeString(esc, 0, -1);

        done:
        {
            if (base == null || base.length == 0) break done;

            if (xmlUriIsAbsolutePath(ref)) break done;

            /*
             * Remove last segment from base
             */
            i = base.length;
            while (i > 0 && !xmlUriIsPathSeparator(base[i - 1], true)) i--;

            /*
             * Concatenate base and ref
             */
            if (i > 0)
            {
                result = new byte[i + ref.length + 1];
                System.arraycopy(base, 0, result, 0, i);
                System.arraycopy(ref, 0, result, i, ref.length);
                result[i + ref.length] = 0;

                /*
                 * Normalize
                 */
                xmlUriNormalizePath(result, true);
                result = cstr(result, result.length);
            }
        }

        if (result == null) result = ref;

        if (fragment >= 0)
        {
            result = xmlUriStrcat(result, escRef, fragment);
        }

        out[0] = result;
        return 0;
    }

    private static int xmlUriBuildSafe(byte[] URI, byte[] base, byte[][] valPtr)
    {
        byte[] val = null;
        int ret, len, indx, cur, out;
        XmlUri ref = null;
        XmlUri bas = null;
        XmlUri res = null;
        XmlUri[] tmp = { null };

        if (valPtr == null) return 1;
        valPtr[0] = null;

        if (URI == null) return 1;

        if (base == null)
        {
            valPtr[0] = URI;
            return 0;
        }

        done:
        {
            /*
             * 1) The URI reference is parsed into the potential four components and
             *    fragment identifier, as described in Section 4.3.
             *
             *    NOTE that a completely empty URI is treated by modern browsers
             *    as a reference to "." rather than as a synonym for the current
             *    URI.  Should we do that here?
             */
            if (URI.length != 0)
            {
                ret = xmlUriParseSafe(URI, tmp);
                ref = tmp[0];
            }
            else
                ret = 0;
            if (ret != 0) break done;
            if (ref != null && ref.scheme != null)
            {
                /*
                 * The URI is absolute don't modify.
                 */
                val = URI;
                break done;
            }

            /*
             * If base has no scheme or authority, it is assumed to be a
             * filesystem path.
             */
            if (indexOf(base, COLON_SLASH_SLASH) < 0)
            {
                return xmlUriResolvePath(URI, base, valPtr);
            }

            // Paths with a Windows drive letter are resolved as filesystem paths only on _WIN32.

            ret = xmlUriParseSafe(base, tmp);
            bas = tmp[0];
            if (ret < 0) break done;
            if (ret != 0)
            {
                if (ref != null)
                {
                    ret = 0;
                    val = xmlUriSave(ref);
                    if (val == null) ret = -1;
                }
                break done;
            }
            if (ref == null)
            {
                /*
                 * the base fragment must be ignored
                 */
                bas.fragment = null;
                val = xmlUriSave(bas);
                if (val == null) ret = -1;
                break done;
            }

            /*
             * 2) If the path component is empty and the scheme, authority, and
             *    query components are undefined, then it is a reference to the
             *    current document and we are done.  Otherwise, the reference URI's
             *    query and fragment components are defined as found (or not found)
             *    within the URI reference and not inherited from the base URI.
             *
             *    NOTE that in modern browsers, the parsing differs from the above
             *    in the following aspect:  the query component is allowed to be
             *    defined while still treating this as a reference to the current
             *    document.
             */
            ret = -1;
            res = create();
            step7:
            {
                if (ref.scheme == null && ref.path == null &&
                    (ref.authority == null && ref.server == null && ref.port == PORT_EMPTY))
                {
                    if (bas.scheme != null) res.scheme = bas.scheme;
                    if (bas.authority != null)
                    {
                        res.authority = bas.authority;
                    }
                    else
                    {
                        if (bas.server != null) res.server = bas.server;
                        if (bas.user != null) res.user = bas.user;
                        res.port = bas.port;
                    }
                    if (bas.path != null) res.path = bas.path;
                    if (ref.queryRaw != null) res.queryRaw = ref.queryRaw;
                    else if (ref.query != null) res.query = ref.query;
                    else if (bas.queryRaw != null) res.queryRaw = bas.queryRaw;
                    else if (bas.query != null) res.query = bas.query;
                    if (ref.fragment != null) res.fragment = ref.fragment;
                    break step7;
                }

                /*
                 * 3) If the scheme component is defined, indicating that the reference
                 *    starts with a scheme name, then the reference is interpreted as an
                 *    absolute URI and we are done.  Otherwise, the reference URI's
                 *    scheme is inherited from the base URI's scheme component.
                 */
                if (ref.scheme != null)
                {
                    val = xmlUriSave(ref);
                    if (val != null) ret = 0;
                    break done;
                }
                if (bas.scheme != null) res.scheme = bas.scheme;

                if (ref.queryRaw != null) res.queryRaw = ref.queryRaw;
                else if (ref.query != null) res.query = ref.query;
                if (ref.fragment != null) res.fragment = ref.fragment;

                /*
                 * 4) If the authority component is defined, then the reference is a
                 *    network-path and we skip to step 7.  Otherwise, the reference
                 *    URI's authority is inherited from the base URI's authority
                 *    component, which will also be undefined if the URI scheme does not
                 *    use an authority component.
                 */
                if (ref.authority != null || ref.server != null || ref.port != PORT_EMPTY)
                {
                    if (ref.authority != null)
                    {
                        res.authority = ref.authority;
                    }
                    else
                    {
                        if (ref.server != null) res.server = ref.server;
                        if (ref.user != null) res.user = ref.user;
                        res.port = ref.port;
                    }
                    if (ref.path != null) res.path = ref.path;
                    break step7;
                }
                if (bas.authority != null)
                {
                    res.authority = bas.authority;
                }
                else if (bas.server != null || bas.port != PORT_EMPTY)
                {
                    if (bas.server != null) res.server = bas.server;
                    if (bas.user != null) res.user = bas.user;
                    res.port = bas.port;
                }

                /*
                 * 5) If the path component begins with a slash character ("/"), then
                 *    the reference is an absolute-path and we skip to step 7.
                 */
                if (ref.path != null && ref.path.startsWith("/"))
                {
                    res.path = ref.path;
                    break step7;
                }

                /*
                 * 6) If this step is reached, then we are resolving a relative-path
                 *    reference.  The relative path needs to be merged with the base
                 *    URI's path.  Although there are many ways to do this, we will
                 *    describe a simple method using a separate string buffer.
                 *
                 * Allocate a buffer large enough for the result string.
                 */
                byte[] refPath = toUtf8(ref.path);
                byte[] basPath = toUtf8(bas.path);
                len = 2; /* extra / and 0 */
                if (refPath != null) len += refPath.length;
                if (basPath != null) len += basPath.length;
                byte[] p = new byte[len];
                p[0] = 0;

                /*
                 * a) All but the last segment of the base URI's path component is
                 *    copied to the buffer.  In other words, any characters after the
                 *    last (right-most) slash character, if any, are excluded.
                 */
                cur = 0;
                out = 0;
                if (basPath != null)
                {
                    while (at(basPath, cur) != 0)
                    {
                        while (at(basPath, cur) != 0 && at(basPath, cur) != '/') cur++;
                        if (at(basPath, cur) == 0) break;

                        cur++;
                        while (out < cur)
                        {
                            p[out] = basPath[out];
                            out++;
                        }
                    }
                }
                p[out] = 0;

                /*
                 * b) The reference's path component is appended to the buffer
                 *    string.
                 */
                if (refPath != null && refPath.length != 0)
                {
                    indx = 0;
                    /*
                     * Ensure the path includes a '/'
                     */
                    if (out == 0 && (bas.server != null || bas.port != PORT_EMPTY))
                        p[out++] = '/';
                    while (indx < refPath.length)
                    {
                        p[out++] = refPath[indx++];
                    }
                }
                p[out] = 0;

                /*
                 * Steps c) to h) are really path normalization steps
                 */
                xmlUriNormalizePath(p, false);
                res.path = fromUtf8(cstr(p, p.length));
            }
            // step_7:

            /*
             * 7) The resulting URI components, including any inherited from the
             *    base URI, are recombined to give the absolute form of the URI
             *    reference.
             */
            val = xmlUriSave(res);
            if (val != null) ret = 0;
        }
        valPtr[0] = val;
        return ret;
    }

    /**
     * Computes the final URI of the reference done by checking that
     * the given URI is valid, and building the final URI using the
     * base URI. This is processed according to section 5.2 of the
     * RFC 2396 (libxml2 xmlBuildURISafe).
     *
     * @param uri  the URI instance found in the document
     * @param base  the base value, may be null
     * @param errorOut  if not null, errorOut[0] is set to 0 on success, -1 if
     * the result is too long or an error code if URI or base are invalid
     * @return the resulting URI, or null on error
     */
    public static String buildSafe(String uri, String base, int[] errorOut)
    {
        byte[][] val = { null };
        int ret = xmlUriBuildSafe(toUtf8(uri), toUtf8(base), val);
        if (errorOut != null) errorOut[0] = ret;
        return fromUtf8(val[0]);
    }

    /**
     * Computes the final URI of the reference by resolving it against the
     * base, like {@link #buildSafe(String, String, int[])} (libxml2 xmlBuildURI).
     *
     * @param uri  the URI instance found in the document
     * @param base  the base value, may be null
     * @return a new URI string or null in case of error
     */
    public static String build(String uri, String base)
    {
        return buildSafe(uri, base, null);
    }

    /**
     * Port of xmlURIEscapeStr: escape a string to hex, except unreserved
     * characters, "@" and the characters in list. Every byte of the UTF-8
     * encoding of a non-ASCII character is escaped.
     *
     * @param str  the string to escape
     * @param list  extra characters that are not escaped, may be null
     * @return the escaped string or null if str is null
     */
    public static String escapeStr(String str, String list)
    {
        byte[] s = toUtf8(str);
        if (s == null) return null;
        if (s.length == 0) return "";
        byte[] l = toUtf8(list);
        byte[] ret = new byte[s.length * 3];
        int out = 0;
        for (int in = 0; in < s.length; in++)
        {
            int ch = s[in] & 0xFF;
            if (ch != '@' && !isUnreserved(ch) && !(l != null && contains(l, ch)))
            {
                int val;
                ret[out++] = '%';
                val = ch >> 4;
                ret[out++] = (byte)(val <= 9 ? '0' + val : 'A' + val - 0xA);
                val = ch & 0xF;
                ret[out++] = (byte)(val <= 9 ? '0' + val : 'A' + val - 0xA);
            }
            else
            {
                ret[out++] = (byte)ch;
            }
        }
        return fromUtf8(Arrays.copyOf(ret, out));
    }

    /** Like strchr(list, ch) != NULL for a non NUL ch. */
    private static boolean contains(byte[] list, int ch)
    {
        for (byte b : list)
        {
            if ((b & 0xFF) == ch) return true;
        }
        return false;
    }
}
