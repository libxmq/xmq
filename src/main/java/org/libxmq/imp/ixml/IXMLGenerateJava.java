/* libxmq - Copyright (C) 2026 Fredrik Öhrström (spdx: MIT) */
package org.libxmq.imp.ixml;

import java.util.*;
import org.libxmq.imp.ixml.IXMLGrammar.Rule;
import org.libxmq.imp.ixml.IXMLGrammar.Term;

/**
 * Generate a Java parser from an LL(*) ixml grammar.
 *
 * The same top down parser as IXMLGenerateC: every rule is a method, every
 * decision is predicted by its lookahead DFA, there is no backtracking and
 * the ixml parse tree (an org.libxmq.imp.xml.XmlDocument) is built while
 * parsing.
 *
 * Java limits a method (also the static initializer) to 64 KiB of byte
 * code, so the character sets and the DFAs are stored as string constants,
 * decoded when the class is loaded, and the prediction is table driven.
 *
 * The generated class has a main method that parses a file or stdin and
 * prints the tree like xmq does, so that: xmq grammar.ixml input.txt
 * and: java Grammar input.txt print the same.
 */
public final class IXMLGenerateJava
{
    private final IXMLAnalyzer analyzer;
    private final IXMLGrammar grammar;
    private final String pkg, cls, source_name;
    private final Map<String, Integer> ids = new LinkedHashMap<>();
    private final Map<String, Integer> setIds = new LinkedHashMap<>();
    private final List<IXMLLookahead.Ranges> setList = new ArrayList<>();
    private final StringBuilder out = new StringBuilder();

    private IXMLGenerateJava(IXMLAnalyzer analyzer, String pkg, String cls, String source_name)
    {
        this.analyzer = analyzer;
        this.grammar = analyzer.grammar();
        this.pkg = pkg;
        this.cls = cls;
        this.source_name = source_name;
    }

    /**
     * Generate the Java source of a parser for the grammar.
     * @param source The ixml grammar.
     * @param pkg The package of the generated class, or null for the default package.
     * @param cls The name of the generated class.
     * @param source_name The grammar file name, used in comments.
     * @return The Java source code.
     * @throws IllegalArgumentException if the grammar is not LL(*).
     */
    public static String generate(String source, String pkg, String cls, String source_name)
    {
        IXMLAnalyzer a = IXMLAnalyzer.compile(source);
        if (!a.dynamic().isEmpty())
        {
            throw new IllegalArgumentException("the grammar needs runtime prediction (no static lookahead for "
                                               + String.join(", ", a.dynamic().keySet())
                                               + "), which the Java generator does not support yet");
        }
        return new IXMLGenerateJava(a, pkg, cls, source_name).run();
    }

    // ------------------------------------------------------------ helpers

    /** A Java string literal, non ascii and specials as unicode escapes. */
    static String jstring(String s)
    {
        StringBuilder b = new StringBuilder("\"");
        for (int i = 0; i < s.length(); i++)
        {
            char c = s.charAt(i);
            if (c == '"' || c == '\\') b.append('\\').append(c);
            else if (c >= 0x20 && c < 0x7f) b.append(c);
            else b.append(String.format("\\u%04x", (int)c));
        }
        return b.append('"').toString();
    }

    /** String constants are limited to 65535 bytes, long data is split into chunks. */
    static String chunks(String data)
    {
        StringBuilder b = new StringBuilder();
        int chunk = 8000;
        for (int i = 0; i < data.length(); i += chunk)
        {
            if (i > 0) b.append(",\n        ");
            b.append(jstring(data.substring(i, Math.min(data.length(), i + chunk))));
        }
        if (data.isEmpty()) b.append("\"\"");
        return b.toString();
    }

    static String comment(String s)
    {
        return s.replace("*/", "* /");
    }

    int ruleId(String name)
    {
        return ids.get(name);
    }

    int setId(BitSet chars)
    {
        IXMLLookahead.Ranges r = IXMLLookahead.Ranges.of(chars);
        String key = Arrays.toString(r.lo) + Arrays.toString(r.hi);
        Integer id = setIds.get(key);
        if (id == null)
        {
            id = setList.size();
            setIds.put(key, id);
            setList.add(r);
        }
        return id;
    }

