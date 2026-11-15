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

/* Ported from the C version (parts/xml_parser.c), which follows libxml2
   (parserInternals.c, encoding.c and error.c). libxml2 is covered by the
   following notice (from libxml2/Copyright):

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
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

package org.libxmq.imp.xml;

import java.io.ByteArrayOutputStream;
import java.nio.charset.StandardCharsets;

/**
 * The byte level part of the xml parser, like xml_parser_prepare_input and
 * xml_parser_format in the C version: conversion of the input to UTF-8
 * (UTF-16 detected by a bom or by the "&lt;?" pattern, ISO-8859-1 by
 * declaration), end of line normalization, validation of UTF-8 and of the
 * xml characters, and error messages with source context. Positions and
 * columns are in bytes and the context line is at most 80 bytes, exactly
 * like the C version (and libxml2), so the messages are bytes too.
 */
final class XmlInput
{
    private XmlInput()
    {
    }

    /** Like vsnprintf into the 512 byte message buffer of the C version. */
    static final int MAX_MSG = 511;

    /** Format a message with source context like libxml2 xmlParserPrintFileContextInternal. */
    static byte[] format(String source, String kind, String msg, byte[] s, int len, int pos, int[] out_line)
    {
        byte[] m = msg.getBytes(StandardCharsets.UTF_8);
        if (m.length > MAX_MSG)
        {
            byte[] t = new byte[MAX_MSG];
            System.arraycopy(m, 0, t, 0, MAX_MSG);
            m = t;
        }
        if (pos > len) pos = len;
        int line = 1;
        for (int k = 0; k < pos; k++) if (s[k] == '\n') line++;
        if (out_line != null) out_line[0] = line;

        ByteArrayOutputStream out = new ByteArrayOutputStream();
        String src = source == null ? "noname.xml" : source;
        out.writeBytes((src + ":" + line + ": " + kind + " : ").getBytes(StandardCharsets.UTF_8));
        out.writeBytes(m);
        if (m.length == 0 || m[m.length - 1] != '\n') out.write('\n');

        // Port of libxml2 2.16 xmlParserInputGetWindow and xmlParserPrintFileContextInternal:
        // at most 80 bytes of the line around pos, never splitting a UTF-8 character.
        final int size = 80;
        int cur = pos;
        // Skip backwards over any end of lines.
        while (cur > 0 && (at(s, len, cur) == '\n' || at(s, len, cur) == '\r')) cur--;
        int n = 0;
        // Search backwards for the beginning of the line (to max size).
        while (n < size && cur > 0 && at(s, len, cur) != '\n' && at(s, len, cur) != '\r')
        {
            cur--;
            n++;
        }
        if (n > 0 && (at(s, len, cur) == '\n' || at(s, len, cur) == '\r'))
        {
            cur++;
        }
        else
        {
            // Skip over continuation bytes.
            while (cur < pos && (at(s, len, cur) & 0xC0) == 0x80) cur++;
        }
        int col = pos - cur;
        // Search forward for the end of the line (to max size), whole characters only.
        int start = cur;
        n = 0;
        while (at(s, len, cur) != 0 && at(s, len, cur) != '\n' && at(s, len, cur) != '\r')
        {
            int[] d = utf8_decode(s, cur, len - cur);
            if (d == null || n + d[1] > size) break;
            cur += d[1];
            n += d[1];
        }
        // The column can only point to the end of the buffer if there is space for the marker.
        if (col >= n) col = n < size ? n : size - 1;
        out.write(s, start, n);
        out.write('\n');
        for (int k = 0; k < col; k++) out.write(s[start + k] == '\t' ? '\t' : ' ');
        out.write('^');
        out.write('\n');
        return out.toByteArray();
    }

    static XmlParseException fatal(String source, String msg, byte[] s, int len, int pos)
    {
        return fatal(source, "parser error", msg, s, len, pos);
    }

    static XmlParseException fatal(String source, String kind, String msg, byte[] s, int len, int pos)
    {
        int[] line = new int[1];
        byte[] full = format(source, kind, msg, s, len, pos, line);
        return new XmlParseException(full, msg, line[0]);
    }

    /** The byte at k, 0 at and after the end, like the nul terminated buffers of libxml2. */
    static int at(byte[] s, int len, int k)
    {
        return k < len ? s[k] & 0xFF : 0;
    }

    static boolean is_blank(byte c)
    {
        return c == 0x20 || c == 0x09 || c == 0x0A || c == 0x0D;
    }

