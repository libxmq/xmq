/* libxmq - Copyright (C) 2026 Fredrik Öhrström (spdx: MIT) */
package org.libxmq.imp.ixml.earley;

import java.util.Arrays;
import java.util.BitSet;

/**
 * The tokens of the Earley parser, like the terminals of yaep in the C version, see
 * doc/parse_ixml_fast.md.
 *
 * A charset can name a huge number of code points, [L] about 130 000 and ~['a'] more than
 * a million, but a parse only sees the code points of its input. So only the code points
 * that are used get a token: a dense number 0..size()-1.
 *
 * - The single character literals of the grammar, eg 'a' or #a, get their tokens first,
 *   in grammar order, when the Grammar is built.
 * - A code point of the input that has no token gets the next one when the input is
 *   scanned (Chart), in input order.
 *
 * Every terminal of the grammar (Grammar.Chars) has a BitSet of the tokens it matches,
 * with one bit per token, not one per code point. A charset gets the tokens of the input
 * code points it contains (Grammar.addUsed), like the charset rules that
 * scan_content_fixup_charsets adds in C. The token sets are small, a few hundred bits for a
 * normal text, and will be used to prune the predictions of the parser: a rule need not be
 * predicted if none of the tokens that can start it is the next token.
 *
 * The tokens are kept by the grammar between parses, like used_unicodes in C: with --lines
 * only the code points that are new on a line are added.
 *
 * A code point is found by a two level table: the page (code point / 256), then the offset in
 * the page. Only the pages of used code points are allocated, so any input is fast and small,
 * also when the code points are far apart, eg 'a' and a tag character #e0001.
 */
public final class Tokens
{
    private static final int PAGE_BITS = 8;
    private static final int PAGE_SIZE = 1 << PAGE_BITS;
    /** pages[cp >> 8][cp & 0xff] is the token + 1 of the code point, 0 for no token. */
    private final int[][] pages = new int[(0x10ffff >> PAGE_BITS) + 1][];
    /** The code point of each token. */
    private int[] codes = new int[64];
    private int size;
    /** The tokens whose code point has been seen in an input. */
    private final BitSet used = new BitSet();

    Tokens() { }

    /** The token of the code point.
     * @param codepoint the code point, 0 to 0x10ffff
     * @return the token, or -1 if the code point has no token */
    public int token(int codepoint)
    {
        int[] page = pages[codepoint >> PAGE_BITS];
        return page == null ? -1 : page[codepoint & (PAGE_SIZE - 1)] - 1;
    }

    /** The token of the code point, a new token if it has none.
     * @param codepoint the code point, 0 to 0x10ffff
     * @return the token */
    int tokenOrAdd(int codepoint)
    {
        int[] page = pages[codepoint >> PAGE_BITS];
        if (page == null) page = pages[codepoint >> PAGE_BITS] = new int[PAGE_SIZE];
        int t = page[codepoint & (PAGE_SIZE - 1)] - 1;
        if (t >= 0) return t;
        if (size == codes.length) codes = Arrays.copyOf(codes, size * 2);
        codes[size] = codepoint;
        page[codepoint & (PAGE_SIZE - 1)] = size + 1;
        return size++;
    }

    /** Mark the token as seen in an input.
     * @param token the token
     * @return true if it was not seen before: its code point is new to the charsets */
    boolean markUsed(int token)
    {
        if (used.get(token)) return false;
        used.set(token);
        return true;
    }

    /** The code point of a token.
     * @param token the token, 0 to size()-1
     * @return the code point */
    public int code(int token)
    {
        return codes[token];
    }

    /** The number of tokens.
     * @return the number of tokens */
    public int size()
    {
        return size;
    }

    /** The tokens whose code points have been seen in the inputs parsed so far.
     * @return a copy of the used tokens */
    public BitSet used()
    {
        return (BitSet)used.clone();
    }
}
