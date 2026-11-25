
package org.libxmq.imp;

import java.util.ArrayList;
import org.libxmq.imp.xml.XmlDocument;
import org.libxmq.imp.xml.XmlElement;
import org.libxmq.imp.xml.XmlNode;
import org.libxmq.imp.xml.XmlText;

/**
 * Internal tests for the quote parsing utilities.
 */
public class TestInternals
{
    /**
     * Creates the test runner.
     */
    public TestInternals()
    {
    }
    /**
     * Runs the internal tests.
     * @param args Command line arguments (unused).
     */
    public static void main(String[] args)
    {
        test_quote_removal();
        test_quotes();
        test_deep_dom();

    }

    /**
     * Tests moving a tree nested deeper than a recursive walk could handle into another
     * document, the moved nodes (also the attributes) must get the new document.
     */
    public static void test_deep_dom()
    {
        final int depth = 200000;
        XmlDocument from = new XmlDocument();
        XmlElement root = new XmlElement("root");
        from.appendRaw(root);
        XmlNode n = root;
        for (int i = 0; i < depth; i++)
        {
            XmlElement c = new XmlElement("a");
            n.appendRaw(c);
            c.setAttribute("id", "x");
            c.appendRaw(new XmlText("t"));
            n = c;
        }
        XmlDocument to = new XmlDocument();
        XmlElement to_root = new XmlElement("to");
        to.appendRaw(to_root);
        to_root.appendRaw(root.firstChild());
        if (n.doc() != to || ((XmlElement)n).attribute("id").doc() != to || n.firstChild().doc() != to)
        {
            System.err.println("ERROR: the moved deep tree did not get the new document.");
            System.exit(1);
        }
    }

    /**
     * Tests removing surrounding quotes.
     */
    public static void test_quote_removal()
    {
        test_surrounding_quote("''", "");
        test_surrounding_quote("\"\"", "");
        test_surrounding_quote("''x''", "x");
        test_surrounding_quote("\"\"x\"\"", "x");
        test_surrounding_quote("'''''x'''''", "x");

        test_surrounding_quote("''\"x\"''", "\"x\"");
        test_surrounding_quote("\"'x'\"", "'x'");
        test_surrounding_quote("\"\"'\"x\"'\"\"", "'\"x\"'");
    }

    /**
     * Tests quote trimming.
     */
    public static void test_quotes()
    {
        ArrayList<QuotePart> parts = new ArrayList<>();

        test_trim_quote("HejsanHoppsan", "HejsanHoppsan");

        test_trim_quote("Hejsan\nHoppsan", "Hejsan\nHoppsan");

        test_trim_quote("\n   \n   Hejsan\n   Hoppsan\n \n   ", "\nHejsan\nHoppsan\n");

        // No newlines means no trimming.
        test_trim_quote(" ", " ");
        test_trim_quote("  ", "  ");
        test_trim_quote("  x  ", "  x  ");
        test_trim_quote("  x", "  x");
        test_trim_quote("x", "x");

        // A single newline is removed.
        test_trim_quote("\n", "");
        // A lot spaces are removed and one less newline.
        test_trim_quote("  \n \n    \n\n ", "\n\n\n");
        test_trim_quote("   \n", "");
        test_trim_quote("   \n   ", "");

        // First line leading spaces are kept if there is some non-space on the first line.
        test_trim_quote(" x\n ", " x");
        test_trim_quote("  x\n ", "  x");

        // Incidental is removed.
        test_trim_quote("\n x\n ", "x");
        test_trim_quote("x\n          ", "x");

        // Remove incidental indentation.
        test_trim_quote("abc\n def", "abc\ndef");

        // Yes, the abc has one extra indentation.
        test_trim_quote(" abc\n def", " abc\ndef");
        // Incidental is 1 because of first line and second line.
        test_trim_quote("\n QhowdyQ\n ", "QhowdyQ");
        // Incidental is 0 because of second line.
        test_trim_quote("\nQhowdyQ\n ", "QhowdyQ");

        // Remove incidetal. Indentation number irrelevant since first line is empty.
        test_trim_quote("\n    x\n  y\n    z\n", "  x\ny\n  z");

        // Assume first line has the found incidental indentation.
        test_trim_quote("HOWDY\n    HOWDY\n    HOWDY", "HOWDY\nHOWDY\nHOWDY");

        // Remove incidental. Indentation number irrelevant since first line is empty.
        test_trim_quote("\n    x\n  y\n    z\n", "  x\ny\n  z");

        // Last line influences incidental indentation, even if it is all spaces.
        test_trim_quote("\n    x\n  ", "  x");
        test_trim_quote("\n    x\n\n  ", "  x\n");
    }

    /**
     * Tests trimming of a quote against an expected value.
     * @param input The input string.
     * @param expected The expected output string.
     */
    static void test_trim_quote(String input, String expected)
    {
        String output = UtilParseQuote.trimQuote(input, 0, input.length());
        if (!expected.equals(output))
        {
            String i = Util.xmq_quote_as_c(input, -1, -1, true);
            String e = Util.xmq_quote_as_c(expected, -1, -1, true);
            String o = Util.xmq_quote_as_c(output, -1, -1, true);

            System.err.println("ERROR:\ninput    "+i+"\nexpected "+e+"\n but got "+o);
            System.exit(1);
        }
    }

    /**
     * Tests finding the inner part of a quoted string.
     * @param input The input string.
     * @param expected The expected output string.
     */
    static void test_surrounding_quote(String input, String expected)
    {
        var pair = UtilParseQuote.findQuoteStartStop(input, 0, input.length());

        String output = input.substring(pair.left(), pair.right());

        if (!expected.equals(output))
        {
            String i = Util.xmq_quote_as_c(input, -1, -1, true);
            String e = Util.xmq_quote_as_c(expected, -1, -1, true);
            String o = Util.xmq_quote_as_c(output, -1, -1, true);

            System.err.println("ERROR:\ninput    "+i+"\nexpected "+e+"\n but got "+o);
            System.exit(1);
        }
    }

}