    String mark(Term t, Rule r)
    {
        if (!t.mark.isEmpty()) return t.mark;
        if (!r.mark.isEmpty()) return r.mark;
        return "^";
    }

    String elementName(Term t, Rule r)
    {
        if (!t.alias.isEmpty()) return t.alias;
        if (!r.alias.isEmpty()) return r.alias;
        return r.name;
    }

    String inset(int sid, String c)
    {
        return "inset(S" + sid + ", A" + sid + ", " + c + ")";
    }

    String fail(Rule r, String expected)
    {
        return "return fail(" + jstring(IXMLGenerateC.displayName(r)) + ", "
            + (expected == null ? "null" : jstring(expected)) + ");";
    }

    // ------------------------------------------------------------ generation

    String run()
    {
        int k = 0;
        for (String name : analyzer.reachable()) ids.put(name, k++);

        StringBuilder rules = new StringBuilder();
        for (String name : analyzer.reachable()) genRule(rules, name);
        StringBuilder dfas = new StringBuilder();
        for (Map.Entry<String, IXMLLookahead.Dfa> e : analyzer.dfas().entrySet())
        {
            if (ids.containsKey(e.getKey())) genDfa(dfas, e.getKey(), e.getValue());
        }

        header();
        out.append("    // Character sets: sorted inclusive code point ranges, and an ascii bitmap.\n");
        for (int i = 0; i < setList.size(); i++)
        {
            IXMLLookahead.Ranges r = setList.get(i);
            StringBuilder data = new StringBuilder();
            long a0 = 0, a1 = 0;
            for (int j = 0; j < r.lo.length; j++)
            {
                if (j > 0) data.append(',');
                data.append(Integer.toString(r.lo[j], 36)).append(',').append(Integer.toString(r.hi[j], 36));
                for (int c = r.lo[j]; c <= r.hi[j] && c < 128; c++)
                {
                    if (c < 64) a0 |= 1L << c; else a1 |= 1L << (c - 64);
                }
            }
            out.append("    private static final int[] S").append(i).append(" = ints(").append(chunks(data.toString())).append(");\n");
            out.append("    private static final long[] A").append(i).append(" = { ").append(a0).append("L, ").append(a1).append("L };\n");
        }
        out.append('\n');
        out.append(dfas);
        out.append(rules);
        footer();
        return out.toString();
    }

