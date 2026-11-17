/* libxmq - Copyright (C) 2026 Fredrik Öhrström (spdx: MIT) */
package org.libxmq.imp.ixml.earley;

import org.libxmq.imp.ixml.earley.Grammar.Call;
import org.libxmq.imp.ixml.earley.Grammar.Chars;
import org.libxmq.imp.ixml.earley.Grammar.Insertion;
import org.libxmq.imp.ixml.earley.Grammar.Not;
import org.libxmq.imp.ixml.earley.Grammar.Production;
import org.libxmq.imp.ixml.earley.Grammar.Symbol;

/**
 * An Earley item: a production with a dot showing how much of it has been
 * matched, and the origin, the input position where the match started.
 *
 * The item (S → S · '+' M, 0) in set 1 means: starting at position 0, the
 * input up to position 1 matches S, and now a '+' is expected.
 *
 * @param production the production being matched
 * @param dot how many symbols of the production have been matched
 * @param origin the input position where the production started
 */
public record Item(Production production, int dot, int origin)
{
    /** Returns true if all symbols have been matched.
     * @return true if the dot is at the end */
    public boolean isComplete()
    {
        return dot == production.symbols.size();
    }

    /** The symbol after the dot, the one expected next.
     * @return the symbol, or null if the item is complete */
    public Symbol next()
    {
        return isComplete() ? null : production.symbols.get(dot);
    }

    /** The same item with the dot moved over the next symbol.
     * @return the advanced item */
    public Item advance()
    {
        return new Item(production, dot + 1, origin);
    }

    @Override
    public String toString()
    {
        return "(" + show(production, dot) + ", " + origin + ")";
    }

    /** Write a production as rule → a b c, with a · at the dot (no · if dot is negative). */
    static String show(Production p, int dot)
    {
        StringBuilder b = new StringBuilder(p.rule.name).append(" →");
        for (int i = 0; i < p.symbols.size(); i++)
        {
            if (i == dot) b.append(" ·");
            b.append(' ').append(show(p.symbols.get(i)));
        }
        if (dot == p.symbols.size()) b.append(" ·");
        return b.toString();
    }

    static String show(Symbol s)
    {
        if (s instanceof Call c) return (c.mark() != 0 ? Character.toString(c.mark()) : "") + c.rule().name;
        if (s instanceof Chars c) return (c.hidden() ? "-" : "") + c.text();
        if (s instanceof Insertion i) return "+'" + i.text() + "'";
        Not n = (Not)s;
        StringBuilder b = new StringBuilder("!");
        for (var set : n.forbidden()) b.append(Grammar.describe(set));
        return b.toString();
    }
}