    /** Find encoding="..." in an xml declaration at the start, returns {start, end} or null. */
    static int[] declared_encoding(byte[] s, int off, int len)
    {
        if (len < 6 || !starts_with(s, off, "<?xml") || !is_blank(s[off + 5])) return null;
        int end = len < 1024 ? len : 1024;
        for (int k = 5; k + 1 < end; ++k)
        {
            if (s[off + k] == '?' && s[off + k + 1] == '>') return null;
            if (k + 8 < end && starts_with(s, off + k, "encoding"))
            {
                int j = k + 8;
                while (j < end && is_blank(s[off + j])) j++;
                if (j >= end || s[off + j] != '=') return null;
                j++;
                while (j < end && is_blank(s[off + j])) j++;
                if (j >= end || (s[off + j] != '"' && s[off + j] != '\'')) return null;
                byte q = s[off + j++];
                int start = j;
                while (j < end && s[off + j] != q) j++;
                if (j >= end) return null;
                return new int[] { start, j };
            }
        }
        return null;
    }

    static boolean starts_with(byte[] s, int off, String t)
    {
        if (off + t.length() > s.length) return false;
        for (int k = 0; k < t.length(); k++) if (s[off + k] != t.charAt(k)) return false;
        return true;
    }

    static boolean ascii_ieq(String a, String b)
    {
        return a.length() == b.length() && a.equalsIgnoreCase(b) && a.chars().allMatch(c -> c < 0x80);
    }

    static void add_cp(ByteArrayOutputStream b, int cp)
    {
        if (cp < 0x80) b.write(cp);
        else if (cp < 0x800) { b.write(0xC0 | (cp >> 6)); b.write(0x80 | (cp & 0x3F)); }
        else if (cp < 0x10000) { b.write(0xE0 | (cp >> 12)); b.write(0x80 | ((cp >> 6) & 0x3F)); b.write(0x80 | (cp & 0x3F)); }
        else { b.write(0xF0 | (cp >> 18)); b.write(0x80 | ((cp >> 12) & 0x3F)); b.write(0x80 | ((cp >> 6) & 0x3F)); b.write(0x80 | (cp & 0x3F)); }
    }

    static byte[] convert_utf16(String source, byte[] raw, int off, int len, boolean big_endian) throws XmlParseException
    {
        if (len % 2 != 0) throw fatal(source, "Invalid bytes in character encoding", raw, raw.length, 0);
        ByteArrayOutputStream out = new ByteArrayOutputStream();
        for (int k = 0; k + 1 < len; k += 2)
        {
            int a = raw[off + k] & 0xFF, b = raw[off + k + 1] & 0xFF;
            int u = big_endian ? (a << 8 | b) : (b << 8 | a);
            if (u >= 0xD800 && u < 0xDC00)
            {
                if (k + 3 >= len) throw fatal(source, "Input is not proper UTF-16, indicate encoding !", raw, raw.length, 0);
                int c = raw[off + k + 2] & 0xFF, d = raw[off + k + 3] & 0xFF;
                int l = big_endian ? (c << 8 | d) : (d << 8 | c);
                if (l < 0xDC00 || l >= 0xE000) throw fatal(source, "Input is not proper UTF-16, indicate encoding !", raw, raw.length, 0);
                u = 0x10000 + ((u - 0xD800) << 10) + (l - 0xDC00);
                k += 2;
            }
            else if (u >= 0xDC00 && u < 0xE000)
            {
                throw fatal(source, "Input is not proper UTF-16, indicate encoding !", raw, raw.length, 0);
            }
            add_cp(out, u);
        }
        return out.toByteArray();
    }

    /** Decode one UTF-8 character like xml_utf8_decode, returns {cp, len} or null if invalid. */
    static int[] utf8_decode(byte[] s, int k, int avail)
    {
        int c = s[k] & 0xFF;
        if (c < 0x80) return new int[] { c, 1 };
        if (c < 0xC2) return null;
        if (c < 0xE0)
        {
            if (avail < 2 || (s[k + 1] & 0xC0) != 0x80) return null;
            return new int[] { ((c & 0x1F) << 6) | (s[k + 1] & 0x3F), 2 };
        }
        if (c < 0xF0)
        {
            if (avail < 3 || (s[k + 1] & 0xC0) != 0x80 || (s[k + 2] & 0xC0) != 0x80) return null;
            int v = ((c & 0x0F) << 12) | ((s[k + 1] & 0x3F) << 6) | (s[k + 2] & 0x3F);
            if (v < 0x800 || (v >= 0xD800 && v < 0xE000)) return null;
            return new int[] { v, 3 };
        }
        if (c < 0xF5)
        {
            if (avail < 4 || (s[k + 1] & 0xC0) != 0x80 || (s[k + 2] & 0xC0) != 0x80 || (s[k + 3] & 0xC0) != 0x80) return null;
            int v = ((c & 0x07) << 18) | ((s[k + 1] & 0x3F) << 12) | ((s[k + 2] & 0x3F) << 6) | (s[k + 3] & 0x3F);
            if (v < 0x10000 || v > 0x10FFFF) return null;
            return new int[] { v, 4 };
        }
        return null;
    }

