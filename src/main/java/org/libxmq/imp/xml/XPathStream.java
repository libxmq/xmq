/* libxmq - Copyright (C) 2026 Fredrik Öhrström (spdx: MIT)

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

*/

package org.libxmq.imp.xml;

import java.util.ArrayList;
import java.util.List;

/**
 * Streaming evaluation of simple xpath expressions. libxml2 evaluates
 * expressions without '[', '(', '@' and ':' (eg //a/b, /a/*, .//.) using
 * its xmlPattern streaming automaton instead of the normal xpath engine,
 * and the results differ in some corner cases (eg .//. does not include
 * the context node). This is a port of xmlXPathTryStreamCompile and
 * xmlXPathRunStreamEval in xpath.c, and of the pattern compiler and
 * stream automaton in pattern.c (libxml2 2.9.14, Copyright (C) 1998-2012
 * Daniel Veillard, MIT license).
 */
final class XPathStream
{
    // Pattern ops (xmlPatOp).
    private static final int OP_ROOT = 1;
    private static final int OP_ELEM = 2;
    private static final int OP_ALL = 8;
    private static final int OP_PARENT = 5;
    private static final int OP_ANCESTOR = 6;

    // Pattern flags.
    private static final int PAT_FROM_ROOT = 1 << 8;
    private static final int PAT_FROM_CUR = 1 << 9;

    // Stream step flags.
    private static final int STEP_DESC = 1;
    private static final int STEP_FINAL = 2;
    private static final int STEP_ROOT = 4;
    private static final int STEP_NODE = 16;
    private static final int STEP_IN_SET = 32;

    // Stream comp flags.
    private static final int STREAM_FINAL_IS_ANY_NODE = 1 << 14;
    private static final int STREAM_FROM_ROOT = 1 << 15;
    private static final int STREAM_DESC = 1 << 16;

    // Node types of the stream automaton (libxml2 xmlElementType numbers).
    private static final int ELEMENT_NODE = 1;
    private static final int TEXT_NODE = 3;
    private static final int CDATA_NODE = 4;
    private static final int PI_NODE = 7;
    private static final int COMMENT_NODE = 8;
    private static final int ANY_NODE = 100;

    private static final class Step
    {
        int flags;
        final String name;
        final int node_type;

        Step(String name, int node_type, int flags)
        {
            this.name = name;
            this.node_type = node_type;
            this.flags = flags;
        }
    }

    /** A compiled alternative, ie xmlPattern with its xmlStreamComp. */
    private static final class Comp
    {
        int pattern_flags;
        final List<Step> steps = new ArrayList<>();
        int flags;
    }

    /** The runtime state of one alternative, ie xmlStreamCtxt. */
    private static final class Ctxt
    {
        final Comp comp;
        int[] states = new int[8];
        int nb_state;
        int level;
        int block_level = -1;

        Ctxt(Comp comp)
        {
            this.comp = comp;
        }

        int addState(int idx, int lvl)
        {
            for (int i = 0; i < nb_state; i++)
            {
                if (states[2 * i] < 0)
                {
                    states[2 * i] = idx;
                    states[2 * i + 1] = lvl;
                    return i;
                }
            }
            if (2 * nb_state + 2 > states.length) states = java.util.Arrays.copyOf(states, states.length * 2);
            states[2 * nb_state] = idx;
            states[2 * nb_state + 1] = lvl;
            nb_state++;
            return nb_state - 1;
        }
    }

    private final List<Comp> comps_;

    private XPathStream(List<Comp> comps)
    {
        comps_ = comps;
    }

    // ----------------------------------------------------------------------
    // Compilation

