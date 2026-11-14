/* libxmq - Copyright (C) 2026 Fredrik Öhrström (spdx: MIT) */
package org.libxmq.imp.ixml;

import java.util.*;
import org.libxmq.imp.ixml.IXMLGrammar.Rule;
import org.libxmq.imp.ixml.IXMLGrammar.Term;

/**
 * LL(*) lookahead analysis of a normalized ixml grammar.
 *
 * Every rule with more than one alternative is a decision. For each decision
 * a lookahead DFA over unicode code points (plus EOF) is built by subset
 * construction over ATN configurations, in the style of ANTLR's LL(*):
 * a configuration is a grammar position (rule, alternative, index), the
 * alternative it predicts and a return stack. A call that is the last term of
 * an alternative is a tail call and pushes nothing, so the right recursive
 * repetition rules of the grammar reader become loops. At the end of a rule
 * with an empty stack, the analysis continues into all call sites of the
 * rule (SLL), at the end of the start rule only EOF can follow.
 *
 * A configuration also carries the cost (ixml =&lt; extension) of the rules
 * entered since the start of the decision. When two alternatives reach the
 * same position with the same stack, the input is ambiguous, but if one of
 * them is cheaper, the more expensive configuration is dropped, ie the parser
 * prefers the alternative with the least cost.
 *
 * A DFA state where all configurations predict the same alternative is an
 * accept state. The analysis fails for a decision when two alternatives reach
 * the same position with the same stack (ambiguity), when a deeper stack than
 * the limit is needed (the lookahead language is not regular), or when the
 * DFA grows too large.
 *
 * A negative lookahead !'abc' or ![set] is a zero width term. When the
 * closure passes it, the configuration gets a pending constraint (predicate,
 * offset): the next input symbols must not match the sets of the predicate.
 * Every consumed symbol either moves the constraint forward, satisfies it (the
 * symbol does not match, or EOF) or kills the configuration (the whole
 * predicate matched). This makes greedy rules like number = [N]+, ![N]. LL(*):
 * after a digit the alternative that leaves the loop dies.
 */
final class IXMLLookahead {

    static final int EOF = IXMLGrammar.EOF;
    static final int STACK_LIMIT = 16;
    static final int STATE_LIMIT = 5000;
    static final long WORK_LIMIT = 150_000;
    static final long STEP_LIMIT = 3_000_000;
    private long steps; // Closure steps of the current decision attempt.
    // Runtime prediction (predictDynamic): no limits, and an ambiguity picks the
    // first of the cheapest alternatives instead of failing.
    private boolean dynamic;
    // Runtime SLL: a conflict with an exact stack is resolved like LL would, only
    // a conflict with an UNKNOWN bottom is a failure (fall back to LL).
    private boolean sllRuntime;

    // ------------------------------------------------------------ char sets

    /** Sorted disjoint inclusive code point ranges. */
    static final class Ranges {
        final int[] lo, hi;
        Ranges(int[] lo, int[] hi) { this.lo = lo; this.hi = hi; }
        static Ranges of(BitSet b) {
            List<int[]> l = new ArrayList<>();
            for (int i = b.nextSetBit(0); i >= 0;) {
                int end = b.nextClearBit(i);
                // EOF is a separate symbol, never part of a scalar range.
                if (i < EOF && end > EOF) { l.add(new int[]{i, EOF - 1}); l.add(new int[]{EOF, EOF}); }
                else l.add(new int[]{i, end - 1});
                i = b.nextSetBit(end);
            }
            int[] lo = new int[l.size()], hi = new int[l.size()];
            for (int k = 0; k < l.size(); k++) { lo[k] = l.get(k)[0]; hi[k] = l.get(k)[1]; }
            return new Ranges(lo, hi);
        }
        boolean contains(int c) {
            int a = 0, b = lo.length - 1;
            while (a <= b) {
                int m = (a + b) >>> 1;
                if (c < lo[m]) b = m - 1; else if (c > hi[m]) a = m + 1; else return true;
            }
            return false;
        }
    }

    // ------------------------------------------------------------ ATN

    final IXMLGrammar grammar;
    final String[] names;                 // rule index -> name
    final Map<String, Integer> index = new HashMap<>();
    final Term[][][] alts;                // rule, alt, term
    final Ranges[][][] sets;              // match terms as ranges (null for others)
    final List<int[]>[] callSites;        // rule -> list of {rule, alt, idx} calling it
    final int start;
    final int[] costs;                    // rule -> cost
    final int[][][] predIds;              // rule, alt, term -> predicate id, -1 if not a predicate
    final List<Ranges[]> preds = new ArrayList<>(); // predicate id -> the sets to not match, in order

