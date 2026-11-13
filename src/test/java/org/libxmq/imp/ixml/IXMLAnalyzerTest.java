package org.libxmq.imp.ixml;

import org.libxmq.imp.xml.XmlElement;
import org.junit.Test;
import static org.junit.Assert.*;

public class IXMLAnalyzerTest {
    private XmlElement analyze(String grammar, String input) {
        return IXMLAnalyzer.analyze(grammar, input).rootElement();
    }
    private void accepts(String grammar, String input) {
        XmlElement e = analyze(grammar, input);
        assertEquals(e.toString(), "true", e.attributeValue("supported"));
        assertEquals("accepted", e.element("test").attributeValue("status"));
        assertNotNull(e.element("parser"));
    }
    private void conflict(String grammar, String code) {
        XmlElement e = analyze(grammar, "");
        assertEquals("false", e.attributeValue("supported"));
        assertTrue(e.elements().stream().filter(d -> d.name().equals("diagnostic")).anyMatch(d -> code.equals(d.attributeValue("code"))));
        assertNull(e.element("parser"));
    }
    @Test public void alternativesAndWholeInput() {
        accepts("s: 'a'; 'b'.", "a");
        assertEquals("rejected", analyze("s: 'a'.", "ab").element("test").attributeValue("status"));
        assertEquals("rejected", analyze("s: 'a'.", "").element("test").attributeValue("status"));
    }
    @Test public void repetitions() {
        accepts("s: 'a'*, 'b'+, 'c'?.", "aaabbc");
        accepts("s: 'ab'+.", "abab");
        accepts("s: 'a'**','.", "");
        accepts("s: 'a'++','.", "a,a,a");
        assertEquals("rejected", analyze("s: 'a'++','.", "a,").element("test").attributeValue("status"));
    }
    @Test public void recursionAndNullable() {
        accepts("s: '(', s, ')', s; .", "(())()");
        accepts("s: a, 'b'. a: .", "b");
        accepts("s: s, 'a'; 'b'.", "baa"); // Direct left recursion is rewritten, see leftRecursion.
        conflict("s: a. a: b. b: s; 'x'.", "left-recursion");
        conflict("s: a, s; 'x'. a: .", "left-recursion");
        conflict("s: ('a'?)*.", "left-recursion");
        conflict("s: s.", "nonproductive");
    }
    @Test public void llStarLookahead() {
        // LL(1) conflicts that a longer lookahead resolves.
        accepts("s: 'ab'; 'ac'.", "ac");
        accepts("s: 'a'?, 'a'.", "a");
        accepts("s: 'a'?, 'a'.", "aa");
        accepts("s: a, 'x'. a: 'x'; .", "x");
        accepts("s: a, 'x'. a: 'x'; .", "xx");
        // Unbounded but regular lookahead.
        accepts("s: a, 'x'; a, 'y'. a: 'a'*.", "aaaay");
        accepts("s: w, '.'; w, '!'. w: ['a'-'z']+.", "hello!");
    }
    @Test public void llStarFailures() {
        conflict("s: ; .", "ambiguous");
        conflict("s: ['a'-'z']; ['m'-'z'].", "ambiguous");
        conflict("s: ~['a']; 'b'.", "ambiguous");
        conflict("s: 'a'*, 'a'*.", "ambiguous");
        // Balanced parentheses before the deciding character are not regular.
        conflict("s: p, 'x'; p, 'y'. p: '(', p, ')'; .", "non-regular");
    }
    @Test public void costs() {
        // The alternative with the least cost wins an ambiguity.
        accepts("s: a; b. a: 'x'. b =< 'x'.", "x");
        assertEquals("a", IXMLAnalyzer.parse("s: a; b. a: 'x'. b =< 'x'.", "x").rootElement().firstChild().name());
        assertEquals("b", IXMLAnalyzer.parse("s: a; b. a =<< 'x'. b =< 'x'.", "x").rootElement().firstChild().name());
        // Equal costs are still ambiguous.
        conflict("s: a; b. a =< 'x'. b =< 'x'.", "ambiguous");
    }
    private void rejects(String grammar, String input) {
        assertEquals("rejected", analyze(grammar, input).element("test").attributeValue("status"));
    }
    @Test public void negativeLookahead() {
        // Without ! two numbers after each other can be split anywhere.
        conflict("s: n, (' '?, n)*. n: [N]+.", "ambiguous");
        // With ! the number is greedy and the grammar is LL(*).
        accepts("s: (w; n; -' ')+. w: [Ll]+, ![Ll]. n: [N]+, ![N].", "h hh1h11 12");
        assertEquals("[w, w, n, w, n, n]", names(IXMLAnalyzer.parse("s: (w; n; -' ')+. w: [Ll]+, ![Ll]. n: [N]+, ![N].", "h hh1h11 12")));
        // A string predicate, any prefix of the forbidden string is fine.
        accepts("s: !'chapter', [L]+.", "chap");
        rejects("s: !'chapter', [L]+.", "chapterchopper");
        // At EOF every negative lookahead succeeds.
        accepts("s: [L], !'a'.", "b");
        rejects("s: vp, ~[]. vp: [L], !'a'.", "aa");
        // A predicate decides an alternative: keywords versus names.
        // The keyword wins over the more expensive name, but iffy is a name.
        String kw = "s: (k; i)++' '. k: 'if', ![L]. i =< [L]+, ![L].";
        accepts(kw, "if iffy");
        assertEquals("[k, i]", names(IXMLAnalyzer.parse(kw, "if iffy")));
        // The predicate kills the empty alternative of the loop, so the
        // exiting alternative is predicted after one symbol.
        accepts("s: 'x', 'y'*, !'z', ~[].", "xyyw");
        rejects("s: 'x', 'y'*, !'z', ~[].", "xyz");
    }
    @Test public void leftRecursion() {
        // Direct left recursion is rewritten into a loop, the tree stays left nested.
        String g = "e: -t; e, '+', t. t: [N]+, ![N].";
        accepts(g, "1+2+3");
        var root = IXMLAnalyzer.parse(g, "1+2+3").rootElement();
        assertEquals("e", root.name());
        assertEquals("e", root.firstChild().name());
        assertEquals("e", root.firstChild().firstChild().name());
        assertEquals("1", root.firstChild().firstChild().textContent());
        assertEquals("1+2+3", root.textContent());
        // Through an optional.
        accepts("d: '(', d, ')'; d?, '[', ']'.", "([])[][]");
        // A = A is a cycle and indirect left recursion is not rewritten.
        conflict("s: a; s.  a: 'x'.", "left-recursion");
        conflict("s: a, 'x'; 'y'. a: s, 'z'.", "left-recursion");
    }
    private static String names(org.libxmq.imp.xml.XmlDocument d) {
        java.util.List<String> l = new java.util.ArrayList<>();
        for (var n = d.rootElement().firstChild(); n != null; n = n.next())
            if (n instanceof org.libxmq.imp.xml.XmlElement e) l.add(e.name());
        return l.toString();
    }
    @Test public void cReaderSyntax() {
        accepts("{outer {inner} tail} ixml version '1.0'. -s > root = @a > value, +'ok', -#a. a: 'it''s'.", "it's\n");
        accepts("s: (); 'x'.", "");
        accepts("s: a.b. a.b: 'x'.", "x");
        accepts("s: ~['x'; #a], [#1f600-#1f64f].", "y😀");
        accepts("s: [Nd]+.", "123");
        accepts("s: #0.", "\u0000");
        conflict("*s: 'a'.", "unsupported-extension");
    }
    @Test public void existingCGrammarFixtures() throws Exception {
        try (var paths = java.nio.file.Files.walk(java.nio.file.Path.of("tests/ixml/correct"))) {
            for (var path : paths.filter(p -> p.toString().endsWith(".ixml")).toList()) {
                try { IXMLGrammar.parse(java.nio.file.Files.readString(path)); }
                catch (IllegalArgumentException e) { throw new AssertionError(path + ": " + e.getMessage(), e); }
            }
        }
        IXMLGrammar.parse(java.nio.file.Files.readString(java.nio.file.Path.of("library/core/ixml.ixml")));
    }
    @Test public void malformedGrammar() {
        String[] invalid = {"", "s: 'x'", "s: missing.", "s: 'a'. s: 'b'.", "s: 'a'. garbage",
            "s: [#110000].", "s: #d800.", "s: ['z'-'a'].", "s: [Xx].", "s: 'x'. {", "s: 'unterminated", "s: 'a', ."};
        for (String g : invalid) {
            try { analyze(g, ""); fail("Should reject: " + g); }
            catch (IllegalArgumentException expected) { assertTrue(expected.getMessage().startsWith("ixml:")); }
        }
    }
    @Test public void inputDoesNotAffectAnalysis() {
        assertEquals("true", analyze("s: 'a'.", "wrong").attributeValue("supported"));
        accepts("s: 'a'*. unused: unused.", "a".repeat(20000));
    }
}
