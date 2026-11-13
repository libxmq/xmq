/* libxmq - Copyright (C) 2026 Fredrik Öhrström (spdx: MIT) */
package org.libxmq.imp.ixml;

import java.nio.charset.StandardCharsets;
import java.util.*;
import org.libxmq.imp.ixml.IXMLGrammar.Rule;
import org.libxmq.imp.ixml.IXMLGrammar.Term;

/**
 * Generate a C parser from an LL(*) ixml grammar.
 *
 * The generated parser is a top down recursive descent parser. Every rule is
 * a C function, every decision (a rule with more than one alternative) is
 * predicted by its lookahead DFA from IXMLLookahead. Since the prediction is
 * exact there is no backtracking, so the ixml parse tree is built while
 * parsing, through the macros XMQ_ADD_ROOT_ELEMENT, XMQ_ADD_ELEMENT,
 * XMQ_SET_ATTRIBUTE and XMQ_ADD_TEXT which by default use libxmq.
 *
 * The generated file also contains a main function (unless compiled with
 * -DXMQ_PARSER_NO_MAIN) that parses a file or stdin and prints the tree like
 * xmq does, so that: xmq grammar.ixml input.txt
 * and: ./grammar_parser input.txt
 * print the same.
 */
public final class IXMLGenerateC
{
    private final IXMLAnalyzer analyzer;
    private final IXMLGrammar grammar;
    private final String prefix;
    private final String source_name;
    private final Map<String, Integer> ids = new LinkedHashMap<>();
    private final Map<String, Integer> setIds = new LinkedHashMap<>(); // ranges text -> id
    private final List<IXMLLookahead.Ranges> setList = new ArrayList<>();
    private final StringBuilder out = new StringBuilder();

    private IXMLGenerateC(IXMLAnalyzer analyzer, String prefix, String source_name)
    {
        this.analyzer = analyzer;
        this.grammar = analyzer.grammar();
        this.prefix = prefix;
        this.source_name = source_name;
    }

    /**
     * Generate the C source of a parser for the grammar.
     * @param source The ixml grammar.
     * @param prefix The prefix of the generated C names, eg xmqcli gives xmqcli_parse.
     * @param source_name The grammar file name, used in comments.
     * @return The C source code.
     * @throws IllegalArgumentException if the grammar is not LL(*).
     */
    public static String generate(String source, String prefix, String source_name)
    {
        IXMLAnalyzer a = IXMLAnalyzer.compile(source);
        return new IXMLGenerateC(a, prefix, source_name).run();
    }

    // ------------------------------------------------------------ helpers

    /** A C string literal for the utf8 bytes of s, using octal escapes for anything special. */
    static String cstring(String s)
    {
        StringBuilder b = new StringBuilder("\"");
        for (byte x : s.getBytes(StandardCharsets.UTF_8))
        {
            int c = x & 0xff;
            if (c >= 0x20 && c < 0x7f && c != '"' && c != '\\' && c != '?') b.append((char)c);
            else b.append(String.format("\\%03o", c));
        }
        return b.append('"').toString();
    }

    static int utf8len(String s)
    {
        return s.getBytes(StandardCharsets.UTF_8).length;
    }

    /** A C comment safe text. */
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

    static String hex(int cp)
    {
        return cp == IXMLGrammar.EOF ? "EOF" : "#" + Integer.toHexString(cp);
    }

