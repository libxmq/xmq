/* libxmq - spdx: MIT */
package org.libxmq.imp.ixml;

import java.util.*;
import org.libxmq.imp.xml.XmlDocument;
import org.libxmq.imp.xml.XmlElement;
import org.libxmq.imp.xml.XmlNode;
import org.libxmq.imp.ixml.IXMLGrammar.Rule;
import org.libxmq.imp.ixml.IXMLGrammar.Term;

/** Conservative, character-level LL(1) analysis and parser-description DOM. */
public final class IXMLAnalyzer {

    /** Fluent element helper on top of the xmq DOM. */
    static final class Element extends XmlElement {
        Element(String name) { super(name); }
        Element attr(String n, String v) { setAttribute(n, v); return this; }
        Element setText(String t) { setContent(t); return this; }
        Element addContent(XmlNode n) { appendRaw(n); return this; }
        Element addContent(Collection<? extends XmlNode> l) { for (XmlNode n : l) appendRaw(n); return this; }
    }
    private static XmlDocument doc(XmlElement root) {
        XmlDocument d = new XmlDocument(); d.setRootElement(root); return d;
    }
    private final IXMLGrammar grammar;
    private final List<Element> diagnostics = new ArrayList<>();
    private final Set<String> reachable = new LinkedHashSet<>();
    private IXMLAnalyzer(IXMLGrammar grammar) { this.grammar = grammar; }

