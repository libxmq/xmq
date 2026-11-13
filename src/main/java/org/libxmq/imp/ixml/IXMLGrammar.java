/* libxmq - Copyright (C) 2024-2026 Fredrik Öhrström (spdx: MIT)
 * Java port of the grammar reader in src/main/c/parts/ixml.c.
 */
package org.libxmq.imp.ixml;

import java.util.*;

/** Grammar reader ported from ixml.c; independent of the YAEP backend. */
public final class IXMLGrammar {
    static final int EOF = 0x110000;
    final Map<String, Rule> rules = new LinkedHashMap<>();
    final Set<String> extensions = new LinkedHashSet<>();
    String start;
    private final String source;
    private int pos, serial;
    private String current; // The rule being read.

    static final class Rule {
        final String name;
        String mark = "", alias = "";
        int cost; // Number of < after = or :, used to prefer cheaper alternatives.
        final List<List<Term>> alternatives = new ArrayList<>();
        final BitSet first = new BitSet(), follow = new BitSet();
        boolean nullable, productive;
        // Left recursion elimination (see eliminateLeftRecursion):
        // lrTail is set on a rewritten rule A, the name of its loop rule.
        // lrOwner is set on the loop rule, the name of A, and lrWrap has for
        // each alternative of the loop rule the element name that wraps the
        // content of A so far, or null for no wrapping.
        String lrTail, lrOwner;
        // For a generated rule (/1, /2, ...) the named rule it was made for, used in error messages.
        String origin;
        List<String> lrWrap;
        Rule(String name) { this.name = name; }
    }
    static final class Term {
        String ref, insertion, mark = "", alias = "";
        BitSet chars;
        // A negative lookahead !'abc' or ![set]: the following input must not
        // start with a code point sequence matching these sets in order.
        List<BitSet> not;
        static Term call(String ref) { Term t = new Term(); t.ref = ref; return t; }
        static Term match(BitSet chars) { Term t = new Term(); t.chars = chars; return t; }
    }
    private IXMLGrammar(String source) { this.source = source; }

