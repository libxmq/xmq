
package org.libxmq.imp;

import java.util.ArrayList;

public class TestInternals
{
    public static void main(String[] args)
    {
        ArrayList<QuotePart> parts = new ArrayList<>();
        test_parse("1", "\n   \n   Hejsan\n   Hoppsan\n \n", "\nHejsan\nHoppsan\n",
                   "nlenl=2 llenl=4 nennl=2 fennl=25 start=5 stop=25 mni=3 mxi=3");

        test_parse("2", "Hejsan\nHoppsan", "Hejsan\nHoppsan",
                   "nlenl=0 llenl=-1 nennl=0 fennl=-1 start=0 stop=14 mni=2147483647 mxi=0");

        test_parse("3", "HejsanHoppsan", "HejsanHoppsan",
                   "nlenl=0 llenl=-1 nennl=0 fennl=-1 start=0 stop=13 mni=2147483647 mxi=0");

        test_parse("3", "HejsanHoppsan", "HejsanHoppsan",
                   "nlenl=0 llenl=-1 nennl=0 fennl=-1 start=0 stop=13 mni=2147483647 mxi=0");

    }

    static void test_parse(String name, String input, String expected, String internals)
    {
        AnalyzeQuote aq = new AnalyzeQuote(input, 0, input.length());
        String output = aq.parseQuote();
        if (!expected.equals(output))
        {
            String i = Util.xmq_quote_as_c(input, -1, -1, true);
            String e = Util.xmq_quote_as_c(expected, -1, -1, true);
            String o = Util.xmq_quote_as_c(output, -1, -1, true);

            System.err.println("ERROR: "+name+"\ninput "+i+"\nexpected "+e+"\n but got "+o);
        }
        String eint = aq.parseInfo();
        if (!internals.equals(eint))
        {
            System.err.println("ERROR: internals "+name+"\nexpected "+internals+"\n but got "+eint);
        }
    }

}
