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
        conflict("s: s, 'a'; 'b'.", "left-recursion");
        conflict("s: a. a: b. b: s; 'x'.", "left-recursion");
        conflict("s: a, s; 'x'. a: .", "left-recursion");
        conflict("s: ('a'?)*.", "left-recursion");
        conflict("s: s.", "nonproductive");
    }
    @Test public void firstFollowConflicts() {
        conflict("s: 'ab'; 'ac'.", "lookahead-conflict");
        conflict("s: 'a'?, 'a'.", "lookahead-conflict");
        conflict("s: a, 'x'. a: 'x'; .", "lookahead-conflict");
        conflict("s: ; .", "lookahead-conflict");
        conflict("s: ['a'-'z']; ['m'-'z'].", "lookahead-conflict");
        conflict("s: ~['a']; 'b'.", "lookahead-conflict");
    }
    @Test public void cReaderSyntax() {
        accepts("{outer {inner} tail} ixml version '1.0'. -s > root = @a > value, +'ok', -#a. a: 'it''s'.", "it's\n");
        accepts("s: (); 'x'.", "");
        accepts("s: a.b. a.b: 'x'.", "x");
        accepts("s: ~['x'; #a], [#1f600-#1f64f].", "y😀");
        accepts("s: [Nd]+.", "123");
        accepts("s: #0.", "\u0000");
        conflict("s: !'x', 'a'.", "unsupported-extension");
        conflict("*s:< 'a'.", "unsupported-extension");
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