    @SuppressWarnings("unchecked")
    IXMLLookahead(IXMLGrammar g) {
        grammar = g;
        int n = g.rules.size();
        names = new String[n];
        alts = new Term[n][][];
        sets = new Ranges[n][][];
        predIds = new int[n][][];
        callSites = new List[n];
        int k = 0;
        costs = new int[n];
        for (Rule r : g.rules.values()) { names[k] = r.name; costs[k] = r.cost; index.put(r.name, k); k++; }
        for (Rule r : g.rules.values()) {
            int ri = index.get(r.name);
            callSites[ri] = new ArrayList<>();
            alts[ri] = new Term[r.alternatives.size()][];
            sets[ri] = new Ranges[r.alternatives.size()][];
            predIds[ri] = new int[r.alternatives.size()][];
            for (int a = 0; a < r.alternatives.size(); a++) {
                List<Term> terms = r.alternatives.get(a);
                alts[ri][a] = terms.toArray(new Term[0]);
                sets[ri][a] = new Ranges[terms.size()];
                predIds[ri][a] = new int[terms.size()];
                for (int t = 0; t < terms.size(); t++) {
                    predIds[ri][a][t] = -1;
                    if (terms.get(t).chars != null) sets[ri][a][t] = Ranges.of(terms.get(t).chars);
                    if (terms.get(t).not != null) {
                        Ranges[] p = new Ranges[terms.get(t).not.size()];
                        for (int q = 0; q < p.length; q++) p[q] = Ranges.of(terms.get(t).not.get(q));
                        predIds[ri][a][t] = preds.size();
                        preds.add(p);
                    }
                }
            }
        }
        for (int ri = 0; ri < n; ri++)
            for (int a = 0; a < alts[ri].length; a++)
                for (int t = 0; t < alts[ri][a].length; t++)
                    if (alts[ri][a][t].ref != null) callSites[index.get(alts[ri][a][t].ref)].add(new int[]{ri, a, t});
        start = index.get(g.start);
    }

    /**
     * An immutable return stack of grammar positions. Stacks are interned (see
     * push), so two equal stacks are the same object and equality and hashing
     * use the object identity, which is fast also for deep stacks.
     */
    static final class Stack {
        final int rule, alt, idx, depth;
        final Stack next;
        Stack(int rule, int alt, int idx, Stack next, int depth) {
            this.rule = rule; this.alt = alt; this.idx = idx; this.next = next; this.depth = depth;
        }
    }
    record StackKey(int rule, int alt, int idx, Stack next) { }
    private final Map<StackKey, Stack> interned = new HashMap<>();

    /** Push a return position, returning the interned stack. */
    Stack push(int rule, int alt, int idx, Stack next) {
        StackKey k = new StackKey(rule, alt, idx, next);
        Stack s = interned.get(k);
        if (s == null) {
            s = new Stack(rule, alt, idx, next, next == null ? 1 : next.depth + 1);
            interned.put(k, s);
        }
        return s;
    }

    /**
     * The bottom of a stack whose outer context is unknown, because the full
     * call context would be deeper than the limit. Returning into UNKNOWN
     * continues at all call sites of the rule (SLL). An empty stack (null)
     * instead means the full context is known to be empty, ie the start rule
     * was reached (tail calls push nothing), so only EOF can follow.
     */
    static final Stack UNKNOWN = new Stack(-3, 0, 0, null, 0);

    /**
     * The pending negative lookahead constraints of a configuration, sorted
     * pairs (predicate id, offset into the predicate). Interned like the
     * stacks, null means no constraints.
     */
    static final class Neg {
        final int[] pairs;
        Neg(int[] pairs) { this.pairs = pairs; }
    }
    private final Map<List<Integer>, Neg> internedNeg = new HashMap<>();

    Neg neg(List<int[]> pairs) {
        if (pairs.isEmpty()) return null;
        pairs.sort((x, y) -> x[0] != y[0] ? Integer.compare(x[0], y[0]) : Integer.compare(x[1], y[1]));
        List<Integer> key = new ArrayList<>();
        for (int[] q : pairs) {
            int n = key.size();
            if (n >= 2 && key.get(n - 2) == q[0] && key.get(n - 1) == q[1]) continue; // duplicate
            key.add(q[0]); key.add(q[1]);
        }
        Neg r = internedNeg.get(key);
        if (r == null) {
            int[] a = new int[key.size()];
            for (int i = 0; i < a.length; i++) a[i] = key.get(i);
            r = new Neg(a);
            internedNeg.put(key, r);
        }
        return r;
    }

    /** Add the constraint for the start of predicate p, null if the predicate always matches (empty). */
    Neg addNeg(Neg n, int p) {
        List<int[]> l = new ArrayList<>();
        if (n != null) for (int i = 0; i < n.pairs.length; i += 2) l.add(new int[]{n.pairs[i], n.pairs[i + 1]});
        l.add(new int[]{p, 0});
        return neg(l);
    }

    static final Neg DEAD = new Neg(new int[0]);

    /** The constraints after consuming symbol, DEAD if a predicate matched completely. */
    Neg stepNeg(Neg n, int symbol) {
        if (n == null || symbol == EOF) return null; // EOF satisfies every negative lookahead.
        List<int[]> l = new ArrayList<>();
        for (int i = 0; i < n.pairs.length; i += 2) {
            Ranges[] p = preds.get(n.pairs[i]);
            int k = n.pairs[i + 1];
            if (!p[k].contains(symbol)) continue; // Satisfied, the input differs.
            if (k + 1 == p.length) return DEAD;   // The forbidden input is here.
            l.add(new int[]{n.pairs[i], k + 1});
        }
        return neg(l);
    }

