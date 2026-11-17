/* libxmq - Copyright (C) 2026 Fredrik Öhrström (spdx: MIT) */
package org.libxmq.imp.ixml.earley;

import org.libxmq.imp.ixml.earley.Grammar.Call;
import org.libxmq.imp.ixml.earley.Grammar.Chars;
import org.libxmq.imp.ixml.earley.Grammar.Production;
import org.libxmq.imp.ixml.earley.Grammar.Rule;
import org.libxmq.imp.ixml.earley.Grammar.Symbol;

/**
 * A plain Earley recognizer.
 *
 * The parser builds one Earley set for each input position. Set k is filled
 * by three operations, applied to every item in it until nothing new is added:
 *
 * - Predict:  the item expects a rule B, add (B → · ..., k) for every
 *             production of B: B may start here.
 * - Scan:     the item expects a terminal that matches the code point at k,
 *             add the item with the dot moved over it to set k+1.
 * - Complete: the item is complete, a rule A matched from its origin j to k.
 *             Every item in set j that was waiting for A moves its dot over A
 *             and is added to set k.
 *
 * The input is accepted when the last set has a complete start rule item with
 * origin 0.
 *
 * An empty match (a nullable rule) completes in the same set it was predicted
 * in, which plain Earley can miss when the waiting item is added after the
 * completion. So, as Aycock and Horspool suggest, when an item expects a
 * nullable rule, the dot is also moved over the rule right away.
 *
 * Insertions and nots match no input, the dot is moved over them directly.
 * A not is not checked here, see removeForbiddenByNots, which runs on the
 * finished chart before the tree is built.
 */
public final class EarleyParser
{
    private final Grammar grammar;

    /** Create a parser for the grammar.
     * @param grammar the grammar */
    public EarleyParser(Grammar grammar)
    {
        this.grammar = grammar;
    }

    /** Run the recognizer over the input.
     * @param input the input as code points
     * @return the chart, check accepted() for the result */
    public Chart recognize(int[] input)
    {
        Chart chart = new Chart(input);

        // The start rule may begin at position 0.
        for (Production p : grammar.start.productions)
        {
            chart.set(0).add(new Item(p, 0, 0));
        }

        for (int k = 0; k <= input.length; k++)
        {
            EarleySet set = chart.set(k);
            // The set grows while we work through it, new items are handled too.
            for (int i = 0; i < set.size(); i++)
            {
                Item item = set.get(i);
                Symbol next = item.next();
                if (next == null) complete(chart, item, k);
                else if (next instanceof Call call) predict(chart, item, call.rule(), k);
                else if (next instanceof Chars chars) scan(chart, item, chars, k);
                else set.add(item.advance()); // An insertion or a not, matches nothing.
            }
        }
        return chart;
    }

    /** Returns true if the chart accepts the whole input: the last set has a
     * complete item of the start rule that began at position 0.
     * @param chart the chart from recognize
     * @return true if accepted */
    public boolean accepted(Chart chart)
    {
        for (Item item : chart.set(chart.length()).items())
        {
            if (item.isComplete() && item.origin() == 0 && item.production().rule == grammar.start) return true;
        }
        return false;
    }

    /** The position of the first code point that could not be parsed: the last
     * set that has items. If all sets have items, the input ended too early
     * and the position is the input length.
     * @param chart the chart from recognize
     * @return the input position */
    public int failurePosition(Chart chart)
    {
        for (int k = 0; k < chart.length(); k++)
        {
            if (chart.set(k + 1).isEmpty()) return k;
        }
        return chart.length();
    }

    /**
     * Remove the items that a not forbids, from the finished chart, before the
     * tree is built.
     *
     * Not implemented yet. An item that moved its dot over a not !x at position
     * k is forbidden if the input at k starts with x. Removing such items, and
     * the items that were made from them, leaves only the derivations that
     * respect the nots. Until then the nots are ignored, as if they were not
     * there.
     *
     * @param chart the chart from recognize
     */
    public void removeForbiddenByNots(Chart chart)
    {
        // To be implemented.
    }

    /** The item expects the rule: the rule may start here. */
    private void predict(Chart chart, Item item, Rule rule, int k)
    {
        EarleySet set = chart.set(k);
        for (Production p : rule.productions)
        {
            set.add(new Item(p, 0, k));
        }
        // An empty match of the rule completes here, move over it now (Aycock and Horspool).
        if (grammar.isNullable(rule)) set.add(item.advance());
    }

    /** The item expects a terminal: if it matches the code point at k, it continues in set k+1. */
    private static void scan(Chart chart, Item item, Chars chars, int k)
    {
        if (k < chart.length() && chars.matches(chart.input[k]))
        {
            chart.set(k + 1).add(item.advance());
        }
    }

    /** The item is complete: its rule matched from the origin to k. The items
     * that waited for the rule at the origin move over it. */
    private static void complete(Chart chart, Item item, int k)
    {
        Rule rule = item.production().rule;
        EarleySet origin = chart.set(item.origin());
        // The origin set can be the set being filled (an empty match), so loop with an index.
        for (int i = 0; i < origin.size(); i++)
        {
            Item waiting = origin.get(i);
            if (waiting.next() instanceof Call call && call.rule() == rule)
            {
                chart.set(k).add(waiting.advance());
            }
        }
    }
}