    /** Like xmlXPathTryStreamCompile, returns null if the expression is not streamable. */
    static XPathStream tryCompile(String expr)
    {
        if (expr.indexOf('[') >= 0 || expr.indexOf('(') >= 0 || expr.indexOf('@') >= 0) return null;
        // xmq never registers namespaces in the libxml2 xpath context (nsNr is 0),
        // so any colon prevents streaming.
        if (expr.indexOf(':') >= 0) return null;
        if (expr.indexOf('\0') >= 0) return null;

        // Like xmlPatterncompile, the alternatives are split at '|'.
        List<Comp> comps = new ArrayList<>();
        int type = 0;
        boolean streamable = true;
        int start = 0;
        int or = 0;
        int n = expr.length();
        while (or < n)
        {
            while (or < n && expr.charAt(or) != '|') or++;
            String alt = expr.substring(start, or);
            if (or < n) or++;
            Comp cur = new Comp();
            List<int[]> ops = new ArrayList<>();
            List<String> values = new ArrayList<>();
            if (!compilePathPattern(alt, cur, ops, values)) return null;
            if (streamable)
            {
                if (type == 0)
                {
                    type = cur.pattern_flags & (PAT_FROM_ROOT | PAT_FROM_CUR);
                }
                else if (type == PAT_FROM_ROOT)
                {
                    if ((cur.pattern_flags & PAT_FROM_CUR) != 0) streamable = false;
                }
                else if (type == PAT_FROM_CUR)
                {
                    if ((cur.pattern_flags & PAT_FROM_ROOT) != 0) streamable = false;
                }
            }
            if (streamable && !streamCompile(cur, ops, values)) return null;
            comps.add(cur);
            start = or;
        }
        if (!streamable || comps.isEmpty()) return null;
        return new XPathStream(comps);
    }

    /** Parser state for one alternative. */
    private static final class P
    {
        final String s;
        int pos;

        P(String s)
        {
            this.s = s;
        }

        int cur()
        {
            return pos < s.length() ? s.charAt(pos) : 0;
        }

        int nxt(int i)
        {
            return pos + i < s.length() ? s.charAt(pos + i) : 0;
        }

        void next()
        {
            if (pos < s.length()) pos++;
        }

        void skipBlanks()
        {
            while (XPathUtil.isBlank(cur())) pos++;
        }

        int curChar()
        {
            return pos < s.length() ? s.codePointAt(pos) : 0;
        }

        /** Like xmlPatScanNCName. */
        String scanNCName()
        {
            skipBlanks();
            int st = pos;
            int c = curChar();
            if (!XPathUtil.isLetter(c) && c != '_') return null;
            while (c != 0 && (XPathUtil.isLetter(c) || XPathUtil.isDigit(c) || c == '.' || c == '-' || c == '_'
                              || XPathUtil.isCombining(c) || XPathUtil.isExtender(c)))
            {
                pos += Character.charCount(c);
                c = curChar();
            }
            return s.substring(st, pos);
        }
    }

    /** Like xmlCompilePathPattern, returns false on error. */
    private static boolean compilePathPattern(String alt, Comp comp, List<int[]> ops, List<String> values)
    {
        P p = new P(alt);
        p.skipBlanks();
        if (p.cur() == '/') comp.pattern_flags |= PAT_FROM_ROOT;
        else comp.pattern_flags |= PAT_FROM_CUR;
        if (p.cur() == '/' && p.nxt(1) == '/')
        {
            push(ops, values, OP_ANCESTOR, null);
            p.next();
            p.next();
        }
        else if (p.cur() == '.' && p.nxt(1) == '/' && p.nxt(2) == '/')
        {
            push(ops, values, OP_ANCESTOR, null);
            p.next();
            p.next();
            p.next();
            p.skipBlanks();
            if (p.cur() == 0) return false;
        }
        if (p.cur() == '/')
        {
            push(ops, values, OP_ROOT, null);
            p.next();
            p.skipBlanks();
            if (p.cur() == 0) return false;
        }
        if (!compileStepPattern(p, ops, values)) return false;
        p.skipBlanks();
        while (p.cur() == '/')
        {
            if (p.nxt(1) == '/')
            {
                push(ops, values, OP_ANCESTOR, null);
                p.next();
                p.next();
                p.skipBlanks();
                if (!compileStepPattern(p, ops, values)) return false;
            }
            else
            {
                push(ops, values, OP_PARENT, null);
                p.next();
                p.skipBlanks();
                if (p.cur() == 0) return false;
                if (!compileStepPattern(p, ops, values)) return false;
            }
        }
        return p.cur() == 0;
    }