    /**
     * A configuration. rule == WAIT_EOF means the start rule has ended and
     * only EOF can follow, rule == DONE means EOF has been read.
     */
    record Config(int rule, int alt, int idx, int predicted, Stack stack, int cost, Neg neg) {
        Config(int rule, int alt, int idx, int predicted, Stack stack) { this(rule, alt, idx, predicted, stack, 0, null); }
    }
    static final int WAIT_EOF = -1, DONE = -2;
    static final Ranges EOF_RANGES = new Ranges(new int[]{EOF}, new int[]{EOF});

    /** Thrown when the analysis of a decision fails. */
    static final class Failure extends Exception {
        private static final long serialVersionUID = 1L;
        final String code;
        Failure(String code, String message) { super(message); this.code = code; }
    }

    /** Is the configuration a point that consumes a symbol (or is done)? */
    boolean isTransition(Config c) {
        if (c.rule < 0) return true;
        Term[] terms = alts[c.rule][c.alt];
        return c.idx < terms.length && terms[c.idx].chars != null;
    }

    /** Epsilon closure: follow calls, returns, insertions and rule ends. */
    Set<Config> closure(Collection<Config> seeds) throws Failure {
        Set<Config> seen = new HashSet<>();
        Set<Config> result = new LinkedHashSet<>();
        Deque<Config> todo = new ArrayDeque<>(seeds);
        while (!todo.isEmpty()) {
            Config c = todo.pop();
            if (!seen.add(c)) continue;
            if (++steps > STEP_LIMIT && !dynamic && !sllRuntime)
                throw new Failure("too-large", "The lookahead analysis needs too much work, more than "
                                  + STEP_LIMIT + " steps.");
            if (isTransition(c)) { result.add(c); continue; }
            Term[] terms = alts[c.rule][c.alt];
            if (c.idx == terms.length) {
                // End of rule.
                if (c.stack == null) {
                    // The full context is empty, the start rule ends here.
                    todo.push(new Config(WAIT_EOF, 0, 0, c.predicted, null, c.cost, c.neg));
                } else if (c.stack == UNKNOWN) {
                    // SLL: continue into every call site of this rule.
                    if (c.rule == start) todo.push(new Config(WAIT_EOF, 0, 0, c.predicted, null, c.cost, c.neg));
                    for (int[] site : callSites[c.rule])
                        todo.push(new Config(site[0], site[1], site[2] + 1, c.predicted, UNKNOWN, c.cost, c.neg));
                } else {
                    todo.push(new Config(c.stack.rule, c.stack.alt, c.stack.idx, c.predicted, c.stack.next, c.cost, c.neg));
                }
                continue;
            }
            Term t = terms[c.idx];
            if (t.ref != null) {
                int callee = index.get(t.ref);
                Stack s = c.stack;
                if (c.idx + 1 < terms.length) {
                    s = push(c.rule, c.alt, c.idx + 1, c.stack);
                    if (s.depth > STACK_LIMIT && !dynamic && !sllRuntime)
                        throw new Failure("non-regular", "The lookahead needs a recursion deeper than "
                                          + STACK_LIMIT + ", it is not a regular language. The recursion: "
                                          + recursion(s) + ".");
                }
                for (int a = 0; a < alts[callee].length; a++)
                    todo.push(new Config(callee, a, 0, c.predicted, s, c.cost + costs[callee], c.neg));
            } else if (t.not != null) {
                // A negative lookahead consumes nothing, but constrains the following input.
                int p = predIds[c.rule][c.alt][c.idx];
                if (preds.get(p).length == 0) continue; // !'' never succeeds.
                todo.push(new Config(c.rule, c.alt, c.idx + 1, c.predicted, c.stack, c.cost, addNeg(c.neg, p)));
            } else {
                // An insertion consumes nothing.
                todo.push(new Config(c.rule, c.alt, c.idx + 1, c.predicted, c.stack, c.cost, c.neg));
            }
        }
        return result;
    }

    /** The named (not generated) rules on the stack, innermost first, repeated runs collapsed. */
    String recursion(Stack s) {
        List<String> l = new ArrayList<>();
        for (Stack i = s; i != null && i != UNKNOWN; i = i.next) {
            String n = names[i.rule];
            if (n.startsWith("/")) continue;
            if (l.isEmpty() || !l.get(l.size() - 1).equals(n)) l.add(n);
        }
        // Show one turn of the cycle: stop when the innermost rule comes back.
        List<String> cycle = new ArrayList<>();
        for (String n : l) {
            if (!cycle.isEmpty() && n.equals(cycle.get(0))) { cycle.add(n); break; }
            cycle.add(n);
        }
        return String.join(" <- ", cycle);
    }

    /** Does the configuration accept the symbol? */
    boolean matches(Config c, int symbol) {
        if (c.rule == WAIT_EOF) return symbol == EOF;
        if (c.rule == DONE) return false;
        return sets[c.rule][c.alt][c.idx].contains(symbol);
    }

    /** The configuration after consuming symbol, null if a negative lookahead kills it. */
    Config advance(Config c, int symbol) {
        Neg n = stepNeg(c.neg, symbol);
        if (n == DEAD) return null;
        if (c.rule == WAIT_EOF) return new Config(DONE, 0, 0, c.predicted, null, c.cost, n);
        return new Config(c.rule, c.alt, c.idx + 1, c.predicted, c.stack, c.cost, n);
    }

