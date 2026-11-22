/* libxmq - Copyright (C) 2026 Fredrik Öhrström (spdx: MIT) */
package org.libxmq.imp.ixml.earley;

import java.util.ArrayList;
import java.util.BitSet;
import java.util.Collections;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

import org.libxmq.imp.ixml.IXMLGrammar;

/**
 * The ixml grammar as the Earley parser sees it: rules with productions,
 * where each production is a sequence of symbols.
 *
 * The grammar is read by IXMLGrammar, which has already rewritten the ixml
 * repetitions, options and groups into hidden generated rules (/1, /2, ...).
 * So here there are only four kinds of symbols:
 *
 * - Call     a nonterminal, a call of another rule, eg the M in S = M, '+', M.
 * - Chars    a terminal, matches one code point from a set, eg '+' or [L].
 * - Insertion  +'text' inserts text into the output and matches nothing.
 * - Not      a negative lookahead !'x', matches nothing (see EarleyParser).
 *
 * Insertion and Not are zero width: the parser steps over them without
 * reading input.
 */
public final class Grammar
{
    /** A rule: the left hand side of its productions. */
    public static final class Rule
    {
        /** The rule name, eg S, or /1 for a generated rule. */
        public final String name;
        /** The mark of the rule: '^' element, '@' attribute, '-' hidden, or 0 for none. */
        public final char mark;
        /** The name used in the output, rule>alias, or the rule name. */
        public final String output_name;
        /** The cost of the rule, the number of &lt; after = or :, lower is preferred. */
        public final int cost;
        /** The productions of the rule, one for each alternative. */
        public final List<Production> productions = new ArrayList<>();
        /** The named rule: the name, or for a generated rule the named rule it was made for,
         *  used in error messages and completion. */
        public final String named;
        /** True if the rule can match the empty input. */
        boolean nullable;

        Rule(String name, char mark, String output_name, int cost, String named)
        {
            this.name = name;
            this.mark = mark;
            this.output_name = output_name;
            this.cost = cost;
            this.named = named;
        }

        @Override
        public String toString()
        {
            return name;
        }
    }

    /** One alternative of a rule: rule → symbol symbol ... */
    public static final class Production
    {
        /** The rule this is an alternative of. */
        public final Rule rule;
        /** The symbols, in order. An empty list is the empty alternative. */
        public final List<Symbol> symbols;
        /** The production number in the grammar, used to print and order productions. */
        public final int number;

        Production(Rule rule, List<Symbol> symbols, int number)
        {
            this.rule = rule;
            this.symbols = Collections.unmodifiableList(symbols);
            this.number = number;
        }

        @Override
        public String toString()
        {
            return Item.show(this, -1);
        }
    }

    /** A symbol in a production. */
    public sealed interface Symbol permits Call, Chars, Insertion, Not
    {
        /** Returns true if the symbol matches nothing: the parser steps over it.
         * @return true for Insertion and Not */
        default boolean isZeroWidth()
        {
            return this instanceof Insertion || this instanceof Not;
        }
    }

    /** A call of a rule. The mark and alias of the call override those of the rule.
     * @param rule the called rule
     * @param mark '^', '@', '-' or 0 for no mark on the call
     * @param alias the alias of the call, call>alias, or null */
    public record Call(Rule rule, char mark, String alias) implements Symbol
    {
        /** The mark used in the output: the mark of the call, else the mark of the rule, else '^'.
         * @return '^', '@' or '-' */
        public char effectiveMark()
        {
            if (mark != 0) return mark;
            if (rule.mark != 0) return rule.mark;
            return '^';
        }

        /** The name used in the output: the alias of the call, else the alias or the name of the rule.
         * @return the name */
        public String effectiveName()
        {
            return alias != null ? alias : rule.output_name;
        }
    }

    /** A terminal: one code point from a set. A hidden terminal, -'x', is not output.
     * @param set the code points matched
     * @param hidden true if the terminal is hidden
     * @param text how the terminal is written in the grammar, eg 'a' or ['a'-'z'; #a]
     * @param tokens the tokens matched (see Tokens), one bit per token: the token of a literal,
     *        or for a charset the tokens of the code points in set that have been seen in the
     *        inputs so far. Shared by the terminals with the same set. */
    public record Chars(BitSet set, boolean hidden, String text, BitSet tokens) implements Symbol
    {
        /** Returns true if the code point is in the set.
         * @param codepoint the code point
         * @return true on a match */
        public boolean matches(int codepoint)
        {
            return set.get(codepoint);
        }

        /** Returns true if the token is matched.
         * @param token the token
         * @return true on a match */
        public boolean matchesToken(int token)
        {
            return tokens.get(token);
        }
    }

    /** An insertion, +'text': outputs the text, matches no input.
     * @param text the inserted text */
    public record Insertion(String text) implements Symbol { }

    /** A negative lookahead, !'abc' or ![set]: the input that follows must not
     * start with code points from these sets, in order. Matches no input.
     * @param forbidden the sets of the forbidden sequence */
    public record Not(List<BitSet> forbidden) implements Symbol { }

