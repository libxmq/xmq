/* libxmq - Copyright (C) 2026 Fredrik Öhrström (spdx: MIT) */
package org.libxmq.imp.ixml.earley;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

import org.libxmq.imp.ixml.earley.Grammar.Call;
import org.libxmq.imp.ixml.earley.Grammar.Chars;
import org.libxmq.imp.ixml.earley.Grammar.Production;
import org.libxmq.imp.ixml.earley.Grammar.Rule;
import org.libxmq.imp.ixml.earley.Grammar.Symbol;

/**
 * Compose the parse tree from a chart that accepted the input.
 *
 * The chart only says which items were alive where. The tree is found by
 * working backwards from the complete start item in the last set:
 *
 * A rule A matched the input from..to if the set at to has the complete item
 * (A → x y z ·, from). To find its children, take the symbols from the last
 * one backwards. The last symbol z ends at to. If z is a terminal it starts at
 * to-1. If z is a rule it starts at some mid, where z matched mid..to, and the
 * symbols before it, x y, matched from..mid. That is true exactly when the set
 * at mid has the item (A → x y · z, from). Then continue with y, ending at mid.
 *
 * When there is more than one way, a production or a split point, the one with
 * the lowest cost wins. The cost of a tree is the sum of the costs of its
 * rules (ixml rule =&lt; ...). If two ways have the same lowest cost the tree is
 * ambiguous, and the first one found is used: productions in grammar order
 * and, for a split, the shortest match of the earlier symbols.
 *
 * A grammar with a cycle, eg A = A | 'a', has infinitely many trees. A rule
 * that is already being composed over the same span is skipped, so the trees
 * without the cycle remain.
 *
 * The results are remembered, each rule over each span is composed once.
 */
public final class TreeBuilder
{
    private final Chart chart;
    private final Grammar grammar;

    /** A rule over a span of the input. */
    private record RuleSpan(Rule rule, int from, int to) { }
    /** The first count symbols of a production over a span of the input. */
    private record PrefixSpan(Production production, int count, int from, int to) { }

    /** The best tree for a rule over a span, absent if not composed yet, null if none exists. */
    private final Map<RuleSpan, Tree> best_rule = new HashMap<>();
    /** The best children for a production prefix over a span, null if none exists. */
    private final Map<PrefixSpan, Children> best_prefix = new HashMap<>();
    /** The rules being composed right now, to break cycles. */
    private final Set<RuleSpan> in_progress = new HashSet<>();

    /** The children found for a prefix of a production: the trees, their total cost,
     * and whether a tie was found. */
    private record Children(List<Tree> trees, int cost, boolean ambiguous)
    {
        static final Children NONE = new Children(List.of(), 0, false);

        Children plus(Tree t)
        {
            List<Tree> l = new ArrayList<>(trees);
            l.add(t);
            return new Children(l, cost + t.cost(), ambiguous || t.ambiguous());
        }
    }

    /** Create a builder for an accepted chart.
     * @param chart the chart, after EarleyParser.removeForbiddenByNots
     * @param grammar the grammar */
    public TreeBuilder(Chart chart, Grammar grammar)
    {
        this.chart = chart;
        this.grammar = grammar;
    }

    /** Compose the tree of the whole input.
     * @return the tree of the start rule, or null if there is none */
    public Tree build()
    {
        return best(grammar.start, 0, chart.length());
    }

    /** The best tree for the rule matching from..to, or null. */
    private Tree best(Rule rule, int from, int to)
    {
        RuleSpan key = new RuleSpan(rule, from, to);
        if (best_rule.containsKey(key)) return best_rule.get(key);
        if (!in_progress.add(key)) return null; // A cycle, try the other ways.

        Tree best = null;
        boolean tie = false;
        for (Production p : rule.productions)
        {
            int n = p.symbols.size();
            if (!chart.set(to).contains(new Item(p, n, from))) continue;
            Children c = prefix(p, n, from, to);
            if (c == null) continue;
            Tree candidate = Tree.rule(p, c.trees(), from, to, rule.cost + c.cost(), c.ambiguous());
            if (best == null || candidate.cost() < best.cost())
            {
                best = candidate;
                tie = false;
            }
            else if (candidate.cost() == best.cost())
            {
                tie = true;
            }
        }
        if (best != null && tie) best = Tree.rule(best.production(), best.children(), from, to, best.cost(), true);

        in_progress.remove(key);
        best_rule.put(key, best);
        return best;
    }

    /** The best children for the first count symbols of the production matching
     * from..to, or null. The set at to has the item (p, count, from). */
    private Children prefix(Production p, int count, int from, int to)
    {
        if (count == 0) return from == to ? Children.NONE : null;
        PrefixSpan key = new PrefixSpan(p, count, from, to);
        if (best_prefix.containsKey(key)) return best_prefix.get(key);

        Symbol last = p.symbols.get(count - 1);
        Item before = new Item(p, count - 1, from); // The item before the dot moved over last.
        Children best = null;

        if (last.isZeroWidth())
        {
            // An insertion or a not, it ends where it starts.
            if (chart.set(to).contains(before))
            {
                Children c = prefix(p, count - 1, from, to);
                if (c != null) best = c.plus(Tree.zeroWidth(last, to));
            }
        }
        else if (last instanceof Chars chars)
        {
            // A terminal matched the code point just before to.
            int at = to - 1;
            if (at >= from && chars.matches(chart.input[at]) && chart.set(at).contains(before))
            {
                Children c = prefix(p, count - 1, from, at);
                if (c != null) best = c.plus(Tree.character(last, chart.input[at], at));
            }
        }
        else
        {
            // A rule matched mid..to, try each mid, shortest earlier symbols first.
            Rule called = ((Call)last).rule();
            boolean tie = false;
            for (int mid = from; mid <= to; mid++)
            {
                if (!chart.set(mid).contains(before)) continue;
                Tree child = best(called, mid, to);
                if (child == null) continue;
                Children c = prefix(p, count - 1, from, mid);
                if (c == null) continue;
                Children candidate = c.plus(child.as(last));
                if (best == null || candidate.cost() < best.cost())
                {
                    best = candidate;
                    tie = false;
                }
                else if (candidate.cost() == best.cost())
                {
                    tie = true;
                }
            }
            if (best != null && tie) best = new Children(best.trees(), best.cost(), true);
        }

        best_prefix.put(key, best);
        return best;
    }
}
