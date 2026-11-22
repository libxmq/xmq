/* Build a small document from scratch and print it as compact xmq.
   Prints: greeting{hello=world} */

import org.libxmq.NS;
import org.libxmq.OutputSettings;
import org.libxmq.XMQ;
import org.libxmq.imp.xml.XmlDocument;
import org.libxmq.imp.xml.XmlElement;

public class CreateDom
{
    public static void main(String[] args)
    {
        XMQ xmq = new XMQ();
        XmlDocument doc = xmq.newDoc();

        XmlElement greeting = xmq.addRootElement(doc, "greeting", NS.NONE);
        xmq.addKeyValue(greeting, "hello", "world", NS.PARENT);

        OutputSettings os = new OutputSettings().setCompact(true);
        System.out.print(xmq.toXMQ(doc, os));
    }
}