    /** Like xmlCompileStepPattern (without the attribute and namespace parts). */
    private static boolean compileStepPattern(P p, List<int[]> ops, List<String> values)
    {
        p.skipBlanks();
        if (p.cur() == '.')
        {
            p.next();
            push(ops, values, OP_ELEM, null);
            return true;
        }
        String name = p.scanNCName();
        if (name == null)
        {
            if (p.cur() == '*')
            {
                p.next();
                push(ops, values, OP_ALL, null);
                return true;
            }
            return false;
        }
        if (XPathUtil.isBlank(p.cur())) p.skipBlanks();
        if (p.cur() == '*') return false;
        push(ops, values, OP_ELEM, name);
        return true;
    }

    private static void push(List<int[]> ops, List<String> values, int op, String value)
    {
        ops.add(new int[] { op });
        values.add(value);
    }

    /** Like xmlStreamCompile, returns false if the alternative cannot be streamed. */
    private static boolean streamCompile(Comp comp, List<int[]> ops, List<String> values)
    {
        int nb = ops.size();
        if (nb == 0) return false;
        if (nb == 1 && ops.get(0)[0] == OP_ELEM && values.get(0) == null)
        {
            comp.flags |= STREAM_FINAL_IS_ANY_NODE;
            return true;
        }
        if ((comp.pattern_flags & PAT_FROM_ROOT) != 0) comp.flags |= STREAM_FROM_ROOT;
        int s = 0;
        boolean root = false;
        int flags = 0;
        int prevs = -1;
        for (int i = 0; i < nb; i++)
        {
            int op = ops.get(i)[0];
            String value = values.get(i);
            switch (op)
            {
            case OP_ROOT:
                if (i != 0) return false;
                root = true;
                break;
            case OP_ELEM:
                if (value == null)
                {
                    if (nb == i + 1 && (flags & STEP_DESC) != 0)
                    {
                        comp.flags |= STREAM_FINAL_IS_ANY_NODE;
                        flags |= STEP_NODE;
                        comp.steps.add(new Step(null, ANY_NODE, flags));
                        s = comp.steps.size() - 1;
                        flags = 0;
                        if (prevs != -1)
                        {
                            comp.steps.get(prevs).flags |= STEP_IN_SET;
                            prevs = -1;
                        }
                        break;
                    }
                    continue;
                }
                comp.steps.add(new Step(value, ELEMENT_NODE, flags));
                s = comp.steps.size() - 1;
                prevs = s;
                flags = 0;
                break;
            case OP_ALL:
                comp.steps.add(new Step(null, ELEMENT_NODE, flags));
                s = comp.steps.size() - 1;
                prevs = s;
                flags = 0;
                break;
            case OP_PARENT:
                break;
            case OP_ANCESTOR:
                if ((flags & STEP_DESC) != 0) break;
                flags |= STEP_DESC;
                comp.flags |= STREAM_DESC;
                break;
            default:
                return false;
            }
        }
        if (comp.steps.size() <= s) return false;
        comp.steps.get(s).flags |= STEP_FINAL;
        if (root) comp.steps.get(0).flags |= STEP_ROOT;
        return true;
    }

    // ----------------------------------------------------------------------
    // The stream automaton

    private static boolean stepMatches(Step step, String name, String ns)
    {
        if (step.node_type == ANY_NODE) return true;
        if (step.name == null) return true; // patterns without prefixes have no namespace
        return ns == null && name != null && step.name.equals(name);
    }