    // ------------------------------------------------------------ DFA

    /** A lookahead DFA for one decision. */
    static final class Dfa {
        final String rule;
        final List<State> states = new ArrayList<>();
        Dfa(String rule) { this.rule = rule; }

        /** Predict the alternative (0 based) for the input at pos, or -1 if no alternative matches. */
        int predict(int[] input, int pos) {
            State s = states.get(0);
            int p = pos;
            while (s.predict < 0) {
                int c = p < input.length ? input[p] : EOF;
                State next = null;
                for (Edge e : s.edges) if (c >= e.lo && c <= e.hi) { next = e.target; break; }
                if (next == null) return -1;
                s = next;
                p++;
                if (p > input.length + 1) return -1;
            }
            return s.predict;
        }

        /** Max number of symbols needed to decide, -1 if unbounded. */
        int maxLookahead() {
            Map<State, Integer> memo = new HashMap<>();
            Set<State> onPath = new HashSet<>();
            return depth(states.get(0), memo, onPath);
        }
        private int depth(State s, Map<State, Integer> memo, Set<State> onPath) {
            if (s.predict >= 0) return 0;
            Integer m = memo.get(s);
            if (m != null) return m;
            if (!onPath.add(s)) return -1;
            int max = 0;
            for (Edge e : s.edges) {
                int d = depth(e.target, memo, onPath);
                if (d < 0) { onPath.remove(s); memo.put(s, -1); return -1; }
                max = Math.max(max, d + 1);
            }
            onPath.remove(s);
            memo.put(s, max);
            return max;
        }
    }

    /** A DFA state. predict is the predicted alternative, or -1 if more lookahead is needed. */
    static final class State {
        final int id;
        int predict = -1;
        final List<Edge> edges = new ArrayList<>();
        State parent;   // For building example inputs in diagnostics.
        int via;        // The symbol read from parent to reach this state.
        State(int id) { this.id = id; }
    }

    /** An edge on the code point range lo..hi (EOF is a separate symbol). */
    record Edge(int lo, int hi, State target) { }

    /**
     * All return stacks the rule can be called with (full LL context), computed
     * from the call graph. Tail calls push nothing. The start rule is called with
     * the empty stack. When recursion would make the stacks deeper than the
     * limit, the empty stack is used instead, which falls back to SLL (all call
     * sites) for that context.
     */
    Set<Stack> contexts(int rule) {
        Set<Stack> memo = contextMemo.get(rule);
        if (memo != null) return memo;
        Set<Stack> result = new LinkedHashSet<>();
        contexts(rule, null, new HashSet<>(), result, 0);
        contextMemo.put(rule, result);
        return result;
    }
    private final Map<Integer, Set<Stack>> contextMemo = new HashMap<>();

    // Walk the callers of rule, with suffix being the stack collected so far
    // (innermost return first). The stack is built outermost-last.
    private void contexts(int rule, Stack inner, Set<String> visiting, Set<Stack> out, int steps) {
        if (out.size() > CONTEXT_LIMIT) { out.add(withBottom(inner)); return; }
        if (rule == start) out.add(inner);
        String key = rule + ":" + System.identityHashCode(inner) + ":" + (inner == null ? 0 : inner.depth);
        // Revisiting the same rule with the same stack (eg a tail recursive
        // loop) adds no new context.
        if (!visiting.add(key)) return;
        if ((inner != null && inner.depth > CONTEXT_DEPTH) || steps > 200) {
            visiting.remove(key);
            out.add(withBottom(inner));
            return;
        }
        for (int[] site : callSites[rule]) {
            boolean tail = site[2] + 1 == alts[site[0]][site[1]].length;
            Stack s = tail ? inner : append(inner, site[0], site[1], site[2] + 1);
            contexts(site[0], s, visiting, out, steps + 1);
        }
        visiting.remove(key);
    }
    static final int CONTEXT_LIMIT = 2000;
    static final int CONTEXT_DEPTH = 8;

    // The stack with an UNKNOWN bottom, used when the full context is too deep.
    Stack withBottom(Stack s) {
        if (s == null) return UNKNOWN;
        return push(s.rule, s.alt, s.idx, withBottom(s.next));
    }

    // Add a return position at the outer (bottom) end of the stack.
    Stack append(Stack s, int rule, int alt, int idx) {
        if (s == null) return push(rule, alt, idx, null);
        return push(s.rule, s.alt, s.idx, append(s.next, rule, alt, idx));
    }

    /**
     * Build the lookahead DFA for the decision in rule. Like ANTLR, the fast
     * SLL analysis (unknown context, all call sites follow a rule) is tried
     * first. It is correct when it finds no conflict. When it reports an
     * ambiguity, which can be caused by merging unrelated call sites, or
     * needs too much work, the decision is analyzed again with the full LL
     * contexts of the rule.
     */
    Dfa build(String rule) throws Failure {
        try {
            return build(rule, Set.of(UNKNOWN));
        } catch (Failure f) {
            if (f.code.equals("too-large")) {
                // The unknown context follows every call site of a rule that
                // ends a tail call, which can explode, eg the exit of a loop
                // at the end of a rule called in many places. The full
                // contexts are precise and often cheap, try them before
                // giving up.
                try {
                    return build(rule, contexts(index.get(rule)));
                } catch (Failure g) {
                    throw f;
                }
            }
            if (!f.code.equals("ambiguous") && !f.code.equals("context-limit")) throw f;
            return build(rule, contexts(index.get(rule)));
        }
    }

