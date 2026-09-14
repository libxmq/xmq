/* libxmq - Copyright (C) 2025-2026 Fredrik Öhrström (spdx: MIT)

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

*/

package org.libxmq.imp;

import org.jdom2.Document;
import org.jdom2.EntityRef;
import org.jdom2.input.SAXBuilder;
import org.jdom2.input.sax.SAXHandler;
import org.jdom2.input.sax.SAXHandlerFactory;
import org.jdom2.input.sax.XMLReaders;
import java.io.StringReader;
import org.xml.sax.SAXException;

public class FixedEntityParser
{
    public static Document parseWithoutDuplicateEntities(String content) throws Exception
    {
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

        Document doc = sb.build(new StringReader(content));
        return doc;
    }
}
