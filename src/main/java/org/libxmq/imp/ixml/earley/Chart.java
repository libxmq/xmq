/* libxmq - Copyright (C) 2026 Fredrik Öhrström (spdx: MIT) */
package org.libxmq.imp.ixml.earley;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * The result of the Earley recognizer: one Earley set for each input position, from 0
 * (before the first code point) to n (after the last).
 *
 * The input is read from its UTF-8 bytes, it is never turned into a String or an array
 * of code points. The chart is built by a single scan of the bytes, like
 * scan_content_fixup_charsets in the C version (see doc/parse_ixml_fast.md): every code
 * point is decoded, gets its token (Tokens) and the set of its position remembers the
 * token. The code points that the grammar has not seen in an input before are then added
 * to the token sets of the charsets that contain them (Grammar.addUsed).
 */
public final class Chart
{
    /** The input is not valid UTF-8. */
    public static final class InvalidUtf8 extends RuntimeException
    {
        /** The code point position of the invalid bytes. */
        public final int position;
        /** The byte offset of the invalid bytes. */
        public final int offset;

        InvalidUtf8(String message, int position, int offset)
        {
            super(message);
            this.position = position;
            this.offset = offset;
        }
    }

    /** The tokens of the grammar, to find the code point of a token. */
    public final Tokens tokens;
    /** The sets, sets.get(k) is the set at position k. */
    public final List<EarleySet> sets = new ArrayList<>();

    /** Scan the input and create the sets.
     * @param grammar the grammar, its tokens and charsets get the new code points of the input
     * @param buf the input bytes
     * @param start the first byte of the input
     * @param stop the end of the input, exclusive
     * @throws InvalidUtf8 if the input is not valid UTF-8 */
    Chart(Grammar grammar, byte[] buf, int start, int stop)
    {
        tokens = grammar.tokens;
        // The code points seen for the first time by the grammar.
        int[] fresh = new int[16];
        int num_fresh = 0;
        try
        {
            int i = start;
            while (i < stop)
            {
                int d = Utf8.decode(buf, i, stop);
                if (d < 0)
                {
                    throw new InvalidUtf8("invalid UTF-8: " + Utf8.describeInvalid(buf, i, stop), sets.size(), i);
                }
                int cp = Utf8.codepoint(d);
                int token = tokens.tokenOrAdd(cp);
                if (tokens.markUsed(token))
                {
                    if (num_fresh == fresh.length) fresh = Arrays.copyOf(fresh, num_fresh * 2);
                    fresh[num_fresh++] = cp;
                }
                sets.add(new EarleySet(sets.size(), token));
                i += Utf8.length(d);
            }
            sets.add(new EarleySet(sets.size(), -1));
        }
        finally
        {
            // Also on invalid UTF-8, the tokens marked as used must be in the charsets
            // for the next parse with the grammar. The same order as C, by code point.
            Arrays.sort(fresh, 0, num_fresh);
            grammar.addUsed(fresh, num_fresh);
        }
    }

    /** The set at position k.
     * @param k the input position, 0 to length()
     * @return the set */
    public EarleySet set(int k)
    {
        return sets.get(k);
    }

    /** The token at position k.
     * @param k the input position, 0 to length()-1
     * @return the token */
    public int token(int k)
    {
        return sets.get(k).token;
    }

    /** The code point at position k.
     * @param k the input position, 0 to length()-1
     * @return the code point */
    public int codepoint(int k)
    {
        return tokens.code(sets.get(k).token);
    }

    /** The number of code points in the input.
     * @return the input length */
    public int length()
    {
        return sets.size() - 1;
    }
}