    /** The start rule. */
    public final Rule start;
    /** All rules by name, in grammar order. */
    public final Map<String, Rule> rules = new LinkedHashMap<>();
    /** All productions, numbered from 0. */
    public final List<Production> productions = new ArrayList<>();
    /** The tokens: the code points of the literals of the grammar and of the inputs parsed so far. */
    public final Tokens tokens = new Tokens();
    /** The token sets of the charsets (the terminals that match more than one code point),
     * by their code point set, in grammar order. A new code point of an input is added to
     * the token sets of the charsets that contain it, see addUsed. */
    private final Map<BitSet, BitSet> charset_tokens = new LinkedHashMap<>();

    /** Build the Earley grammar from an ixml grammar.
     * @param ixml the grammar read by IXMLGrammar (without left recursion elimination)
     */
    public Grammar(IXMLGrammar ixml)
    {
        // First create all rules, so that calls can refer to rules defined later.
        for (IXMLGrammar.Rule r : ixml.rules.values())
        {
            String output_name = r.alias.isEmpty() ? r.name : r.alias;
            String named = r.origin != null ? r.origin : r.name;
            rules.put(r.name, new Rule(r.name, markOf(r.mark), output_name, r.cost, named));
        }
        // Then the productions.
        for (IXMLGrammar.Rule r : ixml.rules.values())
        {
            Rule rule = rules.get(r.name);
            for (List<IXMLGrammar.Term> alternative : r.alternatives)
            {
                List<Symbol> symbols = new ArrayList<>();
                for (IXMLGrammar.Term t : alternative) symbols.add(symbolOf(t));
                Production p = new Production(rule, symbols, productions.size());
                rule.productions.add(p);
                productions.add(p);
            }
        }
        start = rules.get(ixml.start);
        computeNullable();
    }

    /** Returns true if the rule can match the empty input.
     * @param rule the rule
     * @return true if nullable */
    public boolean isNullable(Rule rule)
    {
        return rule.nullable;
    }

    private Symbol symbolOf(IXMLGrammar.Term t)
    {
        if (t.ref != null) return new Call(rules.get(t.ref), markOf(t.mark), t.alias.isEmpty() ? null : t.alias);
        if (t.chars != null)
        {
            String text = t.chars_text != null ? t.chars_text : describe(t.chars);
            return new Chars(t.chars, t.mark.equals("-"), text, tokensOf(t.chars));
        }
        if (t.insertion != null) return new Insertion(t.insertion);
        return new Not(t.not);
    }

    /** The token set of a terminal. A literal, a single code point, gets its token now, like
     * the terminals of the grammar in yaep. A charset gets an empty token set that grows with
     * the code points of the inputs. */
    private BitSet tokensOf(BitSet set)
    {
        int first = set.nextSetBit(0);
        if (first >= 0 && set.nextSetBit(first + 1) < 0)
        {
            BitSet b = new BitSet();
            b.set(tokens.tokenOrAdd(first));
            return b;
        }
        return charset_tokens.computeIfAbsent(set, s -> new BitSet());
    }

    /** Add the code points that are new in an input (seen for the first time by this grammar)
     * to the token sets of the charsets that contain them, like scan_content_fixup_charsets in C.
     * Each new code point is tested against each charset, the work is proportional to the new
     * code points, not to the size of the charsets.
     * @param codes the new code points, they have tokens
     * @param count the number of new code points */
    void addUsed(int[] codes, int count)
    {
        if (count == 0) return;
        for (Map.Entry<BitSet, BitSet> e : charset_tokens.entrySet())
        {
            BitSet set = e.getKey(), toks = e.getValue();
            for (int i = 0; i < count; i++)
            {
                if (set.get(codes[i])) toks.set(tokens.token(codes[i]));
            }
        }
    }

    private static char markOf(String mark)
    {
        return mark.isEmpty() ? 0 : mark.charAt(0);
    }

    /** A rule is nullable if one of its productions has only nullable symbols.
     * Repeat until nothing changes, since a rule can depend on rules after it. */
    private void computeNullable()
    {
        boolean changed = true;
        while (changed)
        {
            changed = false;
            for (Production p : productions)
            {
                if (p.rule.nullable) continue;
                if (p.symbols.stream().allMatch(this::isNullable))
                {
                    p.rule.nullable = true;
                    changed = true;
                }
            }
        }
    }

    private boolean isNullable(Symbol s)
    {
        if (s instanceof Call c) return c.rule().nullable;
        return s.isZeroWidth();
    }

    /** Write a character set the way the C trace does, eg '+' or ['0'-'9' 'a']. */
    static String describe(BitSet set)
    {
        if (set.cardinality() == 1) return quote(set.nextSetBit(0));
        StringBuilder b = new StringBuilder("[");
        int count = 0;
        for (int lo = set.nextSetBit(0); lo >= 0 && count < 8; count++)
        {
            int hi = set.nextClearBit(lo) - 1;
            if (b.length() > 1) b.append(' ');
            b.append(quote(lo));
            if (hi > lo) b.append('-').append(quote(hi));
            lo = set.nextSetBit(hi + 1);
        }
        if (count == 8) b.append(" ...");
        return b.append(']').toString();
    }

    static String quote(int codepoint)
    {
        if (codepoint < 0x21 || codepoint == 0x7f) return "#" + Integer.toHexString(codepoint);
        return "'" + new String(Character.toChars(codepoint)) + "'";
    }
}
