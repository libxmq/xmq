/* Compile an ixml grammar once, then use it to parse text into a document.
   The values found by the grammar are summed using forEach over an xpath.
   The parse is repeated to measure how long each parse takes. */

import org.libxmq.IXML;
import org.libxmq.Proceed;
import org.libxmq.Query;
import org.libxmq.XMQ;
import org.libxmq.imp.xml.XmlDocument;

public class ParseWithIxml
{
    static final int ROUNDS = 10000;

    public static void main(String[] args) throws Exception
    {
        XMQ xmq = new XMQ();

        // The grammar: skip an a, then a comma separated list of numbers B.
        IXML ixml = xmq.compileIXML("decode = -'a', B++-','. B=[N]+.");

        long start = System.nanoTime();

        int[] sum = new int[1];
        for (int i = 0; i < ROUNDS; ++i)
        {
            XmlDocument decode = xmq.parseBufferWithIXML("a123,2,444", ixml);

            sum[0] = 0;
            // The callback is invoked for each node found by the xpath.
            new Query(decode).forEach("//B", node -> {
                sum[0] += Integer.parseInt(node.textContent());
                return Proceed.CONTINUE;
            });
        }

        long us = (System.nanoTime() - start) / 1000;

        System.out.println("sum of a123,2,444 is " + sum[0]);
        System.out.println("each ixml parse and foreach took " + (us / ROUNDS) + " us");
    }
}