    Dfa build(String rule, Set<Stack> ctx) throws Failure {
        int ri = index.get(rule);
        long work = 0;
        steps = 0;
        Dfa dfa = new Dfa(rule);
        List<Config> seeds = new ArrayList<>();
        if (System.getenv("XMQ_LL_DEBUG") != null) {
            System.err.println("decision " + rule + " contexts " + ctx.size());
            for (Stack st : ctx) {
                StringBuilder b = new StringBuilder("   ");
                if (st == null) b.append("<empty>");
                for (Stack i = st; i != null; i = i.next) {
                    if (i == UNKNOWN) { b.append(" <- ?"); break; }
                    b.append(" <- ").append(names[i.rule]).append('/').append(i.alt + 1).append('.').append(i.idx);
                }
                System.err.println(b);
            }
        }
        for (int a = 0; a < alts[ri].length; a++)
            for (Stack st : ctx) seeds.add(new Config(ri, a, 0, a, st));
        Map<Set<Config>, State> known = new HashMap<>();
        Map<Integer, Set<Config>> decided = new HashMap<>();
        Deque<Map.Entry<Set<Config>, State>> todo = new ArrayDeque<>();
        Set<Config> s0 = closure(seeds);
        State st0 = newState(dfa, known, todo, s0, null, 0);
        while (!todo.isEmpty()) {
            Map.Entry<Set<Config>, State> e = todo.pop();
            State state = e.getValue();
            Set<Config> configs = resolve(rule, state, e.getKey());
            Set<Integer> predicted = new TreeSet<>();
            for (Config c : configs) predicted.add(c.predicted);
            if (predicted.size() == 1) { state.predict = predicted.iterator().next(); continue; }
            if (predicted.isEmpty()) continue; // Dead state, no alternative can continue.
            // Partition the symbols into ranges with identical targets. A sweep
            // over the range starts and ends of all configurations gives, for
            // each elementary range, the set of configurations that move on it.
            Config[] cs = configs.toArray(new Config[0]);
            // Many configurations share the same symbol set (the same term), so
            // the sweep is done over the distinct sets of the state.
            Map<Ranges, Integer> setIds = new IdentityHashMap<>();
            List<Ranges> distinct = new ArrayList<>();
            List<List<Integer>> members = new ArrayList<>();
            for (int ci = 0; ci < cs.length; ci++) {
                Config c = cs[ci];
                Ranges r = c.rule == WAIT_EOF ? EOF_RANGES : c.rule >= 0 ? sets[c.rule][c.alt][c.idx] : null;
                if (r == null) continue;
                Integer id = setIds.get(r);
                if (id == null) { id = distinct.size(); setIds.put(r, id); distinct.add(r); members.add(new ArrayList<>()); }
                members.get(id).add(ci);
            }
            // The sets of the pending negative lookaheads split the symbol
            // ranges too, since they decide which configurations survive. They
            // move no configuration by themselves.
            for (Config c : cs) {
                if (c.neg == null) continue;
                for (int i = 0; i < c.neg.pairs.length; i += 2) {
                    Ranges r = preds.get(c.neg.pairs[i])[c.neg.pairs[i + 1]];
                    if (setIds.containsKey(r)) continue;
                    setIds.put(r, distinct.size()); distinct.add(r); members.add(new ArrayList<>());
                }
            }
            List<long[]> events = new ArrayList<>(); // {position, set id, +1 start / -1 end}
            for (int id = 0; id < distinct.size(); id++) {
                Ranges r = distinct.get(id);
                for (int k = 0; k < r.lo.length; k++) {
                    events.add(new long[]{r.lo[k], id, 1});
                    events.add(new long[]{(long)r.hi[k] + 1, id, -1});
                }
            }
            events.sort((x, y) -> Long.compare(x[0], y[0]));
            // Compute the closure only once for each set of moving symbol sets.
            Map<BitSet, Set<Config>> closures = new HashMap<>();
            BitSet active = new BitSet(distinct.size());
            Set<Config> prevTarget = null;
            State prevState = null;
            int prevLo = 0, prevHi = -2;
            int ei = 0;
            while (ei < events.size()) {
                long pos = events.get(ei)[0];
                while (ei < events.size() && events.get(ei)[0] == pos) {
                    long[] ev = events.get(ei++);
                    if (ev[2] > 0) active.set((int)ev[1]); else active.clear((int)ev[1]);
                }
                if (ei >= events.size() || active.isEmpty()) continue;
                int lo = (int)pos, hi = (int)(events.get(ei)[0] - 1);
                Set<Config> target = closures.get(active);
                if (target == null) {
                    // The signature says for every set, including the negative
                    // lookahead sets, whether it contains the symbols lo..hi, so
                    // lo is a valid representative for advancing the constraints.
                    BitSet signature = (BitSet)active.clone();
                    List<Config> moved = new ArrayList<>();
                    for (int id = signature.nextSetBit(0); id >= 0; id = signature.nextSetBit(id + 1))
                        for (int ci : members.get(id)) {
                            Config m = advance(cs[ci], lo);
                            if (m != null) moved.add(m);
                        }
                    // If every configuration that moves predicts the same
                    // alternative, the target state predicts it, its closure
                    // is not needed. This keeps eg an LL(1) loop exit cheap
                    // even when the follow context is huge. One set per
                    // alternative, so that the ranges share the state.
                    int only = moved.isEmpty() ? -1 : moved.get(0).predicted;
                    for (Config m : moved) if (m.predicted != only) { only = -1; break; }
                    if (only >= 0) {
                        target = decided.get(only);
                        if (target == null) { target = new LinkedHashSet<>(moved); decided.put(only, target); }
                    } else {
                        target = closure(moved);
                    }
                    closures.put(signature, target);
                    work += target.size();
                    if (work > WORK_LIMIT)
                        throw new Failure("too-large", "The lookahead analysis needs too much work, more than "
                                          + WORK_LIMIT + " configurations.");
                }
                if (target == prevTarget && lo == prevHi + 1) {
                    prevHi = hi;
                    continue;
                }
                if (prevTarget != null) state.edges.add(new Edge(prevLo, prevHi, prevState));
                State ts = known.get(target);
                if (ts == null) ts = newState(dfa, known, todo, target, state, lo);
                prevTarget = target; prevState = ts; prevLo = lo; prevHi = hi;
            }
            if (prevTarget != null) state.edges.add(new Edge(prevLo, prevHi, prevState));
            if (dfa.states.size() > STATE_LIMIT)
                throw new Failure("too-large", "The lookahead DFA needs more than " + STATE_LIMIT + " states.");
        }
        return dfa;
    }

