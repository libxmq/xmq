/* Build a document where the root element declares a default namespace
   and the children inherit it from the parent.
   Prints: car(xmlns=urn:cargo){model=escargo color=green} */

import org.libxmq.NS;
import org.libxmq.OutputSettings;
import org.libxmq.XMQ;
import org.libxmq.imp.xml.XmlDocument;
import org.libxmq.imp.xml.XmlElement;

public class DefaultNamespace
{
    public static void main(String[] args)
    {
        XMQ xmq = new XMQ();
        XmlDocument doc = xmq.newDoc();

        // NS.here declares the namespace on this element.
        XmlElement car = xmq.addRootElement(doc, "car", NS.here("urn:cargo"));

        // NS.PARENT uses the same namespace as the parent element.
        xmq.addKeyValue(car, "model", "escargo", NS.PARENT);
        xmq.addKeyValue(car, "color", "green", NS.PARENT);

        OutputSettings os = new OutputSettings().setCompact(true);
        System.out.print(xmq.toXMQ(doc, os));
    }
}