    void header()
    {
        out.append("/* Generated by xmqj generate-parser --java from ").append(comment(source_name)).append(", do not edit.\n");
        out.append("   A top down LL(*) parser that builds the ixml parse tree as an org.libxmq.imp.xml.XmlDocument.\n");
        out.append("   Run: java -cp xmq.jar:. ").append(cls).append(" input.txt   (or reads stdin) */\n\n");
        if (pkg != null && !pkg.isEmpty()) out.append("package ").append(pkg).append(";\n\n");
        out.append(String.format("""
import java.util.ArrayList;
import java.util.List;
import org.libxmq.imp.xml.XmlAttr;
import org.libxmq.imp.xml.XmlDocument;
import org.libxmq.imp.xml.XmlElement;
import org.libxmq.imp.xml.XmlNode;
import org.libxmq.imp.xml.XmlText;

/** A parser generated from %2$s. */
public final class %1$s
{
    /** A failed parse. */
    public static final class ParseError extends Exception
    {
        private static final long serialVersionUID = 1L;
        /** Line and column of the error, starting at 1. */
        public final int line, col;
        /** Offset of the error in code points. */
        public final int offset;
        ParseError(String message, int line, int col, int offset)
        {
            super(message);
            this.line = line;
            this.col = col;
            this.offset = offset;
        }
    }

    private static final int EOF = 0x110000;

    private final int[] in;
    private final int n;
    private int pos;
    private final XmlDocument doc = new XmlDocument();
    private final StringBuilder text = new StringBuilder(); // Pending text, not yet added to textNode.
    private XmlElement textNode;
    private int failPos;
    private String failRule, failExpected;
    // Handed from a left recursive rule to its loop rule: where its content starts.
    private XmlElement lrElem;
    private int lrChildren, lrTextLen, lrAttrs;

    private %1$s(int[] in)
    {
        this.in = in;
        this.n = in.length;
    }

    // Decode comma separated base 36 numbers.
    private static int[] ints(String... parts)
    {
        String s = String.join("", parts);
        if (s.isEmpty()) return new int[0];
        String[] f = s.split(",");
        int[] r = new int[f.length];
        for (int i = 0; i < f.length; i++) r[i] = Integer.parseInt(f[i], 36);
        return r;
    }

    // A dfa: for each state the predicted alternative (-1 if more lookahead is
    // needed) followed by its edges as lo,hi,target triples sorted on lo.
    private static int[][] dfa(String... parts)
    {
        String s = String.join("", parts);
        String[] st = s.split(";", -1);
        int[][] r = new int[st.length][];
        for (int i = 0; i < st.length; i++) r[i] = ints(st[i]);
        return r;
    }

    private static boolean inset(int[] r, long[] ascii, int c)
    {
        if (c < 64) return ((ascii[0] >>> c) & 1) != 0;
        if (c < 128) return ((ascii[1] >>> (c - 64)) & 1) != 0;
        int a = 0, b = r.length / 2;
        while (a < b)
        {
            int m = (a + b) >>> 1;
            if (c < r[2*m]) b = m;
            else if (c > r[2*m+1]) a = m + 1;
            else return true;
        }
        return false;
    }

    private int peek(int i)
    {
        return i < n ? in[i] : EOF;
    }

    // Walk the dfa from the current position, return the predicted alternative or -1.
    private int predict(int[][] d)
    {
        int i = pos, s = 0;
        for (;;)
        {
            int[] st = d[s];
            if (st[0] >= 0) return st[0];
            int c = peek(i), a = 0, b = (st.length - 1) / 3, next = -1;
            while (a < b)
            {
                int m = (a + b) >>> 1;
                if (c < st[1+3*m]) b = m;
                else if (c > st[2+3*m]) a = m + 1;
                else { next = st[3+3*m]; break; }
            }
            if (next < 0) return -1;
            s = next;
            i++;
        }
    }

    private boolean fail(String rule, String expected)
    {
        failPos = pos;
        failRule = rule;
        failExpected = expected;
        return false;
    }

    // Add the pending text to its node.
    private void flush()
    {
        if (text.length() > 0 && textNode != null) textNode.addChild(new XmlText(text.toString()));
        text.setLength(0);
        textNode = null;
    }

    // Text goes into the attribute value being collected, or to the element.
    private void text(XmlElement elem, StringBuilder attr, String s)
    {
        if (attr != null) { attr.append(s); return; }
        if (elem == null) return;
        if (elem != textNode) { flush(); textNode = elem; }
        text.append(s);
    }

    private void chr(XmlElement elem, StringBuilder attr, int c)
    {
        if (attr != null) { attr.appendCodePoint(c); return; }
        if (elem == null) return;
        if (elem != textNode) { flush(); textNode = elem; }
        text.appendCodePoint(c);
    }

    private XmlElement element(XmlElement parent, String name)
    {
        flush();
        XmlElement e = new XmlElement(name);
        if (parent != null) parent.addChild(e);
        else if (doc.rootElement() == null) doc.setRootElement(e);
        else doc.appendRaw(e);
        return e;
    }

    // Move the content added to the element since the mark into a new element,
    // added last. This builds the left nested tree of a left recursive rule.
    private static void wrap(XmlElement target, int from, int textLen, int attrsFrom, String name)
    {
        XmlElement w = new XmlElement(name);
        List<XmlNode> children = target.children();
        if (textLen >= 0 && from > 0)
        {
            // Text added after the mark may have been merged into the marked text node.
            XmlNode t = children.get(from - 1);
            String c = t.content();
            if (c.length() > textLen)
            {
                t.setContent(c.substring(0, textLen));
                w.appendRaw(new XmlText(c.substring(textLen)));
            }
        }
        for (int i = from; i < children.size(); i++)
        {
            XmlNode c = children.get(i);
            c.unlink();
            w.appendRaw(c);
        }
        List<XmlAttr> attrs = new ArrayList<>(target.attributes());
        for (int i = attrsFrom; i < attrs.size(); i++)
        {
            target.removeAttribute(attrs.get(i));
            w.addAttribute(attrs.get(i));
        }
        target.appendRaw(w);
    }

""", cls, comment(source_name)));
    }

