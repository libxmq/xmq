/* libxmq - Copyright (C) 2026 Fredrik Öhrström (spdx: MIT) */
package org.libxmq.imp.ixml.earley;

/**
 * Decode UTF-8 directly from a byte buffer, one code point at a time. The ixml input
 * is never turned into a String (two bytes per char) or an int[] (four bytes per code
 * point), the parser reads the code points from the bytes.
 *
 * Invalid UTF-8 is rejected like find_invalid_utf8 in the C version: a stray continuation
 * byte, a truncated sequence, an overlong encoding, a surrogate or above U+10FFFF.
 */
public final class Utf8
{
    private Utf8() { }

    /** Decode the code point at buf[i].
     * @param buf the bytes
     * @param i the index of the first byte of the code point, below stop
     * @param stop the end of the input, exclusive
     * @return the code point in the low 21 bits and the number of bytes (1-4) above them,
     *         see codepoint() and length(), or -1 if the bytes at i are not valid UTF-8 */
    public static int decode(byte[] buf, int i, int stop)
    {
        int c = buf[i] & 0xff;
        if (c < 0x80) return c | (1 << 21);
        int need, v, min;
        if (c >= 0xc2 && c < 0xe0) { need = 2; v = c & 0x1f; min = 0x80; }
        else if (c >= 0xe0 && c < 0xf0) { need = 3; v = c & 0x0f; min = 0x800; }
        else if (c >= 0xf0 && c < 0xf5) { need = 4; v = c & 0x07; min = 0x10000; }
        else return -1;
        if (stop - i < need) return -1;
        for (int j = 1; j < need; j++)
        {
            int b = buf[i + j];
            if ((b & 0xc0) != 0x80) return -1;
            v = (v << 6) | (b & 0x3f);
        }
        if (v < min || v > 0x10ffff || (v >= 0xd800 && v < 0xe000)) return -1;
        return v | (need << 21);
    }

    /** The code point of a decode result.
     * @param decoded from decode, not -1
     * @return the code point */
    public static int codepoint(int decoded)
    {
        return decoded & 0x1fffff;
    }

    /** The number of bytes of a decode result.
     * @param decoded from decode, not -1
     * @return 1 to 4 */
    public static int length(int decoded)
    {
        return decoded >>> 21;
    }

    /** The bytes of the invalid sequence at buf[i], as the C version prints them: 0xC3 0x28,
     * the first byte and up to three continuation bytes after it.
     * @param buf the bytes
     * @param i the start of the invalid sequence
     * @param stop the end of the input, exclusive
     * @return the bytes in hex */
    public static String describeInvalid(byte[] buf, int i, int stop)
    {
        int l = 1;
        while (l < 4 && i + l < stop && (buf[i + l] & 0xc0) == 0x80) l++;
        StringBuilder b = new StringBuilder();
        for (int j = 0; j < l; j++) b.append(String.format(j > 0 ? " 0x%02X" : "0x%02X", buf[i + j] & 0xff));
        return b.toString();
    }
}
