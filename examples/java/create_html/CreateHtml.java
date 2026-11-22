/* Build an html page with a doctype, elements and an attribute, then print it as html.
   Prints: <!DOCTYPE html>
           <html><body><a href="https://libxmq.org"></a></body></html> */

import org.libxmq.NS;
import org.libxmq.OutputSettings;
import org.libxmq.XMQ;
import org.libxmq.imp.xml.XmlDocument;
import org.libxmq.imp.xml.XmlElement;

public class CreateHtml
{
    public static void main(String[] args)
    {
        XMQ xmq = new XMQ();
        XmlDocument doc = xmq.newDoc();

        xmq.setDocType(doc, "html");

        XmlElement html = xmq.addRootElement(doc, "html", NS.NONE);
        XmlElement body = xmq.addElement(html, "body", NS.PARENT);

        // The attributes are name, value pairs.
        xmq.addElementWithAttrs(body, "a", NS.PARENT, "href", "https://libxmq.org");

        OutputSettings os = new OutputSettings().setCompact(true).setEscapeNewlines(true);
        System.out.print(xmq.toHTML(doc, os));
    }
}
