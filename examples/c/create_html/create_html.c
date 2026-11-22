/* Build an html page with a doctype, elements and an attribute, then print it as html.
   Prints: <!DOCTYPE html>
           <html><body><a href="https://libxmq.org"></a></body></html> */

#include "xmq.h"

#include <stdio.h>
#include <stdlib.h>

int main()
{
    XMQReturnDoc rd = xmqNewDoc();
    if (rd.status != XMQ_OK) return 1;
    XMQDoc *doc = rd.doc;

    XMQStatus status = xmqSetDocType(doc, "html");
    if (status != XMQ_OK) return 1;

    XMQReturnNode html = xmqAddRootElement(doc, "html", NS_NONE);
    if (html.status != XMQ_OK) return 1;

    XMQReturnNode body = xmqAddElement(doc, html.node, "body", NS_PARENT);
    if (body.status != XMQ_OK) return 1;

    // XMQ_ATTRS builds a list of attribute name/value pairs.
    XMQReturnNode a = xmqAddElementWithAttrs(doc, body.node, "a", NS_PARENT,
                                             XMQ_ATTRS( { "href", "https://libxmq.org" } ));
    if (a.status != XMQ_OK) return 1;

    XMQOutputSettings *os = xmqNewOutputSettings();
    xmqSetCompact(os, true);
    xmqSetEscapeNewlines(os, true);
    xmqSetColorMode(os, XMQ_MONO);
    xmqSetOutputFormat(os, XMQ_CONTENT_HTML);
    xmqSetRenderFormat(os, XMQ_RENDER_PLAIN);

    char *start, *stop;
    xmqSetupPrintMemory(os, &start, &stop);
    xmqPrint(doc, os);
    xmqFreeOutputSettings(os);

    printf("%s", start);

    free(start);
    xmqFreeDoc(doc);
    return 0;
}