    State newState(Dfa dfa, Map<Set<Config>, State> known, Deque<Map.Entry<Set<Config>, State>> todo,
                   Set<Config> configs, State parent, int via) {
        State s = new State(dfa.states.size());
        s.parent = parent;
        s.via = via;
        dfa.states.add(s);
        known.put(configs, s);
        todo.add(Map.entry(configs, s));
        return s;
    }

    /**
     * Two alternatives at the same position with the same stack will never be
     * told apart, the input is ambiguous. If one of them is cheaper, the more
     * expensive configurations are dropped, else this is an ambiguity failure.
     * Returns the configurations that remain.
     */
    Set<Config> resolve(String rule, State state, Set<Config> configs) throws Failure {
        // The cheapest cost for each alternative at each position.
        Map<List<Object>, Map<Integer, Integer>> at = new HashMap<>();
        for (Config c : configs) {
            List<Object> key = Arrays.asList(c.rule, c.alt, c.idx, c.stack, c.neg);
            at.computeIfAbsent(key, k -> new TreeMap<>()).merge(c.predicted, c.cost, Math::min);
        }
        Set<List<Object>> dropped = new HashSet<>();
        for (Map.Entry<List<Object>, Map<Integer, Integer>> e : at.entrySet()) {
            Map<Integer, Integer> m = e.getValue();
            if (m.size() < 2) continue;
            int min = Collections.min(m.values());
            List<Integer> cheapest = new ArrayList<>();
            for (Map.Entry<Integer, Integer> a : m.entrySet()) if (a.getValue() == min) cheapest.add(a.getKey());
            if (cheapest.size() > 1 && !dynamic) {
                boolean exact = true;
                for (Stack i = (Stack)e.getKey().get(3); i != null; i = i.next) if (i == UNKNOWN) exact = false;
                if (!(sllRuntime && exact)) checkConflict(rule, state, cheapest, e.getKey());
            }
            for (Integer a : m.keySet()) if (a != cheapest.get(0)) dropped.add(withAlt(e.getKey(), a));
        }
        if (dropped.isEmpty()) return configs;
        Set<Config> result = new LinkedHashSet<>();
        for (Config c : configs) {
            if (!dropped.contains(withAlt(Arrays.asList(c.rule, c.alt, c.idx, c.stack, c.neg), c.predicted))) result.add(c);
        }
        return result;
    }

    static List<Object> withAlt(List<Object> key, int alt) {
        List<Object> l = new ArrayList<>(key);
        l.add(alt);
        return l;
    }

    void checkConflict(String rule, State state, List<Integer> alternatives, List<Object> key) throws Failure {
        int r = (Integer)key.get(0), alt = (Integer)key.get(1), idx = (Integer)key.get(2);
        int a = alternatives.get(0) + 1, b = alternatives.get(1) + 1;
        String where = r == DONE ? "both accept the input" :
            r == WAIT_EOF ? "both can reach the end of the input" :
            "both continue at " + names[r] + " alternative " + (alt + 1) + " term " + (idx + 1);
        Stack st = (Stack)key.get(3);
        boolean unknown = false;
        for (Stack i = st; i != null; i = i.next) if (i == UNKNOWN) unknown = true;
        if (unknown) {
            // The conflict was found after the analysis lost the outer call context
            // (recursion deeper than the context limit), it can be a false alarm.
            throw new Failure("context-limit", "Alternatives " + a + " and " + b + " could not be told apart, "
                              + where + " after the input \"" + example(state) + "\", but the call context was"
                              + " lost because of recursion deeper than " + CONTEXT_DEPTH
                              + ", the grammar may still be unambiguous.");
        }
        throw new Failure("ambiguous", "Alternatives " + a + " and " + b + " are ambiguous, " + where
                          + " after the input \"" + example(state) + "\".");
    }

