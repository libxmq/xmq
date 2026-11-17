/* libxmq - Copyright (C) 2026 Fredrik Öhrström (spdx: MIT) */
package org.libxmq.imp.ixml.earley;

import java.io.PrintStream;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;

import org.libxmq.imp.XMQPrinter;
import org.libxmq.imp.ixml.IXMLGrammar;
import org.libxmq.imp.xml.XmlDocument;

/**
 * Parse an input with an ixml grammar using the Earley parser, in four steps:
 *
 * 1. EarleyParser.recognize       build the Earley sets for the input.
 * 2. EarleyParser.removeForbiddenByNots  drop items that a not forbids (to be implemented).
 * 3. TreeBuilder.build            compose the lowest cost tree from the sets.
 * 4. XmlWriter.toXml              serialize the tree following the ixml marks.
 *
 * Run it from the command line to look at the result:
 *   java -cp build/classes org.libxmq.imp.ixml.earley.Earley [--trace] grammar.ixml input.txt
 *   java -cp build/classes org.libxmq.imp.ixml.earley.Earley [--trace] grammar.ixml -i 'input'
 */
public final class Earley
{
    /** The result of a parse: a document, or an error at a position.
     * @param doc the document, null on an error
     * @param error the error message, null on success
     * @param position the input position (in code points) of a syntax error */
    public record Result(XmlDocument doc, String error, int position) { }

    private Earley() { }

    /** Parse the input with the grammar.
     * @param grammar_source the ixml grammar
     * @param input the input
     * @param trace print the grammar and the Earley sets here, or null
     * @return the result
     */
    public static Result parse(String grammar_source, String input, PrintStream trace)
    {
        Grammar grammar = new Grammar(IXMLGrammar.parse(grammar_source));
        if (trace != null)
        {
            for (Grammar.Production p : grammar.productions) trace.println("(earley.grammar) " + p);
        }
        EarleyParser parser = new EarleyParser(grammar);
        parser.setTrace(trace);

        int[] codepoints = input.codePoints().toArray();
        Chart chart = parser.recognize(codepoints);
        if (!parser.accepted(chart))
        {
            int at = parser.failurePosition(chart);
            return new Result(null, "syntax error", at);
        }
        parser.removeForbiddenByNots(chart);

        Tree tree = new TreeBuilder(chart, grammar).build();
        if (tree == null) return new Result(null, "no tree could be built", codepoints.length);
        try
        {
            return new Result(XmlWriter.toXml(tree, grammar), null, codepoints.length);
        }
        catch (XmlWriter.DynamicError e)
        {
            return new Result(null, e.getMessage(), codepoints.length);
        }
    }

    /** Command line: [--trace] grammar.ixml (input.txt | -i input). Prints the tree as xmq.
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
        String input = args[a + 1].equals("-i") && args.length - a > 2 ? args[a + 2]
                                                                    : Files.readString(Path.of(args[a + 1]), StandardCharsets.UTF_8);
        PrintStream trace_to = trace ? out : null;
        Result[] result = new Result[1];
        // Composing and writing the tree recurses as deep as the tree, use a large stack.
        Thread t = new Thread(null, () -> result[0] = parse(grammar, input, trace_to), "earley", 1L << 30);
        t.start();
        t.join();
        Result r = result[0];
        if (r.error() != null)
        {
            System.err.println("earley: " + r.error() + " at position " + r.position());
            System.exit(1);
        }
        out.println(XMQPrinter.toXmq(r.doc()));
    }
}