    /**
     * Convert the input to UTF-8, skip a UTF-8 bom, normalize \r\n and \r to \n, then
     * check that the result is valid UTF-8 with only allowed xml characters.
     * @return The prepared UTF-8 bytes.
     */
    static byte[] prepare(String source, byte[] raw) throws XmlParseException
    {
        int len = raw.length;
        byte[] buf;
        if (len >= 2 && (((raw[0] & 0xFF) == 0xFE && (raw[1] & 0xFF) == 0xFF) || ((raw[0] & 0xFF) == 0xFF && (raw[1] & 0xFF) == 0xFE)))
        {
            buf = convert_utf16(source, raw, 2, len - 2, (raw[0] & 0xFF) == 0xFE);
        }
        else if (len >= 4 && raw[0] == 0 && raw[1] == '<' && raw[2] == 0 && raw[3] == '?')
        {
            buf = convert_utf16(source, raw, 0, len, true);
        }
        else if (len >= 4 && raw[0] == '<' && raw[1] == 0 && raw[2] == '?' && raw[3] == 0)
        {
            buf = convert_utf16(source, raw, 0, len, false);
        }
        else
        {
            int off = 0;
            if (len >= 3 && (raw[0] & 0xFF) == 0xEF && (raw[1] & 0xFF) == 0xBB && (raw[2] & 0xFF) == 0xBF) off = 3;
            int n = len - off;
            buf = new byte[n];
            System.arraycopy(raw, off, buf, 0, n);
            int[] enc = declared_encoding(buf, 0, n);
            if (enc != null)
            {
                String e = new String(buf, enc[0], enc[1] - enc[0], StandardCharsets.ISO_8859_1);
                if (ascii_ieq(e, "ISO-8859-1") || ascii_ieq(e, "latin1") || ascii_ieq(e, "ISO_8859-1")
                    || ascii_ieq(e, "ISO-LATIN-1") || ascii_ieq(e, "L1"))
                {
                    ByteArrayOutputStream out = new ByteArrayOutputStream();
                    for (int k = 0; k < n; k++) add_cp(out, buf[k] & 0xFF);
                    buf = out.toByteArray();
                }
                else if (!(ascii_ieq(e, "UTF-8") || ascii_ieq(e, "UTF8") || ascii_ieq(e, "US-ASCII")
                           || ascii_ieq(e, "ASCII") || ascii_ieq(e, "UTF-16") || ascii_ieq(e, "UTF16")))
                {
                    throw fatal(source, "Unsupported encoding: " + e, buf, n, enc[0]);
                }
            }
        }

        // End of line handling: \r\n and \r become \n.
        boolean has_cr = false;
        for (byte b : buf) if (b == '\r') { has_cr = true; break; }
        if (has_cr)
        {
            ByteArrayOutputStream out = new ByteArrayOutputStream(buf.length);
            for (int k = 0; k < buf.length; k++)
            {
                if (buf[k] == '\r')
                {
                    out.write('\n');
                    if (k + 1 < buf.length && buf[k + 1] == '\n') k++;
                }
                else
                {
                    out.write(buf[k]);
                }
            }
            buf = out.toByteArray();
        }

        // Validate utf8 and xml chars.
        for (int k = 0; k < buf.length; )
        {
            int b = buf[k] & 0xFF;
            if (b >= 0x20 && b < 0x80) { k++; continue; }
            int[] d = utf8_decode(buf, k, buf.length - k);
            if (d == null)
            {
                // Like libxml2 2.16, the decoded input (and so the context line) ends at the bad bytes.
                int need = b >= 0xF0 ? 4 : b >= 0xE0 ? 3 : 2;
                boolean incomplete = b >= 0xC2 && b < 0xF5 && buf.length - k < need;
                for (int j = k + 1; incomplete && j < buf.length; j++) if ((buf[j] & 0xC0) != 0x80) incomplete = false;
                if (incomplete)
                {
                    throw fatal(source, String.format("Incomplete UTF-8 sequence starting with %02X", b), buf, k, k);
                }
                throw fatal(source, "I/O error", String.format("Invalid bytes in character encoding\nBytes: 0x%02X 0x%02X 0x%02X 0x%02X",
                                                  b, k + 1 < buf.length ? buf[k + 1] & 0xFF : 0,
                                                  k + 2 < buf.length ? buf[k + 2] & 0xFF : 0,
                                                  k + 3 < buf.length ? buf[k + 3] & 0xFF : 0),
                            buf, k, k);
            }
            if (!XmlParser.is_xml_char(d[0]))
            {
                throw fatal(source, "Char 0x" + Integer.toHexString(d[0]).toUpperCase(java.util.Locale.ROOT) + " out of allowed range",
                            buf, buf.length, k);
            }
            k += d[1];
        }
        return buf;
    }
}