    String describe(Config c) {
        StringBuilder b = new StringBuilder();
        b.append("alt ").append(c.predicted + 1).append(": ");
        if (c.rule == WAIT_EOF) b.append("WAIT_EOF"); else if (c.rule == DONE) b.append("DONE");
        else b.append(names[c.rule]).append('/').append(c.alt + 1).append('.').append(c.idx);
        for (Stack s = c.stack; s != null; s = s.next) {
            if (s == UNKNOWN) { b.append(" <- ?"); break; }
            b.append(" <- ").append(names[s.rule]).append('/').append(s.alt + 1).append('.').append(s.idx);
        }
        return b.toString();
    }

    /** The input leading from the DFA start to the state, for diagnostics. */
    static String example(State s) {
        StringBuilder b = new StringBuilder();
        if (s == null) return ""; // Runtime prediction has no DFA state.
        for (State i = s; i.parent != null; i = i.parent) {
            int c = i.via;
            String t;
            if (c == EOF) t = "<EOF>";
            else if (c == '\n') t = "\\n";
            else if (c == '\t') t = "\\t";
            else if (c == '\r') t = "\\r";
            else if (c < 0x20 || (c >= 0x7f && c < 0xa0) || !Character.isDefined(c) || (c >= 0xd800 && c <= 0xdfff) || c > 0xffff)
                t = "#" + Integer.toHexString(c);
            else t = new String(Character.toChars(c));
            b.insert(0, t);
        }
        return b.toString();
    }

    /**
     * Predict the alternative of the decision in rule at runtime, like ALL(*):
     * simulate the ATN from the input position with the actual call stack of
     * the parser (ctx, innermost return position first, null is the start
     * rule's empty context) until only one alternative remains. This is used
     * for decisions that have no static lookahead DFA. Costs resolve
     * conflicts like in the static analysis, a remaining ambiguity picks the
     * first alternative.
     * @param ri The rule index.
     * @param ctx The return stack of the parser.
     * @param input The input code points.
     * @param pos The position of the decision.
     * @return The alternative, or -1 if no alternative matches.
     */
    int predictDynamic(int ri, Stack ctx, int[] input, int pos) {
        dynamic = true;
        try {
            return predictDynamicInternal(ri, ctx, input, pos);
        } catch (Failure f) {
            throw new IllegalStateException("Runtime prediction cannot fail: " + f.getMessage());
        } finally {
            dynamic = false;
        }
    }

    /**
     * A state of the runtime SLL DFA of a decision (built lazily, cached across
     * predictions): predict is the alternative, -1 if more lookahead is needed,
     * -2 if no alternative matches, and conflict means that SLL cannot decide
     * here and the exact LL simulation with the real stack must be used.
     */
    static final class DState {
        Set<Config> configs; // The resolved configurations, the ones that move on.
        int predict = -1;
        boolean conflict;
        final Map<Integer, DState> edges = new HashMap<>();
        DState(Set<Config> configs) { this.configs = configs; }
    }
    private final Map<Integer, Map<Set<Config>, DState>> sllStates = new HashMap<>();
    // The start state per decision and truncated context (rule, stack identity).
    private final Map<Integer, Map<Stack, DState>> sllStart = new HashMap<>();
    private final Map<Integer, DState> sllStartNull = new HashMap<>();
    static final int CTX_K = System.getenv("XMQ_CTX_K") != null ? Integer.parseInt(System.getenv("XMQ_CTX_K")) : 4;

    // The top k return positions of the stack with an UNKNOWN bottom (exact if shallower).
    Stack truncate(Stack ctx, int k) {
        if (ctx == null || ctx == UNKNOWN) return ctx;
        if (k == 0) return UNKNOWN;
        return push(ctx.rule, ctx.alt, ctx.idx, truncate(ctx.next, k - 1));
    }
    static long sllHits, llFallbacks, sllStatesBuilt, sllStateConfigs;

    // Resolve and classify a set of SLL configurations as a DFA state.
    private DState sllState(int ri, Set<Config> configs) {
        Map<Set<Config>, DState> known = sllStates.computeIfAbsent(ri, k -> new HashMap<>());
        DState d = known.get(configs);
        if (d != null) return d;
        d = new DState(configs);
        known.put(configs, d);
        sllStatesBuilt++;
        sllStateConfigs += configs.size();
        Set<Config> resolved;
        try {
            resolved = resolve(names[ri], null, configs);
        } catch (Failure f) {
            d.conflict = true;
            return d;
        }
        int predicted = -1;
        boolean unique = true;
        for (Config c : resolved) {
            if (predicted == -1) predicted = c.predicted;
            else if (predicted != c.predicted) { unique = false; break; }
        }
        if (predicted == -1) d.predict = -2;
        else if (unique) d.predict = predicted;
        d.configs = resolved; // The key set of the cache is not changed.
        return d;
    }