    void genDfa(StringBuilder b, String name, IXMLLookahead.Dfa dfa)
    {
        StringBuilder data = new StringBuilder();
        for (IXMLLookahead.State s : dfa.states)
        {
            if (s.id > 0) data.append(';');
            data.append(s.predict >= 0 ? Integer.toString(s.predict, 36) : "-1");
            if (s.predict >= 0) continue;
            List<IXMLLookahead.Edge> edges = new ArrayList<>(s.edges);
            edges.sort(Comparator.comparingInt(IXMLLookahead.Edge::lo));
            for (IXMLLookahead.Edge e : edges)
            {
                data.append(',').append(Integer.toString(e.lo(), 36)).append(',').append(Integer.toString(e.hi(), 36))
                    .append(',').append(Integer.toString(e.target().id, 36));
            }
        }
        b.append("    // The lookahead dfa of ").append(comment(name)).append(".\n");
        b.append("    private static final int[][] D").append(ruleId(name)).append(" = dfa(").append(chunks(data.toString())).append(");\n\n");
    }

    void genRule(StringBuilder b, String name)
    {
        Rule r = grammar.rules.get(name);
        int id = ruleId(name);
        b.append("    // ").append(comment(name)).append('\n');
        b.append("    private boolean r").append(id).append("(XmlElement elem, StringBuilder attr)\n    {\n");
        if (r.lrTail != null)
        {
            // Mark where the content of this left recursive rule starts.
            b.append("        XmlElement lrE = attr != null ? null : elem;\n");
            b.append("        int lrC = 0, lrT = -1, lrA = 0;\n");
            b.append("        if (lrE != null)\n        {\n");
            b.append("            flush();\n");
            b.append("            lrC = lrE.numChildren();\n");
            b.append("            XmlNode last = lrE.lastChild();\n");
            b.append("            lrT = last instanceof XmlText ? last.content().length() : -1;\n");
            b.append("            lrA = lrE.attributes().size();\n");
            b.append("        }\n");
        }
        if (r.lrOwner != null)
        {
            b.append("        XmlElement lrE = attr != null ? null : lrElem; // The content mark of ").append(comment(r.lrOwner)).append('\n');
            b.append("        int lrC = lrChildren, lrT = lrTextLen, lrA = lrAttrs;\n");
        }
        StringBuilder body = new StringBuilder();
        boolean loops;
        if (r.lrOwner == null && genCharLoop(body, r))
        {
            loops = false;
        }
        else if (r.alternatives.size() == 1)
        {
            loops = genAlt(body, r, r.alternatives.get(0), "            ");
        }
        else
        {
            loops = false;
            body.append("            switch (predict(D").append(id).append("))\n            {\n");
            for (int a = 0; a < r.alternatives.size(); a++)
            {
                body.append("            case ").append(a).append(":\n            {\n");
                if (r.lrWrap != null && r.lrWrap.get(a) != null)
                {
                    // Another turn of the loop, the content so far becomes the inner element.
                    body.append("                if (lrE != null) { flush(); wrap(lrE, lrC, lrT, lrA, ")
                        .append(jstring(r.lrWrap.get(a))).append("); }\n");
                }
                loops |= genAlt(body, r, r.alternatives.get(a), "                ");
                body.append("            }\n");
            }
            body.append("            default:\n                ").append(fail(r, null)).append("\n            }\n");
        }
        if (loops)
        {
            b.append("        start: for (;;)\n        {\n").append(body).append("        }\n");
        }
        else
        {
            // Without a loop, remove one level of indentation.
            for (String line : body.toString().split("\n", -1))
            {
                b.append(line.startsWith("    ") ? line.substring(4) : line).append(line.isEmpty() ? "" : "\n");
            }
        }
        b.append("    }\n\n");
    }

