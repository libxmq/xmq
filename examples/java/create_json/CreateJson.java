/* Build a document and print it as json. The attribute S marks a value as a json
   string and the attribute A marks an element as a json array. The key _ is an
   anonymous value, used for the members of an array.
   Prints: {"_":"request","id":"123","names":["samuel","isildur"]} */

import org.libxmq.NS;
import org.libxmq.OutputSettings;
import org.libxmq.XMQ;
import org.libxmq.imp.xml.XmlDocument;
import org.libxmq.imp.xml.XmlElement;

public class CreateJson
{
    public static void main(String[] args)
    {
        XMQ xmq = new XMQ();
        XmlDocument doc = xmq.newDoc();

        XmlElement request = xmq.addRootElement(doc, "request", NS.NONE);

        // Without S the value 123 would be printed as a json number.
        xmq.addKeyValueWithAttrs(request, "id", "123", NS.NONE, "S", "");

        XmlElement names = xmq.addElementWithAttrs(request, "names", NS.NONE, "A", "");
        xmq.addKeyValue(names, "_", "samuel", NS.NONE);
        xmq.addKeyValue(names, "_", "isildur", NS.NONE);

        OutputSettings os = new OutputSettings().setCompact(true).setEscapeNewlines(true);
        System.out.print(xmq.toJSON(doc, os));
    }
}
