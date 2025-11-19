
package org.libxmq.imp;

import java.util.ArrayList;

public class TestInternals
{
    public static void main(String[] args)
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

    static void test_trim_quote(String input, String expected)
    {
        String output = QuoteUtil.trimQuote(input, 0, input.length());
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