    /** The greedy single character loop, see IXMLGenerateC.genCharLoop. */
    boolean genCharLoop(StringBuilder b, Rule r)
    {
        if (r.alternatives.size() != 2) return false;
        int loop = -1;
        for (int a = 0; a < 2; a++)
        {
            List<Term> t = r.alternatives.get(a);
            List<Term> o = r.alternatives.get(1 - a);
            if (t.size() == 2 && t.get(0).chars != null && t.get(1).ref != null && t.get(1).ref.equals(r.name)
                && mark(t.get(1), r).equals("-") && o.isEmpty()) loop = a;
        }
        if (loop < 0) return false;
        IXMLLookahead.Dfa dfa = analyzer.dfas().get(r.name);
        if (dfa == null || dfa.maxLookahead() != 1) return false;
        BitSet take = new BitSet(), stop = new BitSet();
        for (IXMLLookahead.Edge e : dfa.states.get(0).edges)
        {
            if (e.target().predict == loop) take.set(e.lo(), e.hi() + 1);
            else stop.set(e.lo(), e.hi() + 1);
        }
        Term m = r.alternatives.get(loop).get(0);
        BitSet extra = (BitSet)take.clone();
        extra.andNot(m.chars);
        int takeId = setId(take);
        b.append("            // Greedy loop over a character table.\n");
        b.append("            for (;;)\n            {\n");
        b.append("                int c = peek(pos);\n");
        b.append("                if (").append(inset(takeId, "c")).append(")\n                {\n");
        if (!extra.isEmpty())
        {
            int sid = setId(m.chars);
            b.append("                    if (!").append(inset(sid, "c")).append(") ")
             .append(fail(r, IXMLGenerateC.describe(setList.get(sid)))).append('\n');
        }
        if (!m.mark.equals("-")) b.append("                    chr(elem, attr, c);\n");
        b.append("                    pos++;\n                    continue;\n                }\n");
        BitSet all = (BitSet)take.clone();
        all.or(stop);
        if (!IXMLGenerateC.coversAll(all))
        {
            int stopId = setId(stop);
            b.append("                if (!").append(inset(stopId, "c")).append(") ").append(fail(r, null)).append('\n');
        }
        b.append("                return true;\n            }\n");
        return true;
    }

    /** Generate the code for one alternative. Returns true if it loops back to start. */
    boolean genAlt(StringBuilder b, Rule r, List<Term> terms, String ind)
    {
        for (int i = 0; i < terms.size(); i++)
        {
            Term t = terms.get(i);
            boolean last = i == terms.size() - 1;
            if (t.chars != null)
            {
                int sid = setId(t.chars);
                b.append(ind).append("if (!").append(inset(sid, "peek(pos)")).append(") ")
                 .append(fail(r, IXMLGenerateC.describe(setList.get(sid)))).append('\n');
                if (!t.mark.equals("-")) b.append(ind).append("chr(elem, attr, in[pos]);\n");
                b.append(ind).append("pos++;\n");
            }
            else if (t.ref != null)
            {
                Rule callee = grammar.rules.get(t.ref);
                String m = mark(t, callee);
                String fn = "r" + ruleId(t.ref);
                if (m.equals("-"))
                {
                    if (last && callee == r)
                    {
                        // A hidden tail call to this rule loops instead of recursing.
                        b.append(ind).append("continue start; // ").append(comment(t.ref)).append('\n');
                        return true;
                    }
                    if (callee.lrOwner != null)
                    {
                        b.append(ind).append("lrElem = lrE; lrChildren = lrC; lrTextLen = lrT; lrAttrs = lrA;\n");
                    }
                    b.append(ind).append("if (!").append(fn).append("(elem, attr)) return false; // ").append(comment(t.ref)).append('\n');
                }
                else if (m.equals("@"))
                {
                    String an = jstring(elementName(t, callee));
                    b.append(ind).append("if (attr != null) { if (!").append(fn).append("(elem, attr)) return false; }\n");
                    b.append(ind).append("else\n").append(ind).append("{\n");
                    b.append(ind).append("    StringBuilder v = new StringBuilder();\n");
                    b.append(ind).append("    if (!").append(fn).append("(elem, v)) return false;\n");
                    b.append(ind).append("    if (elem != null) elem.setAttribute(").append(an).append(", v.toString());\n");
                    b.append(ind).append("}\n");
                }
                else
                {
                    String en = jstring(elementName(t, callee));
                    b.append(ind).append("if (attr != null) { if (!").append(fn).append("(elem, attr)) return false; }\n");
                    b.append(ind).append("else if (!").append(fn).append("(element(elem, ").append(en).append("), null)) return false;\n");
                }
            }
            else if (t.not != null)
            {
                // A negative lookahead, fail if the input starts with the forbidden sequence.
                int len = t.not.size();
                StringBuilder cond = new StringBuilder("pos + " + len + " <= n");
                StringBuilder what = new StringBuilder("not");
                for (int k = 0; k < len; k++)
                {
                    int sid = setId(t.not.get(k));
                    cond.append(" && ").append(inset(sid, "in[pos + " + k + "]"));
                    what.append(' ').append(IXMLGenerateC.describe(setList.get(sid)));
                }
                b.append(ind).append("if (").append(len == 0 ? "true" : cond).append(") ").append(fail(r, what.toString())).append('\n');
            }
            else
            {
                b.append(ind).append("text(elem, attr, ").append(jstring(t.insertion)).append(");\n");
            }
        }
        b.append(ind).append("return true;\n");
        return false;
    }

