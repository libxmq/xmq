/* libxmq - Copyright (C) 2026 Fredrik Öhrström (spdx: MIT) */
package org.libxmq.imp.ixml.earley;

import java.io.PrintStream;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

import org.libxmq.imp.ixml.earley.Grammar.Call;
import org.libxmq.imp.ixml.earley.Grammar.Chars;
import org.libxmq.imp.ixml.earley.Grammar.Production;
import org.libxmq.imp.ixml.earley.Grammar.Symbol;
import org.libxmq.imp.xml.XmlDocument;
import org.libxmq.imp.xml.XmlElement;
import org.libxmq.imp.xml.XmlText;

/**
 * Record what the Earley parser did: as a trace document (toXml), the same as
 * xmq --traceixml=file records, or (write) in the same line format as the C parser
 * (xmq --debug=ixml.), so that OpenMike/parse-trace.ixml and
 * OpenMike/render-trace.xslq can visualize it, see OpenMike/debug-ixml-java.sh.
 *
 * The lines are written from the finished chart and the chosen tree:
 *
 *   (ixml.gr) S → S '+' M                  a production of the grammar
 *   (ixml.pa.step) @2 (d5,2-2) none/8 M →  · T   an item in set 2
 *   (ixml.pa.token) @2 '2'                 the code point at position 2
 *   (ixml.pa.view) @2 READ '2'             the parser reads it
 *   (ixml.bt.step) adding (d12,2-3) M →  T  a node added to the tree
 *
 * An item is written as (dN,from-to) where dN numbers the dotted rule, the
 * production with the dot, in the order they first appear. Then the kind:
 * none (nothing matched yet, a prediction), part (partly matched) or full
 * (complete), and n, the number of tokens. Like in C the input ends with an
 * extra token ω (end of input), so n is the input length plus one.
 *
 * The generated rules /1, /2, ... are written |1, |2, ... like in C.
 */
public final class EarleyTrace
{
    private final PrintStream out;
    private final Map<Item, Integer> dotted_rule_ids = new HashMap<>();
    private int num_tokens;

    private EarleyTrace(PrintStream out)
    {
        this.out = out;
    }

    /** Write the trace.
     * @param out the stream, eg stderr
     * @param grammar the grammar
     * @param chart the chart from EarleyParser.recognize
     * @param tree the chosen tree, or null if the input was rejected
     * @param steps the nodes in the order they were added to the tree
     */
    public static void write(PrintStream out, Grammar grammar, Chart chart, Tree tree, List<TreeBuilder.Step> steps)
    {
        EarleyTrace t = new EarleyTrace(out);
        t.num_tokens = chart.length() + 1;
        for (Production p : grammar.productions) out.println("(ixml.gr) " + t.ruleText(p, -1));

        out.println("(ixml.pa.debug) @0 recognize()");
        t.writeSet(chart, 0);
        for (int k = 0; k < chart.length(); k++)
        {
            String token = Grammar.quote(chart.input[k]);
            out.println("(ixml.pa.token) @" + k + " " + token);
            out.println("(ixml.pa.view) @" + k + " READ " + token);
            t.writeSet(chart, k + 1);
        }
        out.println("(ixml.pa.token) @" + chart.length() + " ω");
        out.println("(ixml.pa.view) @" + chart.length() + " READ ω");

        if (tree == null) return;
        out.println("(ixml.pa.debug) @" + chart.length() + " build_tree()");
        for (TreeBuilder.Step s : steps)
        {
            Production p = s.production();
            Item complete = new Item(p, p.symbols.size(), s.from());
            out.println("(ixml.bt.step) adding " + t.itemId(complete, s.from(), s.to()) + " " + t.ruleText(p, -1));
        }
    }

    /** The trace as a document, the same as the C version records (xmq --traceixml=file):
     * <pre>
     * trace(engine=earley) {
     *     grammar { rule(name=S rhs="S '+' M") ... }
     *     input { token(pos=0) = 1 ... token(pos=7) = ω }
     *     steps { item(id from to state rule matched expected) ... read(pos=0) ... }
     *     tree { node(step parent id from to rule rhs) ... } }
     * </pre>
     * where state is predicted, partial or complete. The items of set k+1 follow read(pos=k).
     * The tree nodes are in the order they were added, step 1, 2, ..., and parent is
     * the step of the parent node, 0 for the root.
     * @param grammar the grammar
     * @param chart the chart from EarleyParser.recognize
     * @param tree_steps the nodes in the order they were added to the tree, empty if the input was rejected
     * @return the trace document
     */
    public static XmlDocument toXml(Grammar grammar, Chart chart, List<TreeBuilder.Step> tree_steps)
    {
        EarleyTrace t = new EarleyTrace(null);
        XmlElement root = new XmlElement("trace");
        root.setAttribute("engine", "earley");
        XmlElement rules = child(root, "grammar");
        XmlElement input = child(root, "input");
        XmlElement steps = child(root, "steps");
        XmlElement nodes = child(root, "tree");

        for (Production p : grammar.productions)
        {
            XmlElement rule = child(rules, "rule");
            rule.setAttribute("name", traceName(p.rule.name));
            rule.setAttribute("rhs", t.symbolsText(p, 0, p.symbols.size()));
        }
        for (int k = 0; k <= chart.length(); k++)
        {
            String token = k < chart.length() ? tokenText(chart.input[k]) : "ω";
            XmlElement e = child(input, "token");
            e.setAttribute("pos", Integer.toString(k));
            e.appendRaw(new XmlText(token));
        }
        t.addItems(steps, chart, 0);
        for (int k = 0; k <= chart.length(); k++)
        {
            child(steps, "read").setAttribute("pos", Integer.toString(k));
            if (k < chart.length()) t.addItems(steps, chart, k + 1);
        }
        for (TreeBuilder.Step s : tree_steps) t.addNode(nodes, s);

        XmlDocument d = new XmlDocument();
        d.appendRaw(root);
        return d;
    }

