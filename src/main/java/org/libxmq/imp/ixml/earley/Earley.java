/* libxmq - Copyright (C) 2026 Fredrik Öhrström (spdx: MIT) */
package org.libxmq.imp.ixml.earley;

import java.io.PrintStream;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.List;

import org.libxmq.imp.XMQPrinter;
import org.libxmq.imp.ixml.IXMLGrammar;
import org.libxmq.imp.xml.XmlDocument;

/**
 * Parse an input with an ixml grammar using the Earley parser, in four steps:
 *
 * 1. EarleyParser.recognize       scan the UTF-8 bytes of the input into tokens (Chart)
 *                                 and build the Earley sets for them.
 * 2. EarleyParser.removeForbiddenByNots  drop items that a not forbids (to be implemented).
 * 3. TreeBuilder.build            compose the lowest cost tree from the sets.
 * 4. XmlWriter.toXml              serialize the tree following the ixml marks.
 *
 * EarleyTrace writes the sets and the tree from the result, as a trace document
 * (xmq --traceixml=file) or in the line format of the C parser (xmq --debug=ixml.).
 *
 * Run it from the command line to look at the result:
 *   java -cp build/classes org.libxmq.imp.ixml.earley.Earley [--trace] grammar.ixml input.txt
 *   java -cp build/classes org.libxmq.imp.ixml.earley.Earley [--trace] grammar.ixml -i 'input'
 */
public final class Earley
{
    /** The result of a parse: a document, or an error at a position. The grammar, the
     * chart and the tree are kept for EarleyTrace.
     * @param doc the document, null on an error
     * @param error the error message, null on success
     * @param position the input position (in code points) of a syntax error
     * @param grammar the grammar
     * @param chart the Earley sets, null if the input is not valid UTF-8
     * @param tree the chosen tree, null if the input was rejected
     * @param steps the nodes in the order they were added to the tree, see TreeBuilder.steps
     * @param expected on a syntax error, what the parser could read at the position */
    public record Result(XmlDocument doc, String error, int position, Grammar grammar, Chart chart, Tree tree,
                         List<TreeBuilder.Step> steps, List<EarleyParser.Expected> expected) { }

    /** The start of the error message of an input that is not valid UTF-8, see Chart.InvalidUtf8. */
    public static final String INVALID_UTF8 = "invalid UTF-8";

    private Earley() { }

    /** Build the Earley grammar from an ixml grammar source. The grammar can be used for many
     * parses, it keeps the tokens of the code points seen in the inputs (see Tokens), but not
     * by several threads at the same time.
     * @param grammar_source the ixml grammar
     * @return the grammar
     */
    public static Grammar grammar(String grammar_source)
    {
        return new Grammar(IXMLGrammar.parse(grammar_source));
    }

    /** What the Earley parser expected where it rejected the input, for the failure
     * message of another parser (IXMLAnalyzer).
     * @param grammar_source the ixml grammar
     * @param input the rejected input, as UTF-8
     * @param position the code point position where the other parser failed
     * @return what could come next, or an empty list if the Earley parser does not
     *         reject the input at the same position (eg a not, which it does not check yet) */
    public static List<EarleyParser.Expected> expectedAt(String grammar_source, byte[] input, int position)
    {
        EarleyParser parser = new EarleyParser(grammar(grammar_source));
        Chart chart;
        try
        {
            chart = parser.recognize(input, 0, input.length);
        }
        catch (Chart.InvalidUtf8 e)
        {
            return List.of();
        }
        if (parser.accepted(chart) || parser.failurePosition(chart) != position) return List.of();
        return parser.expected(chart, position);
    }

    /** Parse the input with the grammar.
     * @param grammar_source the ixml grammar
     * @param input the input, as UTF-8
     * @return the result
     */
    public static Result parse(String grammar_source, byte[] input)
    {
        return parse(grammar(grammar_source), input, 0, input.length);
    }

    /** Parse the input with the grammar. The code points are decoded directly from the bytes,
     * the input is never turned into a String.
     * @param grammar the grammar, from grammar(), the tokens of the input are added to it
     * @param buf the input as UTF-8
     * @param start the first byte of the input
     * @param stop the end of the input, exclusive
     * @return the result, an input that is not valid UTF-8 gives an error starting with INVALID_UTF8
     */
    public static Result parse(Grammar grammar, byte[] buf, int start, int stop)
    {
        EarleyParser parser = new EarleyParser(grammar);

        Chart chart;
        try
        {
            chart = parser.recognize(buf, start, stop);
        }
        catch (Chart.InvalidUtf8 e)
        {
            return new Result(null, e.getMessage(), e.position, grammar, null, null, List.of(), List.of());
        }
        if (!parser.accepted(chart))
        {
            int position = parser.failurePosition(chart);
            return new Result(null, "syntax error", position, grammar, chart, null, List.of(), parser.expected(chart, position));
        }
        parser.removeForbiddenByNots(chart);

        int length = chart.length();
        TreeBuilder builder = new TreeBuilder(chart, grammar);
        Tree tree = builder.build();
        List<TreeBuilder.Step> steps = builder.steps();
        if (tree == null) return new Result(null, "no tree could be built", length, grammar, chart, null, steps, List.of());
        try
        {
            return new Result(XmlWriter.toXml(tree, grammar), null, length, grammar, chart, tree, steps, List.of());
        }
        catch (XmlWriter.DynamicError e)
        {
            return new Result(null, e.getMessage(), length, grammar, chart, tree, steps, List.of());
        }
    }

    /** Command line: [--trace] grammar.ixml (input.txt | -i input). Prints the tree as xmq,
     * with --trace the trace (EarleyTrace) is written to stderr.
     * @param args the arguments
     * @throws Exception on io errors
     */
    public static void main(String[] args) throws Exception
    {
        PrintStream out = new PrintStream(new java.io.FileOutputStream(java.io.FileDescriptor.out), true, StandardCharsets.UTF_8);
        int a = 0;
        boolean trace = false;
        if (a < args.length && args[a].equals("--trace")) { trace = true; a++; }
        if (args.length - a < 2)
        {
            System.err.println("Usage: Earley [--trace] grammar.ixml (input.txt | -i input)");
            System.exit(1);
        }
        String grammar = Files.readString(Path.of(args[a]), StandardCharsets.UTF_8);
        byte[] input = args[a + 1].equals("-i") && args.length - a > 2 ? args[a + 2].getBytes(StandardCharsets.UTF_8)
                                                                    : Files.readAllBytes(Path.of(args[a + 1]));
        Result[] result = new Result[1];
        // Composing and writing the tree recurses as deep as the tree, use a large stack.
        Thread t = new Thread(null, () -> result[0] = parse(grammar, input), "earley", 1L << 30);
        t.start();
        t.join();
        Result r = result[0];
        if (trace && r.chart() != null)
        {
            PrintStream err = new PrintStream(new java.io.FileOutputStream(java.io.FileDescriptor.err), true, StandardCharsets.UTF_8);
            EarleyTrace.write(err, r.grammar(), r.chart(), r.tree(), r.steps());
        }
        if (r.error() != null)
        {
            System.err.println("earley: " + r.error() + " at position " + r.position());
            System.exit(1);
        }
        out.println(XMQPrinter.toXmq(r.doc()));
    }
}