    void footer()
    {
        Rule r = grammar.rules.get(grammar.start);
        String m = r.mark.isEmpty() ? "^" : r.mark;
        String start = "r" + ruleId(grammar.start);
        String call = m.equals("^")
            ? "p." + start + "(p.element(null, " + jstring(r.alias.isEmpty() ? r.name : r.alias) + "), null)"
            : "p." + start + "(null, null)";
        out.append(String.format("""
    /**
     * Parse the input and return the ixml parse tree.
     * @param input The input.
     * @return The parse tree.
     * @throws ParseError If the input does not match the grammar.
     */
    public static XmlDocument parse(String input) throws ParseError
    {
        %1$s p = new %1$s(input.codePoints().toArray());
        boolean ok = %2$s;
        if (ok && p.pos != p.n) ok = p.fail(null, "EOF");
        if (ok)
        {
            p.flush();
            return p.doc;
        }
        int line = 1, col = 1;
        for (int i = 0; i < p.failPos && i < p.n; i++)
        {
            if (p.in[i] == '\\n') { line++; col = 1; } else col++;
        }
        int c = p.peek(p.failPos);
        String found = c == EOF ? "end of input"
            : (c > 0x20 && c < 0x7f && c != 0x27) ? "'" + (char)c + "'"
            : c == ' ' ? "space" : "#" + Integer.toHexString(c);
        String rule = p.failRule != null ? p.failRule : "input";
        String message = p.failExpected != null
            ? "expected " + p.failExpected + " in " + rule + ", found " + found
            : "no alternative of " + rule + " matches, found " + found;
        throw new ParseError(message, line, col, p.failPos);
    }

    /**
     * Parse a file (or stdin) and print the tree like xmq.
     * @param args The file name, or nothing for stdin.
     * @throws Exception If the file cannot be read.
     */
    public static void main(String[] args) throws Exception
    {
        String file = args.length > 0 ? args[0] : null;
        byte[] data = file != null ? java.nio.file.Files.readAllBytes(java.nio.file.Path.of(file)) : System.in.readAllBytes();
        String input = new String(data, java.nio.charset.StandardCharsets.UTF_8);
        XmlDocument[] result = new XmlDocument[1];
        ParseError[] error = new ParseError[1];
        // Deeply nested input recurses deeply, use a large stack.
        Thread t = new Thread(null, () -> {
            try { result[0] = parse(input); }
            catch (ParseError e) { error[0] = e; }
        }, "parse", 1L << 30);
        t.start();
        t.join();
        if (error[0] != null)
        {
            System.err.println((file != null ? file : "stdin") + ":" + error[0].line + ":" + error[0].col + ": error: " + error[0].getMessage());
            System.exit(1);
        }
        java.io.PrintStream o = new java.io.PrintStream(new java.io.FileOutputStream(java.io.FileDescriptor.out), true, java.nio.charset.StandardCharsets.UTF_8);
        o.println(org.libxmq.imp.XMQPrinter.toXmq(result[0]));
    }
}
""", cls, call));
    }
}
