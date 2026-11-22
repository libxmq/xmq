/* Build a document and print it as json. The attribute S marks a value as a json
   string and the attribute A marks an element as a json array. The key _ is an
   anonymous value, used for the members of an array.
   Prints: {"_":"request","id":"123","names":["samuel","isildur"]} */

#include "xmq.h"

#include <stdio.h>
#include <stdlib.h>

int main()
{
    XMQReturnDoc rd = xmqNewDoc();
    if (rd.status != XMQ_OK) return 1;
    XMQDoc *doc = rd.doc;

    XMQReturnNode rn = xmqAddRootElement(doc, "request", NS_NONE);
    if (rn.status != XMQ_OK) return 1;
    XMQNode *request = rn.node;

    // Without S the value 123 would be printed as a json number.
    rn = xmqAddKeyValueWithAttrs(doc, request, "id", "123", NS_NONE,
                                 XMQ_ATTRS( { "S", "" } ));
    if (rn.status != XMQ_OK) return 1;

    XMQReturnNode names = xmqAddElementWithAttrs(doc, request, "names", NS_NONE,
                                                 XMQ_ATTRS( { "A", "" } ));
    if (names.status != XMQ_OK) return 1;

    rn = xmqAddKeyValue(doc, names.node, "_", "samuel", NS_NONE);
    if (rn.status != XMQ_OK) return 1;

    rn = xmqAddKeyValue(doc, names.node, "_", "isildur", NS_NONE);
    if (rn.status != XMQ_OK) return 1;

    XMQOutputSettings *os = xmqNewOutputSettings();
    xmqSetCompact(os, true);
    xmqSetEscapeNewlines(os, true);
    xmqSetColorMode(os, XMQ_MONO);
    xmqSetOutputFormat(os, XMQ_CONTENT_JSON);
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
