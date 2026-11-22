/* Use NS.ancestor with a namespace that no ancestor has declared. The namespace
   is then declared on the root element with a generated prefix, ns1, ns2...
   Prints: box(xmlns=urn:blue xmlns:ns1=urn:soft xmlns:ns2=urn:bar)
           {ns1:flower=many ns1:power=123 ns2:soft=petal} */

import org.libxmq.NS;
import org.libxmq.OutputSettings;
import org.libxmq.XMQ;
import org.libxmq.imp.xml.XmlDocument;
import org.libxmq.imp.xml.XmlElement;

public class GeneratedPrefixes
{
    public static void main(String[] args)
    {
        XMQ xmq = new XMQ();
        XmlDocument doc = xmq.newDoc();

        XmlElement box = xmq.addRootElement(doc, "box", NS.here("urn:blue"));

        xmq.addKeyValue(box, "flower", "many", NS.ancestor("urn:soft"));

        // The second use of urn:soft finds the ns1 prefix declared above.
        xmq.addKeyValue(box, "power", "123", NS.ancestor("urn:soft"));

        xmq.addKeyValue(box, "soft", "petal", NS.ancestor("urn:bar"));

        OutputSettings os = new OutputSettings().setCompact(true).setEscapeNewlines(true);
        System.out.print(xmq.toXMQ(doc, os));
    }
}