    private void addItems(XmlElement steps, Chart chart, int k)
    {
        for (Item item : chart.set(k).items())
        {
            XmlElement e = child(steps, "item");
            String id = itemId(item, item.origin(), k);
            e.setAttribute("id", id.substring(2, id.indexOf(',')));
            e.setAttribute("from", Integer.toString(item.origin()));
            e.setAttribute("to", Integer.toString(k));
            e.setAttribute("state", item.isComplete() ? "complete" : item.dot() == 0 ? "predicted" : "partial");
            e.setAttribute("rule", traceName(item.production().rule.name));
            e.setAttribute("matched", symbolsText(item.production(), 0, item.dot()));
            e.setAttribute("expected", symbolsText(item.production(), item.dot(), item.production().symbols.size()));
        }
    }

    /** A node added to the tree. */
    private void addNode(XmlElement nodes, TreeBuilder.Step s)
    {
        Production p = s.production();
        String id = itemId(new Item(p, p.symbols.size(), s.from()), s.from(), s.to());
        XmlElement e = child(nodes, "node");
        e.setAttribute("step", Integer.toString(s.step()));
        e.setAttribute("parent", Integer.toString(s.parent()));
        e.setAttribute("id", id.substring(2, id.indexOf(',')));
        e.setAttribute("from", Integer.toString(s.from()));
        e.setAttribute("to", Integer.toString(s.to()));
        e.setAttribute("rule", traceName(p.rule.name));
        e.setAttribute("rhs", symbolsText(p, 0, p.symbols.size()));
    }

    private static XmlElement child(XmlElement parent, String name)
    {
        XmlElement e = new XmlElement(name);
        parent.appendRaw(e);
        return e;
    }

    /** A token without the quotes, eg 1 for '1', but #a for a newline, like the C trace. */
    private static String tokenText(int codepoint)
    {
        String q = Grammar.quote(codepoint);
        return q.startsWith("'") ? q.substring(1, q.length() - 1) : q;
    }

    /** The symbols from index from to index to of the production as text, eg S '+' M. */
    private String symbolsText(Production p, int from, int to)
    {
        StringBuilder b = new StringBuilder();
        for (int i = from; i < to; i++)
        {
            if (i > from) b.append(' ');
            b.append(symbolText(p.symbols.get(i)));
        }
        return b.toString();
    }

    /** Write the items of set k, in the order they were added. */
    private void writeSet(Chart chart, int k)
    {
        for (Item item : chart.set(k).items())
        {
            String kind = item.dot() == 0 && !item.isComplete() ? "none" : item.isComplete() ? "full" : "part";
            out.println("(ixml.pa.step) @" + k + " " + itemId(item, item.origin(), k) + " " + kind + "/" + num_tokens + " "
                        + ruleText(item.production(), item.isComplete() ? -1 : item.dot()));
        }
    }

    /** (dN,from-to) where N numbers the dotted rule (production and dot). */
    private String itemId(Item item, int from, int to)
    {
        Item dotted_rule = new Item(item.production(), item.dot(), 0); // The origin is not part of the dotted rule.
        Integer id = dotted_rule_ids.get(dotted_rule);
        if (id == null)
        {
            id = dotted_rule_ids.size() + 1;
            dotted_rule_ids.put(dotted_rule, id);
        }
        return "(d" + id + "," + from + "-" + to + ")";
    }

    /** The production as "name →  a · b c", with the dot before symbol dot, or no dot if dot is -1. */
    private String ruleText(Production p, int dot)
    {
        StringBuilder b = new StringBuilder(traceName(p.rule.name)).append(" → ");
        List<Symbol> symbols = p.symbols;
        for (int i = 0; i < symbols.size(); i++)
        {
            if (i == dot) b.append(" ·");
            b.append(' ').append(symbolText(symbols.get(i)));
        }
        return b.toString();
    }

    private static String symbolText(Symbol s)
    {
        if (s instanceof Call c) return traceName(c.rule().name);
        if (s instanceof Chars c) return c.text();
        return Item.show(s);
    }

    /** The generated rules /1 are written |1 like in C. */
    private static String traceName(String name)
    {
        return name.startsWith("/") ? "|" + name.substring(1) : name;
    }
}
