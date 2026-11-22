/* Declare an extra namespace on an element with addNamespace, then use it
   for a child with NS.ancestor, which finds the prefix declared on an ancestor.
   Prints: flower(xmlns=urn:power xmlns:s=urn:soft){s:petals=many} */

import org.libxmq.NS;
import org.libxmq.OutputSettings;
import org.libxmq.XMQ;
import org.libxmq.imp.xml.XmlDocument;
import org.libxmq.imp.xml.XmlElement;

public class AddNamespace
{
    public static void main(String[] args)
    {
        XMQ xmq = new XMQ();
        XmlDocument doc = xmq.newDoc();

        XmlElement flower = xmq.addRootElement(doc, "flower", NS.here("urn:power"));

        xmq.addNamespace(flower, NS.here("{s}urn:soft"));

        xmq.addKeyValue(flower, "petals", "many", NS.ancestor("urn:soft"));

        OutputSettings os = new OutputSettings().setCompact(true).setEscapeNewlines(true);
        System.out.print(xmq.toXMQ(doc, os));
    }
}
