/* Build a document that mixes namespaces with prefixes. The prefix is given
   inside {} in NS.here, eg NS.here("{krf}urn:kraftwerk"), and NS.NONE puts
   an element in no namespace at all.
   Prints: krf:robot(xmlns:krf=urn:kraftwerk){krf:who='we are'krf:the=robots
           c:car(xmlns:c=urn:cargo){c:model=escargo}box(xmlns=''){color=blue}} */

import org.libxmq.NS;
import org.libxmq.OutputSettings;
import org.libxmq.XMQ;
import org.libxmq.imp.xml.XmlDocument;
import org.libxmq.imp.xml.XmlElement;

public class NamespacePrefixes
{
    public static void main(String[] args)
    {
        XMQ xmq = new XMQ();
        XmlDocument doc = xmq.newDoc();

        XmlElement robot = xmq.addRootElement(doc, "robot", NS.here("{krf}urn:kraftwerk"));
        xmq.addKeyValue(robot, "who", "we are", NS.PARENT);
        xmq.addKeyValue(robot, "the", "robots", NS.PARENT);

        XmlElement car = xmq.addElement(robot, "car", NS.here("{c}urn:cargo"));
        xmq.addKeyValue(car, "model", "escargo", NS.PARENT);

        XmlElement box = xmq.addElement(robot, "box", NS.NONE);
        xmq.addKeyValue(box, "color", "blue", NS.PARENT);

        OutputSettings os = new OutputSettings().setCompact(true).setEscapeNewlines(true);
        System.out.print(xmq.toXMQ(doc, os));
    }
}