    /** Read a grammar using the parse_ixml_* productions from the C reader.
     * @param source iXML source
     * @return normalized grammar
     */
    public static IXMLGrammar parse(String source) {
        IXMLGrammar g = new IXMLGrammar(source);
        g.parseIxml();
        return g;
    }
    /**
     * Rewrite direct left recursion, A = y1 | y2 | A, x1 | A, x2, into a loop:
     * A = y1, T | y2, T and T = x1, T | x2, T | (), like ANTLR 4. The parse
     * tree is still the left nested ixml tree: at the start of each turn of
     * the loop, the parser wraps the content that A has produced so far into
     * a new element (the inner A), unless the inner A is hidden. A left
     * recursive call through an optional, A = A?, x, is expanded first.
     * Rules where the inner A is an attribute, or with A = A, are left
     * alone (and reported as left recursive by the analysis).
     */
    void eliminateLeftRecursion() {
        for (Rule a : new ArrayList<>(rules.values())) {
            if (a.lrOwner != null || a.lrTail != null) continue;
            // Expand A = G, rest where G = A | () into A = A, rest | rest.
            List<List<Term>> expanded = new ArrayList<>();
            boolean viaOptional = false;
            for (List<Term> alt : a.alternatives) {
                Term g = alt.isEmpty() ? null : optionalOf(alt.get(0), a.name);
                if (g == null) { expanded.add(alt); continue; }
                viaOptional = true;
                List<Term> with = new ArrayList<>(alt); with.set(0, g);
                expanded.add(with);
                expanded.add(new ArrayList<>(alt.subList(1, alt.size())));
            }
            List<List<Term>> base = new ArrayList<>(), rec = new ArrayList<>();
            for (List<Term> alt : expanded) {
                if (!alt.isEmpty() && a.name.equals(alt.get(0).ref)) rec.add(alt); else base.add(alt);
            }
            if (rec.isEmpty() || base.isEmpty()) continue;
            List<String> wraps = new ArrayList<>();
            boolean ok = true;
            for (List<Term> alt : rec) {
                Term t = alt.get(0);
                String m = !t.mark.isEmpty() ? t.mark : !a.mark.isEmpty() ? a.mark : "^";
                if (alt.size() == 1 || m.equals("@") || m.equals("*")) { ok = false; break; }
                wraps.add(m.equals("-") ? null : !t.alias.isEmpty() ? t.alias : !a.alias.isEmpty() ? a.alias : a.name);
            }
            if (!ok) continue;
            Rule tail = generated();
            tail.origin = a.origin != null ? a.origin : a.name;
            tail.lrOwner = a.name;
            tail.lrWrap = wraps;
            for (List<Term> alt : rec) {
                List<Term> l = new ArrayList<>(alt.subList(1, alt.size()));
                l.add(call(tail));
                tail.alternatives.add(l);
            }
            tail.alternatives.add(new ArrayList<>());
            tail.lrWrap.add(null);
            a.alternatives.clear();
            for (List<Term> alt : base) {
                List<Term> l = new ArrayList<>(alt);
                l.add(call(tail));
                a.alternatives.add(l);
            }
            a.lrTail = tail.name;
            if (viaOptional) { /* The optional rule may now be unused, which is harmless. */ }
        }
    }
    // If t calls a generated rule G = A | () (in any order), return the call of A in G.
    private Term optionalOf(Term t, String a) {
        if (t.ref == null || !t.ref.startsWith("/")) return null;
        Rule g = rules.get(t.ref);
        if (g.alternatives.size() != 2) return null;
        Term found = null;
        boolean empty = false;
        for (List<Term> alt : g.alternatives) {
            if (alt.isEmpty()) empty = true;
            else if (alt.size() == 1 && a.equals(alt.get(0).ref)) found = alt.get(0);
        }
        return empty ? found : null;
    }
    private int peek() { return pos == source.length() ? -1 : source.codePointAt(pos); }
    private int take() { int c = peek(); if (c >= 0) pos += Character.charCount(c); return c; }
    private boolean eat(int c) { if (peek() != c) return false; take(); return true; }
    private void expect(int c) { if (!eat(c)) throw error("expected '" + (char)c + "'"); whitespace(); }
    private IllegalArgumentException error(String message) {
        int line = 1, column = 1;
        for (int i = 0; i < pos; i++) { if (source.charAt(i) == '\n') { line++; column = 1; } else column++; }
        return new IllegalArgumentException("ixml:" + line + ":" + column + ": " + message);
    }
    // parse_ixml_whitespace / parse_ixml_comment. Iterative nesting avoids stack overflow.
    private void whitespace() {
        while (true) {
            int c = peek();
            if (c == '{') {
                take(); int depth = 1;
                while (depth != 0) {
                    c = take();
                    if (c < 0) throw error("comment is not closed");
                    if (c == '{') depth++;
                    if (c == '}') depth--;
                }
            } else if (c == ' ' || c == '\t' || c == '\n' || c == '\r' ||
                       (c >= 0 && Character.getType(c) == Character.SPACE_SEPARATOR)) take();
            else return;
        }
    }
    private void parseIxml() {
        whitespace();
        int saved = pos;
        if (source.startsWith("ixml", pos)) {
            pos += 4; int after = pos; whitespace();
            if (pos > after && source.startsWith("version", pos)) {
                pos += 7; after = pos; whitespace();
                if (pos == after) throw error("expected whitespace after version");
                string(); whitespace(); expect('.');
            } else pos = saved;
        }
        while (peek() >= 0) parseRule();
        if (start == null) throw error("expected rule");
        for (Rule r : rules.values()) for (List<Term> a : r.alternatives) for (Term t : a)
            if (t.ref != null && !rules.containsKey(t.ref)) throw error("undefined rule: " + t.ref);
    }
    private String mark() {
        int c = peek();
        if (c == '@' || c == '^' || c == '-' || c == '*') {
            take(); whitespace();
            if (c == '*') extensions.add("controlled ambiguity (*)");
            return Character.toString(c);
        }
        return "";
    }
    private static boolean nameStart(int c) { return c == '_' || Character.isLetter(c); }
    private static boolean nameFollower(int c) {
        return nameStart(c) || c == '-' || c == '.' || c == 0xb7 || c == 0x203f || c == 0x2040 ||
            Character.getType(c) == Character.DECIMAL_DIGIT_NUMBER || Character.getType(c) == Character.NON_SPACING_MARK;
    }
    private String name() {
        if (!nameStart(peek())) throw error("expected name");
        int begin = pos; take();
        while (peek() >= 0 && nameFollower(peek())) {
            // Same trailing-dot disambiguation as is_ixml_name_follower in C.
            if (peek() == '.' && (pos + 1 == source.length() || !nameFollower(source.codePointAt(pos + 1)))) break;
            take();
        }
        String value = source.substring(begin, pos); whitespace(); return value;
    }
    private String alias() { if (!eat('>')) return ""; whitespace(); return name(); }
    private void parseRule() {
        String m = mark(), n = name(), a = alias();
        if (rules.containsKey(n)) throw error("duplicate rule: " + n);
        Rule r = new Rule(n); r.mark = m; r.alias = a; rules.put(n, r);
        current = n;
        if (start == null) start = n;
        if (!eat(':') && !eat('=')) throw error("expected equal or colon");
        while (eat('<')) r.cost++;
        whitespace(); r.alternatives.addAll(alts()); expect('.');
    }
    // parse_ixml_alts / parse_ixml_alt; retain empty alternatives explicitly.
    private List<List<Term>> alts() {
        List<List<Term>> result = new ArrayList<>();
        do {
            whitespace(); List<Term> sequence = new ArrayList<>();
            if (peek() != '.' && peek() != ')' && peek() != ';' && peek() != '|') {
                sequence.add(term());
                while (eat(',')) { whitespace(); sequence.add(term()); }
            }
            result.add(sequence);
        } while (eat(';') || eat('|'));
        return result;
    }
    private Rule generated() {
        Rule r = new Rule("/" + (++serial)); r.mark = "-"; r.origin = current; rules.put(r.name, r); return r;
    }
    private Term call(Rule r) { Term t = Term.call(r.name); t.mark = "-"; return t; }
    // make_last_term_*: same anonymous-rule lowering, but repetitions are
    // right-recursive and factored instead of YAEP's left-recursive form.
    private Term term() {
        Term factor = factor();
        if (eat('?')) {
            Rule r = generated(); r.alternatives.add(List.of(factor)); r.alternatives.add(List.of());
            whitespace(); return call(r);
        }
        int c = peek();
        if (c != '*' && c != '+') return factor;
        take(); boolean separated = eat(c); whitespace();
        Term sep = separated ? factor() : null;
        Rule tail = generated();
        tail.alternatives.add(separated ? List.of(sep, factor, call(tail)) : List.of(factor, call(tail)));
        tail.alternatives.add(List.of());
        if (c == '*' && !separated) return call(tail);
        Rule head = generated(); head.alternatives.add(List.of(factor, call(tail)));
        if (c == '*') head.alternatives.add(List.of());
        return call(head);
    }
    private Term factor() {
        if (eat('(')) {
            whitespace(); Rule r = generated(); r.alternatives.addAll(alts()); expect(')'); return call(r);
        }
        if (eat('+')) {
            whitespace(); Term t = new Term(); t.insertion = literal(); whitespace(); return t;
        }
        if (eat('!')) {
            // parse_ixml_not: a zero width negative lookahead on a string or a charset.
            whitespace();
            Term t = new Term();
            t.not = new ArrayList<>();
            if (peek() == '[' || peek() == '~') t.not.add(charset());
            else literal().codePoints().forEach(cp -> { BitSet b = new BitSet(); b.set(cp); t.not.add(b); });
            whitespace(); return t;
        }
        String m = mark(); Term t;
        if (peek() == '[' || peek() == '~') t = Term.match(charset());
        else if (peek() == '\'' || peek() == '"' || peek() == '#') {
            String value = literal();
            List<Term> chars = new ArrayList<>();
            value.codePoints().forEach(cp -> { BitSet b = new BitSet(); b.set(cp); Term ch = Term.match(b); ch.mark = m; chars.add(ch); });
            if (chars.size() == 1) t = chars.get(0);
            else { Rule r = generated(); r.alternatives.add(chars); t = call(r); }
        } else {
            t = Term.call(name()); t.alias = alias(); t.mark = m; return t;
        }
        if (m.equals("@") || m.equals("*")) throw error("terminal mark must be ^ or -");
        if (t.ref == null) t.mark = m;
        whitespace(); return t;
    }
    private String literal() {
        if (peek() == '#') return new String(Character.toChars(encoded()));
        return string();
    }
    // parse_ixml_string: doubled quote escapes, codepoint decoding, bounded EOF.
    private String string() {
        int quote = take();
        if (quote != '\'' && quote != '"') throw error("expected string or encoded character");
        StringBuilder b = new StringBuilder();
        while (true) {
            int c = take(); if (c < 0) throw error("string not terminated");
            if (c == quote && !eat(quote)) return b.toString();
            if (c >= 0xd800 && c <= 0xdfff) throw error("invalid Unicode scalar");
            b.appendCodePoint(c);
        }
    }
    private int encoded() {
        if (!eat('#')) throw error("expected #");
        int begin = pos; long value = 0;
        while (peek() >= 0 && ((peek() >= '0' && peek() <= '9') || (peek() >= 'a' && peek() <= 'f') || (peek() >= 'A' && peek() <= 'F'))) {
            value = value * 16 + Character.digit(take(), 16);
            if (value > 0x10ffff) throw error("encoded character out of range");
        }
        if (pos == begin || (value >= 0xd800 && value <= 0xdfff)) throw error("invalid encoded character");
        return (int)value;
    }
    private BitSet charset() {
        boolean exclude = eat('~'); whitespace(); expect('['); BitSet bits = new BitSet();
        if (peek() != ']') do {
            whitespace();
            if (peek() >= 'A' && peek() <= 'Z') {
                String category = Character.toString(take());
                if ((peek() >= 'a' && peek() <= 'z') || peek() == 'C') category += Character.toString(take());
                bits.or(category(category)); whitespace();
            } else {
                String from = literal(); whitespace();
                if (eat('-')) {
                    whitespace(); String to = literal(); whitespace();
                    if (from.codePointCount(0, from.length()) != 1 || to.codePointCount(0, to.length()) != 1)
                        throw error("range endpoints must be single characters");
                    int lo = from.codePointAt(0), hi = to.codePointAt(0);
                    if (lo > hi) throw error("reversed character range");
                    bits.set(lo, hi + 1);
                } else from.codePoints().forEach(bits::set);
            }
        } while (eat(';') || eat('|'));
        expect(']');
        if (exclude) bits.flip(0, EOF);
        bits.clear(0xd800, 0xe000); return bits;
    }
    private BitSet category(String code) {
        String valid = "L Lu Ll Lt Lm Lo LC M Mn Mc Me N Nd Nl No P Pc Pd Ps Pe Pi Pf Po S Sm Sc Sk So Z Zs Zl Zp C Cc Cf Cs Co Cn";
        if (!Arrays.asList(valid.split(" ")).contains(code)) throw error("unknown Unicode category: " + code);
        String[] categories = {"Cn", "Lu", "Ll", "Lt", "Lm", "Lo", "Mn", "Me", "Mc", "Nd",
            "Nl", "No", "Zs", "Zl", "Zp", "Cc", "Cf", "", "Co", "Cs", "Pd", "Ps", "Pe", "Pc",
            "Po", "Sm", "Sc", "Sk", "So", "Pi", "Pf"};
        BitSet b = new BitSet();
        for (int cp = 0; cp < EOF; cp++) {
            String actual = categories[Character.getType(cp)];
            if (!(cp >= 0xd800 && cp <= 0xdfff) &&
                (actual.equals(code) || (code.equals("LC") && (actual.equals("Lu") || actual.equals("Ll") || actual.equals("Lt"))) || (code.length() == 1 && actual.startsWith(code)))) b.set(cp);
        }
        return b;
    }
}
