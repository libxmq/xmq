/* libxmq - Copyright (C) 2026 Fredrik Öhrström (spdx: MIT) */
package org.libxmq.imp.ixml.earley;

import java.util.List;

import org.libxmq.imp.ixml.earley.Grammar.Production;
import org.libxmq.imp.ixml.earley.Grammar.Symbol;

/**
 * A node in the parse tree chosen by TreeBuilder.
 *
 * - A rule node has the production used and one child per symbol of it.
 * - A character node is a terminal that matched the code point.
 * - A zero width node is an insertion or a not.
 *
 * The symbol is the one in the parent production that this node matched,
 * for a rule node the Call with its mark and alias. The root has no symbol.
 *
 * @param symbol the symbol in the parent production, null for the root
 * @param production the production of a rule node, else null
 * @param children the children of a rule node, else empty
 * @param codepoint the code point of a character node, else -1
 * @param from the input position where the node starts
 * @param to the input position after the node
 * @param cost the sum of the rule costs in the tree, lower is preferred
 * @param ambiguous true if another tree with the same cost was possible
 */
public record Tree(Symbol symbol, Production production, List<Tree> children, int codepoint,
                   int from, int to, int cost, boolean ambiguous)
{
    /** A rule node.
     * @param production the production
     * @param children one child per symbol
     * @param from start position
     * @param to end position
     * @param cost the total cost
     * @param ambiguous true if a tie was found
     * @return the node, without a symbol */
    static Tree rule(Production production, List<Tree> children, int from, int to, int cost, boolean ambiguous)
    {
        return new Tree(null, production, children, -1, from, to, cost, ambiguous);
    }

    /** A character node.
     * @param symbol the terminal
     * @param codepoint the matched code point
     * @param at the input position
     * @return the node */
    static Tree character(Symbol symbol, int codepoint, int at)
    {
        return new Tree(symbol, null, List.of(), codepoint, at, at + 1, 0, false);
    }

    /** A node for an insertion or a not.
     * @param symbol the symbol
     * @param at the input position
     * @return the node */
    static Tree zeroWidth(Symbol symbol, int at)
    {
        return new Tree(symbol, null, List.of(), -1, at, at, 0, false);
    }

    /** The same node, as matched by the symbol in the parent production.
     * @param s the symbol
     * @return the node with the symbol */
    Tree as(Symbol s)
    {
        return new Tree(s, production, children, codepoint, from, to, cost, ambiguous);
    }

    /** Returns true if this is a rule node.
     * @return true if the node has a production */
    public boolean isRule()
    {
        return production != null;
    }
}
