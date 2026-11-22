package org.libxmq.imp.ixml.earley;

import java.nio.charset.StandardCharsets;
import java.util.BitSet;

import org.junit.Test;
import static org.junit.Assert.*;

public class TokensTest {
    private static byte[] utf8(String s) { return s.getBytes(StandardCharsets.UTF_8); }

    /** The terminal [L] of the grammar name = -'Hej ', [L]+, '!'. */
    private static Grammar.Chars letters(Grammar g) {
        for (Grammar.Production p : g.productions)
            for (Grammar.Symbol s : p.symbols)
                if (s instanceof Grammar.Chars c && c.text().equals("[L]")) return c;
        throw new AssertionError("no [L]");
    }

    private static BitSet tokensOf(Grammar g, String chars) {
        BitSet b = new BitSet();
        chars.codePoints().forEach(cp -> b.set(g.tokens.token(cp)));
        return b;
    }

    private static BitSet bits(int from, int to) {
        BitSet b = new BitSet();
        b.set(from, to);
        return b;
    }

    @Test public void literalsGetTokensFirstThenTheInputInInputOrder() {
        Grammar g = Earley.grammar("name = -'Hej ', [L]+, '!'.");
        // H e j space ! from the grammar.
        assertEquals(5, g.tokens.size());
        assertEquals(tokensOf(g, "Hej !"), bits(0, 5));
        assertEquals(-1, g.tokens.token('a'));
        assertTrue(letters(g).tokens().isEmpty());

        assertNull(Earley.parse(g, utf8("Hej ba!"), 0, 7).error());
        assertEquals(7, g.tokens.size());
        assertEquals(5, g.tokens.token('b'));
        assertEquals(6, g.tokens.token('a'));
        // The letters seen in the input, also those read by the literal 'Hej ', like C.
        assertEquals(tokensOf(g, "Hejab"), letters(g).tokens());
        assertEquals(tokensOf(g, "Hej ab!"), g.tokens.used());
    }

    @Test public void laterParsesOnlyAddTheNewCodePoints() {
        Grammar g = Earley.grammar("name = -'Hej ', [L]+, '!'.");
        byte[] lines = utf8("Hej aaa!\nHej Hå!\nHej 𝄞!");
        assertNull(Earley.parse(g, lines, 0, 8).error());
        assertEquals(tokensOf(g, "Heja"), letters(g).tokens());
        int size = g.tokens.size();
        assertNull(Earley.parse(g, lines, 9, 17).error());
        assertEquals(size + 1, g.tokens.size());
        assertEquals(tokensOf(g, "Hejaå"), letters(g).tokens());
        // A code point far from the others, not a letter: a token, but not in [L].
        Earley.Result r = Earley.parse(g, lines, 18, lines.length);
        assertEquals("syntax error", r.error());
        assertEquals(4, r.position());
        assertEquals(0x1d11e, r.chart().codepoint(4));
        assertEquals(tokensOf(g, "Hejaå"), letters(g).tokens());
    }

    @Test public void excludedCharsets() {
        Grammar g = Earley.grammar("s = ~['a'; Nd]+.");
        Earley.Result r = Earley.parse(g, utf8("xyz"), 0, 3);
        assertNull(r.error());
        r = Earley.parse(g, utf8("xa"), 0, 2);
        assertEquals(1, r.position());
        r = Earley.parse(g, utf8("x7"), 0, 2);
        assertEquals(1, r.position());
    }

    @Test public void invalidUtf8() {
        Grammar g = Earley.grammar("name = -'Hej ', [L]+, '!'.");
        byte[] bad = { 'H', 'e', 'j', ' ', 'q', (byte)0xc3, '(', '!' };
        Earley.Result r = Earley.parse(g, bad, 0, bad.length);
        assertEquals("invalid UTF-8: 0xC3", r.error());
        assertEquals(5, r.position());
        assertNull(r.chart());
        // The letters seen before the invalid bytes are in [L] for the next parse.
        assertEquals(tokensOf(g, "Hejq"), letters(g).tokens());
        assertNull(Earley.parse(g, utf8("Hej q!"), 0, 6).error());
    }

    @Test public void decode() {
        byte[] b = utf8("aå€𝄞");
        int i = 0;
        int[] expected = { 'a', 0xe5, 0x20ac, 0x1d11e };
        for (int k = 0; k < expected.length; k++) {
            int d = Utf8.decode(b, i, b.length);
            assertEquals(expected[k], Utf8.codepoint(d));
            assertEquals(k + 1, Utf8.length(d));
            i += Utf8.length(d);
        }
        assertEquals(-1, Utf8.decode(new byte[] { (byte)0xc0, (byte)0x80 }, 0, 2)); // Overlong.
        assertEquals(-1, Utf8.decode(new byte[] { (byte)0xed, (byte)0xa0, (byte)0x80 }, 0, 3)); // Surrogate.
        assertEquals(-1, Utf8.decode(new byte[] { (byte)0xe2, (byte)0x82 }, 0, 2)); // Truncated.
        assertEquals(-1, Utf8.decode(new byte[] { (byte)0x80 }, 0, 1)); // Stray continuation.
    }
}
