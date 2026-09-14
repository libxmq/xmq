
package org.libxmq.imp;

import org.jdom2.Document;
import org.jdom2.input.SAXBuilder;
import org.jdom2.input.sax.SAXHandler;
import org.jdom2.input.sax.SAXHandlerFactory;
import org.jdom2.input.sax.XMLReaders;
import org.jdom2.output.Format;
import org.jdom2.output.XMLOutputter;
import org.xml.sax.SAXException;

import java.io.StringReader;

public class JDomEntityFix {
    public static void main(String[] args) throws Exception {
        String xml = "<!DOCTYPE time [<!ENTITY copy \"howdy\">]>\n<time>alfa&copy;beta</time>";

        SAXBuilder sb = new SAXBuilder(XMLReaders.NONVALIDATING);
        sb.setExpandEntities(false);

        sb.setFeature("http://xml.org/sax/features/external-general-entities", false);
        sb.setFeature("http://apache.org/xml/features/nonvalidating/load-external-dtd", false);

        sb.setSAXHandlerFactory(new SAXHandlerFactory() {
            @Override
            public SAXHandler createSAXHandler(org.jdom2.JDOMFactory factory) {
                return new SAXHandler(factory) {
                    private boolean inGeneralEntity = false;

                    @Override
                    public void startEntity(String name) throws SAXException {
                        // Flush any pending text (e.g. "alfa") BEFORE opening the entity
                        if (!inGeneralEntity && !name.equals("[dtd]") && !name.equals("[parameter]")) {
                            flushCharacters();
                            inGeneralEntity = true;
                        }
                        super.startEntity(name);
                    }

                    @Override
                    public void endEntity(String name) throws SAXException {
                        super.endEntity(name);
                        if (!name.equals("[dtd]") && !name.equals("[parameter]")) {
                            // Clear out any "howdy" buffered while inside the entity
                            flushCharacters();
                            inGeneralEntity = false;
                        }
                    }

                    @Override
                    public void characters(char[] ch, int start, int length) throws SAXException {
                        // Ignore character callbacks while inside the entity body
                        if (!inGeneralEntity) {
                            super.characters(ch, start, length);
                        }
                    }
                };
            }
        });

        Document doc = sb.build(new StringReader(xml));

        XMLOutputter outputter = new XMLOutputter(Format.getRawFormat());
        System.out.println(outputter.outputString(doc));
    }
}
