/* libxmq - spdx: MIT */
package org.libxmq.imp.ixml;

import java.util.*;
import org.libxmq.imp.xml.XmlDocument;
import org.libxmq.imp.xml.XmlElement;
import org.libxmq.imp.xml.XmlNode;
import org.libxmq.imp.xml.XmlText;
import org.libxmq.imp.ixml.IXMLGrammar.Rule;
import org.libxmq.imp.ixml.IXMLGrammar.Term;

/** Character-level LL(*) analysis and parser-description DOM. Every decision gets a
 *  lookahead DFA, see IXMLLookahead. */
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
    private final IXMLLookahead lookahead;
    private final Map<String, IXMLLookahead.Dfa> dfas = new LinkedHashMap<>();
    // Decisions without a static DFA, predicted at runtime, with the reason.
    private final Map<String, IXMLLookahead.Failure> dynamic = new LinkedHashMap<>();
    private IXMLAnalyzer(IXMLGrammar grammar) {
        this.grammar = grammar;
        grammar.eliminateLeftRecursion();
        this.lookahead = new IXMLLookahead(grammar);
    }

    /** Analyze a grammar and test its predictive parser against the whole input.
     * @param source iXML grammar
     * @param input sample input (not used to infer grammar properties), or null for no test
     * @return an analysis DOM containing diagnostics or a parser and test result
     */
    public static XmlDocument analyze(String source, String input) {
        return new IXMLAnalyzer(IXMLGrammar.parse(source)).analyze(input);
    }

    /** Analyze a grammar for code generation.
     * @param source iXML grammar
     * @return the analyzer, with the grammar, the reachable rules and the lookahead DFAs
     * @throws IllegalArgumentException if the grammar is not LL(*)
     */
    static IXMLAnalyzer compile(String source) {
        IXMLAnalyzer a = new IXMLAnalyzer(IXMLGrammar.parse(source));
        a.prepare();
        if (!a.diagnostics.isEmpty()) {
            StringBuilder b = new StringBuilder("grammar is not LL(*):");
            for (Element d : a.diagnostics) b.append("\n").append(d.attributeValue("rule")).append(": ").append(d.textContent());
            throw new IllegalArgumentException(b.toString());
        }
        return a;
    }
    /** Compile a grammar for parsing many inputs with the LL(*) parser, see parseInput.
     * @param source iXML grammar
     * @return the compiled parser
     * @throws IllegalArgumentException if the grammar is not LL(*)
     */
    public static IXMLAnalyzer compileParser(String source) { return compile(source); }

    /** Parse an input with a parser from compileParser.
     * @param input the input to parse
     * @return the parse tree
     * @throws IllegalArgumentException if the input is rejected
     */
    public XmlDocument parseInput(String input) {
        Element tree = new Element("tree");
        String error = run(input, tree);
        if (error != null) throw new IllegalArgumentException(error);
        XmlDocument d = new XmlDocument();
        XmlNode root = tree.firstChild();
        if (root instanceof XmlElement e) d.appendRaw(e);
        return d;
    }

    IXMLGrammar grammar() { return grammar; }
    Set<String> reachable() { return reachable; }
    Map<String, IXMLLookahead.Dfa> dfas() { return dfas; }
    Map<String, IXMLLookahead.Failure> dynamic() { return dynamic; }
    IXMLLookahead lookahead() { return lookahead; }

    /** Parse the input with the LL(*) parser of the grammar and return the ixml parse tree.
     * @param source iXML grammar
     * @param input the input to parse
     * @return the parse tree
     * @throws IllegalArgumentException if the grammar is not LL(*) or the input is rejected
     */
    public static XmlDocument parse(String source, String input) {
        IXMLAnalyzer a = new IXMLAnalyzer(IXMLGrammar.parse(source));
        a.prepare();
        if (!a.diagnostics.isEmpty()) {
            StringBuilder b = new StringBuilder("grammar is not LL(*):");
            for (Element d : a.diagnostics) b.append("\n").append(d.attributeValue("rule")).append(": ").append(d.textContent());
            throw new IllegalArgumentException(b.toString());
        }
        Element tree = new Element("tree");
        String error = a.run(input, tree);
        if (error != null) throw new IllegalArgumentException(error);
        XmlDocument d = new XmlDocument();
        XmlNode root = tree.firstChild();
        if (root instanceof XmlElement e) d.appendRaw(e);
        return d;
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
        }
        if (!diagnostics.isEmpty()) return;
        for (String name : lookahead.decisions(reachable)) {
            if (System.getenv("XMQ_LL_TIME") != null) System.err.println("start " + name);
            long t0 = System.nanoTime();
            String result = "ok";
            try {
                dfas.put(name, lookahead.build(name));
            } catch (IXMLLookahead.Failure f) {
                if (f.code.equals("ambiguous")) {
                    diagnose(f.code, name, f.getMessage());
                } else {
                    // No static lookahead DFA (the lookahead is not regular, too large,
                    // or the context was lost): predicted at runtime, see predictDynamic.
                    dynamic.put(name, f);
                }
                result = f.code;
            }
            if (System.getenv("XMQ_LL_TIME") != null) {
                IXMLLookahead.Dfa d = dfas.get(name);
                System.err.printf("%8.1f ms %-12s %-14s states=%d%n", (System.nanoTime() - t0) / 1e6, name, result,
                                  d == null ? -1 : d.states.size());
            }
        }
    }
    private void prepare() {
        findReachable();
        for (String extension : grammar.extensions) diagnose("unsupported-extension", grammar.start, extension);
        computeSets(); checkRules();
    }
    private XmlDocument analyze(String input) {
        Element root = new Element("analysis").attr("model", "LL(*)").attr("lookahead", "unicode-codepoint");
        prepare();
        root.attr("supported", Boolean.toString(diagnostics.isEmpty()));
        root.addContent(diagnostics);
        // Without diagnostics this describes the parser, else just the normalized grammar.
        Element parser = new Element(diagnostics.isEmpty() ? "parser" : "grammar").attr("start", grammar.start).attr("require-eof", "true");
        root.addContent(parser);
        for (String name : reachable) {
            Rule r = grammar.rules.get(name);
            Element e = new Element("rule").attr("name", name).attr("mark", r.mark)
                .attr("nullable", Boolean.toString(r.nullable)).attr("first", ranges(r.first)).attr("follow", ranges(r.follow));
            if (!r.alias.isEmpty()) e.attr("alias", r.alias);
            parser.addContent(e);
            IXMLLookahead.Dfa dfa = dfas.get(name);
            if (dfa != null) e.addContent(decision(dfa));
            IXMLLookahead.Failure dyn = dynamic.get(name);
            if (dyn != null) e.addContent(new Element("decision").attr("dynamic", "true").attr("reason", dyn.code).setText(dyn.getMessage()));
            for (List<Term> a : r.alternatives) {
                Element branch = new Element("alternative").attr("lookahead", ranges(select(r, a))); e.addContent(branch);
                for (Term t : a) {
                    Element operation;
                    if (t.ref != null) operation = new Element("call").attr("rule", t.ref);
                    else if (t.chars != null) operation = new Element("match").attr("set", ranges(t.chars));
                    else if (t.not != null) {
                        operation = new Element("not");
                        for (BitSet b : t.not) operation.addContent(new Element("match").attr("set", ranges(b)));
                    }
                    else operation = new Element("insert").attr("codepoints", codepoints(t.insertion));
                    operation.attr("mark", t.mark);
                    if (!t.alias.isEmpty()) operation.attr("alias", t.alias);
                    branch.addContent(operation);
                }
                if (a.isEmpty()) branch.addContent(new Element("empty"));
            }
        }
        if (input != null) {
            if (diagnostics.isEmpty()) root.addContent(test(input));
            else root.addContent(new Element("test").attr("status", "not-run"));
        }
        return doc(root);
    }
    private static Element decision(IXMLLookahead.Dfa dfa) {
        int k = dfa.maxLookahead();
        Element d = new Element("decision").attr("states", Integer.toString(dfa.states.size()))
            .attr("lookahead", k < 0 ? "unbounded" : Integer.toString(k));
        for (IXMLLookahead.State s : dfa.states) {
            Element st = new Element("state").attr("id", Integer.toString(s.id));
            if (s.predict >= 0) st.attr("predict", Integer.toString(s.predict + 1));
            for (IXMLLookahead.Edge e : s.edges) {
                String range = e.lo() == e.hi() ? hex(e.lo()) : hex(e.lo()) + "-" + hex(e.hi());
                st.addContent(new Element("edge").attr("on", range).attr("to", Integer.toString(e.target().id)));
            }
            d.addContent(st);
        }
        return d;
    }
    /** A rule invocation in the LL(*) interpreter. */
    private static final class Frame {
        Rule rule; List<Term> terms; int idx; int alt;
        char kind;           // '^' element, '-' hidden, '@' attribute
        String name;         // element or attribute name
        Element element;     // for '^'
        StringBuilder text;  // for '@'
        // For a rule rewritten from left recursion: where its content starts.
        Element lrTarget;    // The element that receives the content, null if none.
        int lrChildren, lrTextLen, lrAttrs;
    }
    private int failedAt;
    // Run the LL(*) parser on the input, the tree is added below into.
    // Returns null if accepted, else an error message. An explicit call stack
    // executes the parser without Java stack limits.
    private String run(String input, Element into) {
        int[] cps = input.codePoints().toArray(); int offset = 0;
        Deque<Frame> stack = new ArrayDeque<>();
        Term startTerm = Term.call(grammar.start);
        String err = enter(stack, startTerm, cps, offset, into);
        if (err != null) return err;
        while (!stack.isEmpty()) {
            Frame f = stack.peek();
            if (f.idx == f.terms.size()) {
                stack.pop();
                if (f.kind == '@') nearestElement(stack, into).attr(f.name, f.text.toString());
                continue;
            }
            Term t = f.terms.get(f.idx++);
            if (t.ref != null) {
                err = enter(stack, t, cps, offset, into);
                if (err != null) return err;
            } else if (t.chars != null) {
                int cp = offset == cps.length ? IXMLGrammar.EOF : cps[offset];
                if (!t.chars.get(cp)) { failedAt = offset; return "Expected " + ranges(t.chars) + ", found " + hex(cp); }
                if (!t.mark.equals("-")) text(stack, into, new String(Character.toChars(cp)));
                offset++;
            } else if (t.not != null) {
                if (notMatches(t.not, cps, offset)) {
                    failedAt = offset;
                    return "Negative lookahead matched at " + hex(offset == cps.length ? IXMLGrammar.EOF : cps[offset]);
                }
            } else {
                text(stack, into, t.insertion);
            }
        }
        if (offset != cps.length) { failedAt = offset; return "Expected EOF"; }
        failedAt = offset;
        return null;
    }
    /** Does the input at offset start with the forbidden sequence of a negative lookahead? */
    static boolean notMatches(List<BitSet> not, int[] cps, int offset) {
        if (offset + not.size() > cps.length) return false;
        for (int k = 0; k < not.size(); k++) if (!not.get(k).get(cps[offset + k])) return false;
        return true;
    }
    private String enter(Deque<Frame> stack, Term t, int[] cps, int offset, Element into) {
        Rule r = rule(t);
        int alt = 0;
        if (r.alternatives.size() > 1) {
            IXMLLookahead.Dfa dfa = dfas.get(r.name);
            if (dfa != null) {
                alt = dfa.predict(cps, offset);
            } else {
                // Runtime prediction with the actual return stack.
                IXMLLookahead.Stack ctx = null;
                Iterator<Frame> it = stack.descendingIterator(); // From the start rule frame.
                while (it.hasNext()) {
                    Frame fr = it.next();
                    // A frame at its end (a tail call) returns immediately, like in the ATN
                    // where tail calls push nothing. This keeps loops from growing the context.
                    if (fr.idx == fr.terms.size()) continue;
                    ctx = lookahead.push(lookahead.index.get(fr.rule.name), fr.alt, fr.idx, ctx);
                }
                alt = lookahead.predictAdaptive(lookahead.index.get(r.name), ctx, cps, offset);
            }
            if (alt < 0) {
                failedAt = offset;
                int cp = offset == cps.length ? IXMLGrammar.EOF : cps[offset];
                return "No alternative in " + r.name + " for " + hex(cp);
            }
        }
        Frame f = new Frame();
        f.rule = r; f.terms = r.alternatives.get(alt); f.alt = alt;
        String mark = !t.mark.isEmpty() ? t.mark : !r.mark.isEmpty() ? r.mark : "^";
        f.name = !t.alias.isEmpty() ? t.alias : !r.alias.isEmpty() ? r.alias : r.name;
        boolean inAttribute = false;
        for (Frame p : stack) if (p.kind == '@') { inAttribute = true; break; }
        f.kind = inAttribute ? '-' : mark.charAt(0);
        if (f.kind == '^') {
            f.element = new Element(f.name);
            Element parent = nearestElement(stack, into);
            parent.addContent(f.element);
        } else if (f.kind == '@') {
            f.text = new StringBuilder();
        }
        stack.push(f);
        if (r.lrTail != null && !inAttribute && f.kind != '@') {
            // Remember where the content of this rule starts, for wrapping it
            // in the loop that replaced the left recursion.
            Element target = nearestElement(stack, into);
            f.lrTarget = target;
            f.lrChildren = target.numChildren();
            XmlNode last = target.lastChild();
            f.lrTextLen = last instanceof XmlText ? last.content().length() : -1;
            f.lrAttrs = target.attributes().size();
        }
        if (r.lrWrap != null && r.lrWrap.get(alt) != null) {
            for (Frame o : stack) {
                if (o.rule.name.equals(r.lrOwner)) {
                    if (o.lrTarget != null) wrap(o, r.lrWrap.get(alt));
                    break;
                }
            }
        }
        return null;
    }
    // Move the content produced since the mark in the owner frame into a new
    // element, which is added last. This builds the left nested tree.
    private static void wrap(Frame o, String name) {
        Element target = o.lrTarget;
        Element w = new Element(name);
        List<XmlNode> children = target.children();
        int from = o.lrChildren;
        if (o.lrTextLen >= 0 && from > 0) {
            // Text added after the mark may have been merged into the last text node.
            XmlNode t = children.get(from - 1);
            String c = t.content();
            if (c.length() > o.lrTextLen) {
                t.setContent(c.substring(0, o.lrTextLen));
                w.appendRaw(new XmlText(c.substring(o.lrTextLen)));
            }
        }
        for (int i = from; i < children.size(); i++) {
            XmlNode n = children.get(i);
            n.unlink();
            w.appendRaw(n);
        }
        List<org.libxmq.imp.xml.XmlAttr> attrs = new ArrayList<>(target.attributes()); // A copy, the list is live.
        for (int i = o.lrAttrs; i < attrs.size(); i++) {
            org.libxmq.imp.xml.XmlAttr a = attrs.get(i);
            target.removeAttribute(a);
            w.addAttribute(a);
        }
        target.appendRaw(w);
    }
    private static Element nearestElement(Deque<Frame> stack, Element into) {
        for (Frame p : stack) if (p.kind == '^') return p.element;
        return into;
    }
    private static void text(Deque<Frame> stack, Element into, String s) {
        for (Frame p : stack) {
            if (p.kind == '@') { p.text.append(s); return; }
            if (p.kind == '^') { p.element.addChild(new XmlText(s)); return; }
        }
        into.addChild(new XmlText(s));
    }
    private Element test(String input) {
        Element tree = new Element("tree");
        String err = run(input, tree);
        if (err != null) return rejected(failedAt, err);
        Element accepted = new Element("test").attr("status", "accepted").attr("consumed-codepoints", Integer.toString(failedAt));
        return accepted.addContent(tree);
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