    /** Analyze a grammar and test its predictive parser against the whole input.
     * @param source iXML grammar
     * @param input sample input (not used to infer grammar properties)
     * @return an analysis DOM containing diagnostics or a parser and test result
     */
    public static XmlDocument analyze(String source, String input) {
        return new IXMLAnalyzer(IXMLGrammar.parse(source)).analyze(input);
    }
    private Rule rule(Term t) { return grammar.rules.get(t.ref); }
    private boolean nullable(Term t) { return t.ref != null ? rule(t).nullable : t.chars == null; }
    private BitSet first(Term t) { return t.ref != null ? rule(t).first : t.chars == null ? new BitSet() : t.chars; }
    private boolean nullable(List<Term> terms) { return terms.stream().allMatch(this::nullable); }
    private BitSet first(List<Term> terms) {
        BitSet b = new BitSet();
        for (Term t : terms) { b.or(first(t)); if (!nullable(t)) break; }
        return b;
    }
    private static boolean union(BitSet into, BitSet from) {
        int before = into.cardinality(); into.or(from); return before != into.cardinality();
    }
    private BitSet select(Rule r, List<Term> a) {
        BitSet b = first(a); if (nullable(a)) b.or(r.follow); return b;
    }
    private void diagnose(String code, String rule, String text) {
        diagnostics.add(new Element("diagnostic").attr("code", code).attr("rule", rule).setText(text));
    }
    private void computeSets() {
        boolean changed;
        do {
            changed = false;
            for (Rule r : grammar.rules.values()) for (List<Term> a : r.alternatives) {
                if (!r.nullable && nullable(a)) { r.nullable = true; changed = true; }
                if (!r.productive && a.stream().allMatch(t -> t.ref != null ? rule(t).productive : t.chars == null || !t.chars.isEmpty())) {
                    r.productive = true; changed = true;
                }
                changed |= union(r.first, first(a));
            }
        } while (changed);
        grammar.rules.get(grammar.start).follow.set(IXMLGrammar.EOF);
        do {
            changed = false;
            for (Rule r : grammar.rules.values()) for (List<Term> a : r.alternatives) {
                BitSet suffix = (BitSet)r.follow.clone();
                for (int i = a.size() - 1; i >= 0; i--) {
                    Term t = a.get(i);
                    if (t.ref != null) changed |= union(rule(t).follow, suffix);
                    if (!nullable(t)) suffix.clear();
                    suffix.or(first(t));
                }
            }
        } while (changed);
    }
    private void findReachable() {
        Deque<String> todo = new ArrayDeque<>(); todo.push(grammar.start);
        while (!todo.isEmpty()) {
            String name = todo.pop(); if (!reachable.add(name)) continue;
            for (List<Term> a : grammar.rules.get(name).alternatives)
                for (Term t : a) if (t.ref != null) todo.push(t.ref);
        }
    }
    private void checkRules() {
        Map<String, Set<String>> left = new LinkedHashMap<>();
        for (Rule r : grammar.rules.values()) {
            Set<String> edges = new LinkedHashSet<>(); left.put(r.name, edges);
            for (List<Term> a : r.alternatives) for (Term t : a) {
                if (t.ref != null) edges.add(t.ref);
                if (!nullable(t)) break;
            }
        }
        for (String name : reachable) {
            Rule r = grammar.rules.get(name);
            Set<String> seen = new HashSet<>(); Deque<String> todo = new ArrayDeque<>(left.get(name));
            while (!todo.isEmpty()) {
                String n = todo.pop();
                if (n.equals(name)) { diagnose("left-recursion", name, "Rule can call itself without consuming input."); break; }
                if (seen.add(n)) todo.addAll(left.get(n));
            }
            if (!r.productive) diagnose("nonproductive", name, "Rule cannot derive a finite input string.");
            BitSet used = new BitSet();
            for (int i = 0; i < r.alternatives.size(); i++) {
                BitSet s = select(r, r.alternatives.get(i)), overlap = (BitSet)s.clone(); overlap.and(used);
                if (!overlap.isEmpty()) diagnose("lookahead-conflict", name,
                    "Alternative " + (i + 1) + " overlaps an earlier alternative on " + ranges(overlap) + ".");
                used.or(s);
            }
        }
    }
    private XmlDocument analyze(String input) {
        Element root = new Element("analysis").attr("model", "LL(1)").attr("lookahead", "unicode-codepoint");
        findReachable();
        for (String extension : grammar.extensions) diagnose("unsupported-extension", grammar.start, extension);
        computeSets(); checkRules();
        root.attr("supported", Boolean.toString(diagnostics.isEmpty()));
        if (!diagnostics.isEmpty()) {
            root.addContent(diagnostics);
            root.addContent(new Element("test").attr("status", "not-run"));
            return doc(root);
        }
        Element parser = new Element("parser").attr("start", grammar.start).attr("require-eof", "true");
        root.addContent(parser);
        for (String name : reachable) {
            Rule r = grammar.rules.get(name);
            Element e = new Element("rule").attr("name", name).attr("mark", r.mark)
                .attr("nullable", Boolean.toString(r.nullable)).attr("first", ranges(r.first)).attr("follow", ranges(r.follow));
            if (!r.alias.isEmpty()) e.attr("alias", r.alias);
            parser.addContent(e);
            for (List<Term> a : r.alternatives) {
                Element branch = new Element("alternative").attr("lookahead", ranges(select(r, a))); e.addContent(branch);
                for (Term t : a) {
                    Element operation;
                    if (t.ref != null) operation = new Element("call").attr("rule", t.ref);
                    else if (t.chars != null) operation = new Element("match").attr("set", ranges(t.chars));
                    else operation = new Element("insert").attr("codepoints", codepoints(t.insertion));
                    operation.attr("mark", t.mark);
                    if (!t.alias.isEmpty()) operation.attr("alias", t.alias);
                    branch.addContent(operation);
                }
                if (a.isEmpty()) branch.addContent(new Element("empty"));
            }
        }
        root.addContent(test(input));
        return doc(root);
    }
    // Explicit call stack executes the predictive parser without Java stack limits.
    private Element test(String input) {
        int[] cps = input.codePoints().toArray(); int offset = 0;
        Deque<Term> stack = new ArrayDeque<>(); stack.push(Term.call(grammar.start));
        while (!stack.isEmpty()) {
            Term t = stack.pop(); int cp = offset == cps.length ? IXMLGrammar.EOF : cps[offset];
            if (t.ref != null) {
                Rule r = rule(t); List<Term> chosen = null;
                for (List<Term> a : r.alternatives) if (select(r, a).get(cp)) { chosen = a; break; }
                if (chosen == null) return rejected(offset, "No alternative in " + r.name + " for " + hex(cp));
                for (int i = chosen.size() - 1; i >= 0; i--) stack.push(chosen.get(i));
            } else if (t.chars != null) {
                if (!t.chars.get(cp)) return rejected(offset, "Expected " + ranges(t.chars) + ", found " + hex(cp));
                offset++;
            }
        }
        if (offset != cps.length) return rejected(offset, "Expected EOF");
        return new Element("test").attr("status", "accepted").attr("consumed-codepoints", Integer.toString(offset));
    }
    private Element rejected(int offset, String message) {
        return new Element("test").attr("status", "rejected").attr("offset-codepoints", Integer.toString(offset)).setText(message);
    }
    private static String hex(int cp) { return cp == IXMLGrammar.EOF ? "EOF" : "#" + Integer.toHexString(cp); }
    private static String codepoints(String s) {
        StringJoiner j = new StringJoiner(" "); s.codePoints().forEach(cp -> j.add(hex(cp))); return j.toString();
    }
    private static String ranges(BitSet b) {
        StringJoiner j = new StringJoiner(" ");
        for (int lo = b.nextSetBit(0); lo >= 0;) {
            int end = b.nextClearBit(lo);
            // EOF is a separate lookahead symbol, never part of a scalar range.
            if (lo < IXMLGrammar.EOF && end > IXMLGrammar.EOF) end = IXMLGrammar.EOF;
            j.add(end == lo + 1 ? hex(lo) : hex(lo) + "-" + hex(end - 1));
            lo = b.nextSetBit(end);
        }
        return j.toString();
    }
}