    static String describe(IXMLLookahead.Ranges r)
    {
        StringBuilder b = new StringBuilder();
        for (int k = 0; k < r.lo.length && k < 8; k++)
        {
            if (b.length() > 0) b.append(' ');
            b.append(r.lo[k] == r.hi[k] ? hex(r.lo[k]) : hex(r.lo[k]) + "-" + hex(r.hi[k]));
        }
        if (r.lo.length > 8) b.append(" ...");
        return b.toString();
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

    // ------------------------------------------------------------ generation

    String run()
    {
        int k = 0;
        for (String name : analyzer.reachable()) ids.put(name, k++);

        StringBuilder rules = new StringBuilder();
        for (String name : analyzer.reachable()) genRule(rules, name);
        StringBuilder predicts = new StringBuilder();
        for (Map.Entry<String, IXMLLookahead.Dfa> e : analyzer.dfas().entrySet())
        {
            if (ids.containsKey(e.getKey())) genPredict(predicts, e.getKey(), e.getValue());
        }

        header();
        // Character sets.
        out.append("// Character sets, sorted inclusive code point ranges.\n");
        for (int i = 0; i < setList.size(); i++)
        {
            IXMLLookahead.Ranges r = setList.get(i);
            out.append("static const int ").append(prefix).append("_set_").append(i).append("[] = {");
            for (int j = 0; j < r.lo.length; j++)
            {
                if (j % 8 == 0) out.append("\n    ");
                out.append(r.lo[j]).append(',').append(r.hi[j]).append(',');
            }
            out.append("\n};\n");
            // A bitmap of the ascii members, for the fast path in _inset.
            int[] bits = new int[16];
            for (int j = 0; j < r.lo.length; j++)
                for (int c = r.lo[j]; c <= r.hi[j] && c < 128; c++) bits[c >> 3] |= 1 << (c & 7);
            out.append("static const unsigned char ").append(prefix).append("_ascii_").append(i).append("[16] = {");
            for (int j = 0; j < 16; j++) out.append(j == 0 ? "" : ",").append(bits[j]);
            out.append("};\n");
        }
        out.append('\n');
        // Prototypes.
        for (String name : analyzer.reachable())
        {
            out.append("static bool ").append(prefix).append("_r").append(ruleId(name))
               .append("(").append(prefix).append("_Parser *p, void *elem, ").append(prefix)
               .append("_Buf *attr); // ").append(comment(name)).append('\n');
        }
        out.append('\n');
        out.append(predicts);
        out.append(rules);
        footer();
        return out.toString();
    }

    void header()
    {
        String P = prefix;
        out.append("/* Generated by xmqj generate-parser --c from ").append(comment(source_name)).append(", do not edit.\n");
        out.append("   A top down LL(*) parser that builds the ixml parse tree using libxmq.\n");
        out.append("   Compile: cc -o ").append(P).append(" ").append(P).append(".c -lxmq -lxslt -lxml2 -lz\n");
        out.append("   Run:     ./").append(P).append(" input.txt   (or reads stdin)\n");
        out.append("   Define XMQ_PARSER_NO_MAIN to leave out the main function. */\n\n");
        out.append("""
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "xmq.h"

// The tree is built with these macros, define them before including this
// file to build something else than a libxmq document. ctx is the void *ctx
// given to the parse function, a node is a void*.
#ifndef XMQ_ADD_ROOT_ELEMENT
#define XMQ_ADD_ROOT_ELEMENT(ctx, name) ((void*)xmqAddRootElement((XMQDoc*)(ctx), (name), NS_NONE).node)
#endif
#ifndef XMQ_ADD_ELEMENT
#define XMQ_ADD_ELEMENT(ctx, parent, name) ((void*)xmqAddElement((XMQDoc*)(ctx), (XMQNode*)(parent), (name), NS_NONE).node)
#endif
#ifndef XMQ_SET_ATTRIBUTE
#define XMQ_SET_ATTRIBUTE(ctx, node, name, value, len) xmqSetAttribute((XMQDoc*)(ctx), (XMQNode*)(node), (name), (value), NS_NONE)
#endif
#ifndef XMQ_ADD_TEXT
#define XMQ_ADD_TEXT(ctx, node, text, len) xmqAddText((XMQDoc*)(ctx), (XMQNode*)(node), (text), (len))
#endif
// Left recursive rules are parsed as loops, the left nested tree is built by
// marking where the content of the rule starts and wrapping the content since
// the mark into a new element at each turn of the loop.
#ifndef XMQ_CONTENT_MARK
#define XMQ_MARK_TYPE XMQContentMark
#define XMQ_CONTENT_MARK(ctx, node) xmqContentMark((XMQNode*)(node))
#define XMQ_WRAP_CONTENT(ctx, node, mark, name) xmqWrapContent((XMQDoc*)(ctx), (XMQNode*)(node), (mark), (name))
#endif

""".replace("    ", "    "));
        out.append(String.format("""
#define %1$s_EOF 0x110000

typedef struct %1$s_Buf { char *data; size_t len, cap; } %1$s_Buf;

typedef struct %1$s_Parser
{
    const int *in;          // The input as code points.
    size_t n;               // Number of code points.
    size_t pos;             // Current position.
    void *ctx;              // Given to the tree macros.
    %1$s_Buf text;           // Pending text, not yet added to text_node.
    void *text_node;
    size_t fail_pos;        // Where the parse failed.
    const char *fail_rule;  // The rule that failed.
    const char *fail_expected;
    void *lr_elem;          // Handed from a left recursive rule to its loop rule.
    XMQ_MARK_TYPE lr_mark;
} %1$s_Parser;

/** The result of a failed parse. */
typedef struct %1$s_Error
{
    size_t line, col, offset; // Position of the error, line and col start at 1.
    char message[256];
} %1$s_Error;

static void %1$s_buf_add(%1$s_Buf *b, const char *s, size_t len)
{
    if (b->len + len + 1 > b->cap)
    {
        size_t cap = b->cap ? b->cap * 2 : 64;
        while (cap < b->len + len + 1) cap *= 2;
        b->data = (char*)realloc(b->data, cap);
        b->cap = cap;
    }
    memcpy(b->data + b->len, s, len);
    b->len += len;
    b->data[b->len] = 0;
}

static size_t %1$s_utf8(int c, char *buf)
{
    if (c < 0x80) { buf[0] = (char)c; return 1; }
    if (c < 0x800) { buf[0] = (char)(0xc0 | (c >> 6)); buf[1] = (char)(0x80 | (c & 0x3f)); return 2; }
    if (c < 0x10000) { buf[0] = (char)(0xe0 | (c >> 12)); buf[1] = (char)(0x80 | ((c >> 6) & 0x3f)); buf[2] = (char)(0x80 | (c & 0x3f)); return 3; }
    buf[0] = (char)(0xf0 | (c >> 18)); buf[1] = (char)(0x80 | ((c >> 12) & 0x3f));
    buf[2] = (char)(0x80 | ((c >> 6) & 0x3f)); buf[3] = (char)(0x80 | (c & 0x3f)); return 4;
}

// Add the pending text to its node.
static void %1$s_flush(%1$s_Parser *p)
{
    if (p->text.len > 0 && p->text_node) XMQ_ADD_TEXT(p->ctx, p->text_node, p->text.data, p->text.len);
    p->text.len = 0;
    p->text_node = NULL;
}

// Text goes into the attribute value being collected, or to the element.
static void %1$s_text(%1$s_Parser *p, void *elem, %1$s_Buf *attr, const char *s, size_t len)
{
    if (attr) { %1$s_buf_add(attr, s, len); return; }
    if (!elem) return;
    if (elem != p->text_node) { %1$s_flush(p); p->text_node = elem; }
    %1$s_buf_add(&p->text, s, len);
}

static void %1$s_char(%1$s_Parser *p, void *elem, %1$s_Buf *attr, int c)
{
    char buf[4];
    size_t len = %1$s_utf8(c, buf);
    %1$s_text(p, elem, attr, buf, len);
}

static int %1$s_peek(%1$s_Parser *p, size_t i)
{
    return i < p->n ? p->in[i] : %1$s_EOF;
}

// Is c inside the sorted ranges lo,hi,lo,hi... of n pairs?
static bool %1$s_in(const int *r, size_t n, int c)
{
    size_t a = 0, b = n;
    while (a < b)
    {
        size_t m = (a + b) / 2;
        if (c < r[2*m]) b = m;
        else if (c > r[2*m+1]) a = m + 1;
        else return true;
    }
    return false;
}

// Is c in the character set? Ascii is looked up in a bitmap.
static inline bool %1$s_inset(const int *r, size_t n, const unsigned char *ascii, int c)
{
    if (c < 128) return (ascii[c >> 3] >> (c & 7)) & 1;
    return %1$s_in(r, n, c);
}

// Find the next DFA state for c in the sorted edges lo,hi,target... of n edges, -1 if none.
static int %1$s_step(const int *e, size_t n, int c)
{
    size_t a = 0, b = n;
    while (a < b)
    {
        size_t m = (a + b) / 2;
        if (c < e[3*m]) b = m;
        else if (c > e[3*m+1]) a = m + 1;
        else return e[3*m+2];
    }
    return -1;
}

static bool %1$s_fail(%1$s_Parser *p, const char *rule, const char *expected)
{
    p->fail_pos = p->pos;
    p->fail_rule = rule;
    p->fail_expected = expected;
    return false;
}

""", P));
    }

    void genPredict(StringBuilder b, String name, IXMLLookahead.Dfa dfa)
    {
        String P = prefix;
        int id = ruleId(name);
        // Edge tables.
        for (IXMLLookahead.State s : dfa.states)
        {
            if (s.predict >= 0 || s.edges.isEmpty()) continue;
            List<IXMLLookahead.Edge> edges = new ArrayList<>(s.edges);
            edges.sort(Comparator.comparingInt(IXMLLookahead.Edge::lo));
            b.append("static const int ").append(P).append("_d").append(id).append('_').append(s.id).append("[] = {");
            int j = 0;
            for (IXMLLookahead.Edge e : edges)
            {
                if (j++ % 6 == 0) b.append("\n    ");
                b.append(e.lo()).append(',').append(e.hi()).append(',').append(e.target().id).append(',');
            }
            b.append("\n};\n");
        }
        b.append("// Predict the alternative of ").append(comment(name)).append(", 0 based, -1 if none matches.\n");
        b.append("static int ").append(P).append("_predict").append(id).append("(").append(P).append("_Parser *p)\n{\n");
        b.append("    size_t i = p->pos;\n    int s = 0;\n    for (;;)\n    {\n        switch (s)\n        {\n");
        for (IXMLLookahead.State s : dfa.states)
        {
            b.append("        case ").append(s.id).append(": ");
            if (s.predict >= 0)
            {
                b.append("return ").append(s.predict).append(";\n");
            }
            else if (s.edges.isEmpty())
            {
                b.append("return -1;\n");
            }
            else
            {
                b.append("s = ").append(P).append("_step(").append(P).append("_d").append(id).append('_').append(s.id)
                 .append(", ").append(s.edges.size()).append(", ").append(P).append("_peek(p, i)); break;\n");
            }
        }
        b.append("        default: return -1;\n        }\n        if (s < 0) return -1;\n        i++;\n    }\n}\n\n");
    }

    void genRule(StringBuilder b, String name)
    {
        String P = prefix;
        Rule r = grammar.rules.get(name);
        int id = ruleId(name);
        b.append("// ").append(comment(name)).append('\n');
        b.append("static bool ").append(P).append("_r").append(id).append("(").append(P)
         .append("_Parser *p, void *elem, ").append(P).append("_Buf *attr)\n{\n");
        boolean usesStart = false;
        StringBuilder body = new StringBuilder();
        if (r.lrTail != null)
        {
            // Mark where the content of this left recursive rule starts.
            b.append("    void *lr_elem = attr ? NULL : elem;\n");
            b.append("    XMQ_MARK_TYPE lr_mark = {0};\n");
            b.append("    if (lr_elem) { ").append(P).append("_flush(p); lr_mark = XMQ_CONTENT_MARK(p->ctx, lr_elem); }\n");
        }
        if (r.lrOwner != null)
        {
            b.append("    void *lr_elem = p->lr_elem; // The content mark of ").append(comment(r.lrOwner)).append('\n');
            b.append("    XMQ_MARK_TYPE lr_mark = p->lr_mark;\n");
            b.append("    if (attr) lr_elem = NULL;\n");
        }
        if (r.lrOwner == null && genCharLoop(body, r))
        {
            // Done.
        }
        else if (r.alternatives.size() == 1)
        {
            usesStart = genAlt(body, r, r.alternatives.get(0), "    ");
        }
        else
        {
            body.append("    switch (").append(P).append("_predict").append(id).append("(p))\n    {\n");
            for (int a = 0; a < r.alternatives.size(); a++)
            {
                body.append("    case ").append(a).append(":\n    {\n");
                if (r.lrWrap != null && r.lrWrap.get(a) != null)
                {
                    // Another turn of the loop, the content so far becomes the inner element.
                    body.append("        if (lr_elem) { ").append(P).append("_flush(p); XMQ_WRAP_CONTENT(p->ctx, lr_elem, lr_mark, ")
                        .append(cstring(r.lrWrap.get(a))).append("); }\n");
                }
                usesStart |= genAlt(body, r, r.alternatives.get(a), "        ");
                body.append("    }\n");
            }
            body.append("    default:\n        return ").append(P).append("_fail(p, ").append(cstring(name))
                .append(", NULL);\n    }\n");
        }
        if (usesStart) b.append("start:\n");
        b.append(body);
        b.append("}\n\n");
    }

    /** A call of _inset for the character set with id sid. */
    String inset(int sid, String c)
    {
        return prefix + "_inset(" + prefix + "_set_" + sid + ", " + setList.get(sid).lo.length + ", "
            + prefix + "_ascii_" + sid + ", " + c + ")";
    }

    static boolean coversAll(BitSet b)
    {
        return b.nextClearBit(0) > IXMLGrammar.EOF;
    }

    /**
     * A greedy single character loop, the lowering of [S]* or the tail of [S]+:
     * a hidden rule R = [S], R | () whose decision needs one symbol of
     * lookahead. It becomes a while loop over a character table. With a
     * following ![S] the loop is greedy and the lookahead is one symbol.
     * Returns false if the rule does not have this shape.
     */
    boolean genCharLoop(StringBuilder b, Rule r)
    {
        String P = prefix;
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
        b.append("    // Greedy loop over a character table.\n");
        b.append("    for (;;)\n    {\n");
        b.append("        int c = ").append(P).append("_peek(p, p->pos);\n");
        b.append("        if (").append(inset(takeId, "c")).append(")\n        {\n");
        if (!extra.isEmpty())
        {
            // The prediction also takes symbols that the match rejects (a syntax error).
            int sid = setId(m.chars);
            b.append("            if (!").append(inset(sid, "c")).append(") return ").append(P).append("_fail(p, ")
             .append(cstring(r.name)).append(", ").append(cstring(describe(setList.get(sid)))).append(");\n");
        }
        if (!m.mark.equals("-")) b.append("            ").append(P).append("_char(p, elem, attr, c);\n");
        b.append("            p->pos++;\n            continue;\n        }\n");
        BitSet all = (BitSet)take.clone();
        all.or(stop);
        if (!coversAll(all))
        {
            int stopId = setId(stop);
            b.append("        if (!").append(inset(stopId, "c")).append(") return ").append(P).append("_fail(p, ")
             .append(cstring(r.name)).append(", NULL);\n");
        }
        b.append("        return true;\n    }\n");
        return true;
    }

    /** Generate the code for one alternative. Returns true if it loops back to start. */
    boolean genAlt(StringBuilder b, Rule r, List<Term> terms, String ind)
    {
        String P = prefix;
        boolean loops = false;
        for (int i = 0; i < terms.size(); i++)
        {
            Term t = terms.get(i);
            boolean last = i == terms.size() - 1;
            if (t.chars != null)
            {
                int sid = setId(t.chars);
                IXMLLookahead.Ranges rs = setList.get(sid);
                b.append(ind).append("if (!").append(inset(sid, P + "_peek(p, p->pos)")).append(") return ").append(P)
                 .append("_fail(p, ").append(cstring(r.name)).append(", ").append(cstring(describe(rs))).append(");\n");
                if (!t.mark.equals("-"))
                {
                    b.append(ind).append(P).append("_char(p, elem, attr, p->in[p->pos]);\n");
                }
                b.append(ind).append("p->pos++;\n");
            }
            else if (t.ref != null)
            {
                Rule callee = grammar.rules.get(t.ref);
                String m = mark(t, callee);
                String fn = P + "_r" + ruleId(t.ref);
                if (m.equals("-"))
                {
                    if (last && callee == r)
                    {
                        // A hidden tail call to this rule loops instead of recursing.
                        b.append(ind).append("goto start; // ").append(comment(t.ref)).append('\n');
                        loops = true;
                        return loops;
                    }
                    if (callee.lrOwner != null)
                    {
                        b.append(ind).append("p->lr_elem = lr_elem;\n");
                        b.append(ind).append("p->lr_mark = lr_mark;\n");
                    }
                    b.append(ind).append("if (!").append(fn).append("(p, elem, attr)) return false; // ")
                     .append(comment(t.ref)).append('\n');
                }
                else if (m.equals("@"))
                {
                    String an = cstring(elementName(t, callee));
                    b.append(ind).append("if (attr) { if (!").append(fn).append("(p, elem, attr)) return false; }\n");
                    b.append(ind).append("else\n").append(ind).append("{\n");
                    b.append(ind).append("    ").append(P).append("_Buf v = {0};\n");
                    b.append(ind).append("    ").append(P).append("_buf_add(&v, \"\", 0);\n");
                    b.append(ind).append("    if (!").append(fn).append("(p, elem, &v)) { free(v.data); return false; }\n");
                    b.append(ind).append("    if (elem) XMQ_SET_ATTRIBUTE(p->ctx, elem, ").append(an).append(", v.data, v.len);\n");
                    b.append(ind).append("    free(v.data);\n");
                    b.append(ind).append("}\n");
                }
                else
                {
                    String en = cstring(elementName(t, callee));
                    b.append(ind).append("if (attr) { if (!").append(fn).append("(p, elem, attr)) return false; }\n");
                    b.append(ind).append("else\n").append(ind).append("{\n");
                    b.append(ind).append("    ").append(P).append("_flush(p);\n");
                    b.append(ind).append("    void *n = elem ? XMQ_ADD_ELEMENT(p->ctx, elem, ").append(en)
                     .append(") : XMQ_ADD_ROOT_ELEMENT(p->ctx, ").append(en).append(");\n");
                    b.append(ind).append("    if (!").append(fn).append("(p, n, NULL)) return false;\n");
                    b.append(ind).append("}\n");
                }
            }
            else if (t.not != null)
            {
                // A negative lookahead, fail if the input starts with the forbidden sequence.
                int len = t.not.size();
                StringBuilder cond = new StringBuilder();
                StringBuilder what = new StringBuilder("not");
                cond.append("p->pos + ").append(len).append(" <= p->n");
                for (int k = 0; k < len; k++)
                {
                    int sid = setId(t.not.get(k));
                    cond.append(" && ").append(inset(sid, "p->in[p->pos + " + k + "]"));
                    what.append(' ').append(describe(setList.get(sid)));
                }
                if (len == 0) cond.setLength(0);
                b.append(ind).append("if (").append(len == 0 ? "true" : cond).append(") return ").append(P)
                 .append("_fail(p, ").append(cstring(r.name)).append(", ").append(cstring(what.toString())).append(");\n");
            }
            else
            {
                // An insertion.
                b.append(ind).append(P).append("_text(p, elem, attr, ").append(cstring(t.insertion)).append(", ")
                 .append(utf8len(t.insertion)).append(");\n");
            }
        }
        b.append(ind).append("return true;\n");
        return loops;
    }

    void footer()
    {
        out.append(String.format("""
// Decode utf8 into code points, returns the number of code points or (size_t)-1 on bad utf8.
static size_t %1$s_decode(const char *start, const char *stop, int *out, size_t *bad_offset)
{
    const unsigned char *i = (const unsigned char*)start, *end = (const unsigned char*)stop;
    size_t n = 0;
    while (i < end)
    {
        int c = *i;
        int len = c < 0x80 ? 1 : (c >> 5) == 6 ? 2 : (c >> 4) == 14 ? 3 : (c >> 3) == 30 ? 4 : 0;
        if (len == 0 || i + len > end) { *bad_offset = (size_t)(i - (const unsigned char*)start); return (size_t)-1; }
        if (len > 1)
        {
            c &= 0x3f >> (len - 1);
            for (int k = 1; k < len; k++)
            {
                if ((i[k] & 0xc0) != 0x80) { *bad_offset = (size_t)(i - (const unsigned char*)start); return (size_t)-1; }
                c = (c << 6) | (i[k] & 0x3f);
            }
        }
        out[n++] = c;
        i += len;
    }
    return n;
}

/**
    Parse the utf8 input start..stop and build the parse tree using the
    XMQ_* macros, with ctx as their first argument (by default an XMQDoc*).
    Returns true on success, else false with the error filled in.
*/
bool %1$s_parse(void *ctx, const char *start, const char *stop, %1$s_Error *error)
{
    %1$s_Parser p;
    memset(&p, 0, sizeof(p));
    p.ctx = ctx;
    int *in = (int*)malloc(sizeof(int) * ((size_t)(stop - start) + 1));
    size_t bad = 0;
    size_t n = %1$s_decode(start, stop, in, &bad);
    if (n == (size_t)-1)
    {
        if (error)
        {
            memset(error, 0, sizeof(*error));
            error->offset = bad;
            snprintf(error->message, sizeof(error->message), "invalid utf8 at byte offset %%zu", bad);
        }
        free(in);
        return false;
    }
    p.in = in;
    p.n = n;
    %2$s
    if (ok && p.pos != p.n) { %1$s_fail(&p, NULL, "EOF"); ok = false; }
    if (ok) %1$s_flush(&p);
    if (!ok && error)
    {
        memset(error, 0, sizeof(*error));
        size_t line = 1, col = 1;
        for (size_t i = 0; i < p.fail_pos && i < n; i++)
        {
            if (in[i] == '\\n') { line++; col = 1; } else col++;
        }
        error->line = line;
        error->col = col;
        error->offset = p.fail_pos;
        int c = %1$s_peek(&p, p.fail_pos);
        char found[32];
        if (c == %1$s_EOF) snprintf(found, sizeof(found), "end of input");
        else snprintf(found, sizeof(found), "#%%x", c);
        if (p.fail_expected)
            snprintf(error->message, sizeof(error->message), "expected %%s in %%s, found %%s",
                     p.fail_expected, p.fail_rule ? p.fail_rule : "input", found);
        else
            snprintf(error->message, sizeof(error->message), "no alternative of %%s matches, found %%s",
                     p.fail_rule ? p.fail_rule : "input", found);
    }
    free(p.text.data);
    free(in);
    return ok;
}

#ifndef XMQ_PARSER_NO_MAIN

static char *%1$s_read_all(FILE *f, size_t *len)
{
    size_t cap = 65536, n = 0;
    char *buf = (char*)malloc(cap);
    size_t r;
    while ((r = fread(buf + n, 1, cap - n, f)) > 0)
    {
        n += r;
        if (n == cap) { cap *= 2; buf = (char*)realloc(buf, cap); }
    }
    *len = n;
    return buf;
}

int main(int argc, char **argv)
{
    const char *file = argc > 1 ? argv[1] : NULL;
    FILE *f = file ? fopen(file, "rb") : stdin;
    if (!f)
    {
        fprintf(stderr, "%1$s: %%s: No such file or directory\\n", file);
        return 1;
    }
    size_t len = 0;
    char *buf = %1$s_read_all(f, &len);
    if (file) fclose(f);

    XMQReturnDoc rd = xmqNewDoc();
    XMQDoc *doq = rd.doc;
    %1$s_Error err;
    if (!%1$s_parse(doq, buf, buf + len, &err))
    {
        fprintf(stderr, "%%s:%%zu:%%zu: error: %%s\\n", file ? file : "stdin", err.line, err.col, err.message);
        free(buf);
        xmqFreeDoc(doq);
        return 1;
    }

    XMQOutputSettings *os = xmqNewOutputSettings();
    xmqSetAddIndent(os, 4);
    xmqSetFinalNewline(os, true);
    xmqSetColorMode(os, XMQ_MONO);
    xmqSetOutputFormat(os, XMQ_CONTENT_XMQ);
    xmqSetupDefaultColors(os);
    xmqSetupPrintStdOutStdErr(os);
    xmqPrint(doq, os);
    xmqFreeOutputSettings(os);
    xmqFreeDoc(doq);
    free(buf);
    return 0;
}

#endif
""", prefix, startCall()));
    }

    /** The call of the start rule, creating the root element unless the start rule is hidden. */
    String startCall()
    {
        Rule r = grammar.rules.get(grammar.start);
        String m = r.mark.isEmpty() ? "^" : r.mark;
        String fn = prefix + "_r" + ruleId(grammar.start);
        if (m.equals("^"))
        {
            String name = cstring(r.alias.isEmpty() ? r.name : r.alias);
            return "void *root = XMQ_ADD_ROOT_ELEMENT(ctx, " + name + ");\n    bool ok = " + fn + "(&p, root, NULL);";
        }
        return "bool ok = " + fn + "(&p, NULL, NULL);";
    }
}