    /**
     * Predict at runtime with the cached SLL DFA, falling back to the exact LL
     * simulation (predictDynamic) when the SLL DFA has a conflict.
     * @param ri The rule index.
     * @param ctx The return stack of the parser, used by the LL fallback.
     * @param input The input code points.
     * @param pos The position of the decision.
     * @return The alternative, or -1 if no alternative matches.
     */
    int predictAdaptive(int ri, Stack ctx, int[] input, int pos) {
        sllRuntime = true;
        try {
            return predictAdaptiveInternal(ri, ctx, input, pos);
        } finally {
            sllRuntime = false;
        }
    }

    private int predictAdaptiveInternal(int ri, Stack ctx, int[] input, int pos) {
        try {
            // The DFA is cached per decision and the top CTX_K frames of the real
            // stack, which removes most conflicts that come from merging call sites.
            Stack tctx = truncate(ctx, CTX_K);
            DState d = tctx == null ? sllStartNull.get(ri) : sllStart.computeIfAbsent(ri, k -> new HashMap<>()).get(tctx);
            if (d == null) {
                List<Config> seeds = new ArrayList<>();
                for (int a = 0; a < alts[ri].length; a++) seeds.add(new Config(ri, a, 0, a, tctx));
                steps = 0;
                d = sllState(ri, closure(seeds));
                if (tctx == null) sllStartNull.put(ri, d);
                else sllStart.get(ri).put(tctx, d);
            }
            for (int p = pos; ; p++) {
                if (d.conflict) break;
                if (d.predict >= 0) { sllHits++; return d.predict; }
                if (d.predict == -2) return -1;
                if (p > input.length) break;
                int sym = p < input.length ? input[p] : EOF;
                DState next = d.edges.get(sym);
                if (next == null) {
                    List<Config> moved = new ArrayList<>();
                    for (Config c : d.configs) {
                        if (matches(c, sym)) {
                            Config m = advance(c, sym);
                            if (m != null) moved.add(m);
                        }
                    }
                    steps = 0;
                    Set<Config> target;
                    try {
                        target = closure(moved);
                    } catch (Failure f) {
                        // Too deep or too large for SLL here, use LL.
                        DState c = new DState(new LinkedHashSet<>());
                        c.conflict = true;
                        d.edges.put(sym, c);
                        break;
                    }
                    next = sllState(ri, target);
                    d.edges.put(sym, next);
                }
                d = next;
            }
        } catch (Failure f) {
            // The start state could not be built, use LL.
        }
        llFallbacks++;
        sllRuntime = false;
        return predictDynamic(ri, ctx, input, pos);
    }

    static final boolean DYN_STATS = System.getenv("XMQ_DYN_STATS") != null;
    static long dynCalls, dynSymbols, dynConfigs;
    static final Map<String, long[]> dynPerRule = new TreeMap<>();
    static {
        if (DYN_STATS) Runtime.getRuntime().addShutdownHook(new Thread(() -> {
            System.err.println("sll hits " + sllHits + " ll fallbacks " + llFallbacks + " sll states " + sllStatesBuilt + " sll state configs " + sllStateConfigs);
            System.err.println("dynamic predictions " + dynCalls + " symbols " + dynSymbols + " configs " + dynConfigs);
            dynPerRule.entrySet().stream().sorted((a, b) -> Long.compare(b.getValue()[2], a.getValue()[2])).limit(15)
                .forEach(e -> System.err.printf("  %-30s calls %8d symbols %9d configs %11d%n", e.getKey(), e.getValue()[0], e.getValue()[1], e.getValue()[2]));
        }));
    }

    private int predictDynamicInternal(int ri, Stack ctx, int[] input, int pos) throws Failure {
        long[] st = DYN_STATS ? dynPerRule.computeIfAbsent(names[ri], k -> new long[3]) : null;
        if (DYN_STATS) { dynCalls++; st[0]++; }
        {
            List<Config> seeds = new ArrayList<>();
            for (int a = 0; a < alts[ri].length; a++) seeds.add(new Config(ri, a, 0, a, ctx));
            Set<Config> configs = closure(seeds);
            for (int p = pos; ; p++) {
                configs = resolve(names[ri], null, configs);
                int predicted = -1;
                boolean unique = true;
                for (Config c : configs) {
                    if (predicted == -1) predicted = c.predicted;
                    else if (predicted != c.predicted) { unique = false; break; }
                }
                if (predicted == -1) return -1;
                if (unique) return predicted;
                if (p > input.length) {
                    // All input including EOF consumed and still ambiguous, pick the first.
                    int min = Integer.MAX_VALUE;
                    for (Config c : configs) min = Math.min(min, c.predicted);
                    return min;
                }
                int sym = p < input.length ? input[p] : EOF;
                List<Config> moved = new ArrayList<>();
                for (Config c : configs) {
                    if (matches(c, sym)) {
                        Config m = advance(c, sym);
                        if (m != null) moved.add(m);
                    }
                }
                configs = closure(moved);
                if (DYN_STATS) { dynSymbols++; dynConfigs += configs.size(); st[1]++; st[2] += configs.size(); }
            }
        }
    }

    /** The rules that are decisions, ie have more than one alternative. */
    List<String> decisions(Collection<String> reachable) {
        List<String> l = new ArrayList<>();
        for (String r : reachable) if (alts[index.get(r)].length > 1) l.add(r);
        return l;
    }
}