    /** Like xmlStreamPushInternal. */
    private int push(List<Ctxt> streams, String name, String ns, int node_type)
    {
        int ret = 0;
        for (Ctxt stream : streams)
        {
            Comp comp = stream.comp;
            int nb_step = comp.steps.size();
            if (node_type == ELEMENT_NODE && name == null && ns == null)
            {
                // A document node (or a reset).
                stream.nb_state = 0;
                stream.level = 0;
                stream.block_level = -1;
                if ((comp.flags & STREAM_FROM_ROOT) != 0)
                {
                    if (nb_step == 0)
                    {
                        ret = 1;
                    }
                    else if (nb_step == 1 && comp.steps.get(0).node_type == ANY_NODE
                             && (comp.steps.get(0).flags & STEP_DESC) != 0)
                    {
                        ret = 1;
                    }
                    else if ((comp.steps.get(0).flags & STEP_ROOT) != 0)
                    {
                        stream.addState(0, 0);
                    }
                }
                continue;
            }
            if (nb_step == 0)
            {
                // "." is handled by the min depth check.
                continue;
            }
            if (stream.block_level != -1)
            {
                stream.level++;
                continue;
            }
            if (node_type != ELEMENT_NODE && (comp.flags & STREAM_FINAL_IS_ANY_NODE) == 0)
            {
                stream.level++;
                continue;
            }
            int i = 0;
            int m = stream.nb_state;
            boolean match = false;
            int fin = 0;
            while (i < m)
            {
                int step_nr;
                int desc;
                if ((comp.flags & STREAM_DESC) == 0)
                {
                    step_nr = stream.states[2 * (stream.nb_state - 1)];
                    if (stream.states[2 * (stream.nb_state - 1) + 1] < stream.level) return -1;
                    desc = 0;
                    i = m;
                }
                else
                {
                    step_nr = stream.states[2 * i];
                    if (step_nr < 0)
                    {
                        i++;
                        continue;
                    }
                    int tmp = stream.states[2 * i + 1];
                    if (tmp > stream.level)
                    {
                        i++;
                        continue;
                    }
                    desc = comp.steps.get(step_nr).flags & STEP_DESC;
                    if (tmp < stream.level && desc == 0)
                    {
                        i++;
                        continue;
                    }
                }
                Step step = comp.steps.get(step_nr);
                if (step.node_type != node_type && step.node_type != ANY_NODE)
                {
                    i++;
                    continue;
                }
                match = stepMatches(step, name, ns);
                if (match)
                {
                    fin = step.flags & STEP_FINAL;
                    if (fin != 0) ret = 1;
                    else stream.addState(step_nr + 1, stream.level + 1);
                    if (ret != 1 && (step.flags & STEP_IN_SET) != 0) ret = 1;
                }
                if ((comp.flags & STREAM_DESC) == 0 && (!match || fin != 0))
                {
                    stream.block_level = stream.level + 1;
                }
                i++;
            }
            stream.level++;
            Step step = comp.steps.get(0);
            if ((step.flags & STEP_ROOT) != 0) continue;
            int desc = step.flags & STEP_DESC;
            // Xpath streams are NOTPATTERN streams.
            if (stream.level != 1 && desc == 0) continue;
            if (step.node_type != node_type && step.node_type != ANY_NODE) continue;
            match = stepMatches(step, name, ns);
            fin = step.flags & STEP_FINAL;
            if (match)
            {
                if (fin != 0) ret = 1;
                else stream.addState(1, stream.level);
                if (ret != 1 && (step.flags & STEP_IN_SET) != 0) ret = 1;
            }
            if ((comp.flags & STREAM_DESC) == 0 && (!match || fin != 0))
            {
                stream.block_level = stream.level;
            }
        }
        return ret;
    }

    /** Like xmlStreamPop. */
    private static void pop(List<Ctxt> streams)
    {
        for (Ctxt stream : streams)
        {
            if (stream.block_level == stream.level) stream.block_level = -1;
            if (stream.level != 0) stream.level--;
            for (int i = stream.nb_state - 1; i >= 0; i--)
            {
                int lev = stream.states[2 * i + 1];
                if (lev > stream.level) stream.nb_state--;
                if (lev <= stream.level) break;
            }
        }
    }

    // ----------------------------------------------------------------------
    // Evaluation

    private static int typeNumber(XmlNode n)
    {
        switch (n.type())
        {
        case ELEMENT: return ELEMENT_NODE;
        case TEXT: return TEXT_NODE;
        case CDATA: return CDATA_NODE;
        case COMMENT: return COMMENT_NODE;
        case PI: return PI_NODE;
        default: return -1;
        }
    }

    private static boolean skipped(XmlNode n)
    {
        return n.type() == XmlNodeType.ENTITY_DECL || n.type() == XmlNodeType.DTD;
    }

    /** The libxml2 children pointer, an entity reference points to its declaration. */
    private static XmlNode children(XmlNode n)
    {
        if (n.type() == XmlNodeType.ENTITY_REF) return ((XmlEntityRef)n).entity();
        return n.firstChild();
    }

    /** Like xmlXPathRunStreamEval. */
    List<XmlNode> eval(XmlNode context, XmlNode doc)
    {
        List<XmlNode> result = new ArrayList<>();
        int max_depth = 0;
        int min_depth = 12345678;
        boolean from_root = false;
        boolean eval_all_nodes = false;
        for (Comp c : comps_)
        {
            for (Step s : c.steps) if ((s.flags & STEP_DESC) != 0) max_depth = -2;
            if (max_depth != -2 && c.steps.size() > max_depth) max_depth = c.steps.size();
            if (c.steps.size() < min_depth) min_depth = c.steps.size();
            if ((c.pattern_flags & PAT_FROM_ROOT) != 0) from_root = true;
            if ((c.flags & STREAM_FINAL_IS_ANY_NODE) != 0) eval_all_nodes = true;
        }
        if (max_depth == -2) max_depth = 10000;

        if (min_depth == 0)
        {
            if (from_root)
            {
                if (doc != null) result.add(doc);
            }
            else if (context != null)
            {
                result.add(context);
            }
        }
        if (max_depth == 0) return result;

        XmlNode cur = null;
        XmlNode limit = null;
        if (from_root)
        {
            cur = doc;
        }
        else if (context != null)
        {
            switch (context.type())
            {
            case ELEMENT: case DOCUMENT: case HTML_DOCUMENT:
                cur = context;
                break;
            default:
                break;
            }
            limit = cur;
        }
        if (cur == null) return result;

        // libxml2 orders the alternatives first, last, ..., second, the order does not matter.
        List<Ctxt> streams = new ArrayList<>();
        for (Comp c : comps_) streams.add(new Ctxt(c));

        if (from_root)
        {
            int ret = push(streams, null, null, ELEMENT_NODE);
            if (ret == 1) result.add(cur);
        }
        int depth = 0;
        boolean scan_children = true;
        outer:
        for (;;)
        {
            if (!scan_children)
            {
                // next_node:
                int t = typeNumber(cur);
                boolean handled = false;
                if (t == ELEMENT_NODE || ((t == TEXT_NODE || t == CDATA_NODE || t == COMMENT_NODE || t == PI_NODE)
                                          && eval_all_nodes))
                {
                    int ret;
                    if (t == ELEMENT_NODE)
                    {
                        XmlNs ns = ((XmlElement)cur).ns();
                        ret = push(streams, cur.name(), ns == null ? null : ns.href(), ELEMENT_NODE);
                    }
                    else
                    {
                        ret = push(streams, null, null, t);
                    }
                    if (ret == 1) result.add(cur);
                    if (children(cur) == null || depth >= max_depth)
                    {
                        pop(streams);
                        while (cur.next() != null)
                        {
                            cur = cur.next();
                            if (!skipped(cur))
                            {
                                handled = true;
                                break;
                            }
                        }
                    }
                }
                if (handled) continue; // goto next_node
            }
            scan_children = false;
            // scan_children:
            if (cur.type() == XmlNodeType.NAMESPACE) break;
            XmlNode ch = children(cur);
            if (ch != null && depth < max_depth)
            {
                if (ch.type() != XmlNodeType.ENTITY_DECL)
                {
                    cur = ch;
                    depth++;
                    if (cur.type() != XmlNodeType.DTD)
                    {
                        if (depth < 0) break;
                        continue;
                    }
                }
            }
            if (cur == limit) break;
            boolean found = false;
            while (cur.next() != null)
            {
                cur = cur.next();
                if (!skipped(cur))
                {
                    found = true;
                    break;
                }
            }
            if (found) continue;
            do
            {
                cur = cur.parent();
                depth--;
                if (cur == null || cur == limit || cur.type() == XmlNodeType.DOCUMENT) break outer;
                if (cur.type() == XmlNodeType.ELEMENT)
                {
                    pop(streams);
                }
                else if (eval_all_nodes && typeNumber(cur) > 1)
                {
                    pop(streams);
                }
                if (cur.next() != null)
                {
                    cur = cur.next();
                    break;
                }
            }
            while (cur != null);
            if (cur == null || depth < 0) break;
        }
        return result;
    }
}
