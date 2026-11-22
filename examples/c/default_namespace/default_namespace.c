/* Build a document where the root element declares a default namespace
   and the children inherit it from the parent.
   Prints: car(xmlns=urn:cargo){model=escargo color=green} */

#include "xmq.h"

#include <stdio.h>
#include <stdlib.h>

int main()
{
    XMQReturnDoc rd = xmqNewDoc();
    if (rd.status != XMQ_OK) return 1;
    XMQDoc *doc = rd.doc;

    // NS_HERE declares the namespace on this element.
    XMQReturnNode rn = xmqAddRootElement(doc, "car", NS_HERE("urn:cargo"));
    if (rn.status != XMQ_OK) return 1;
    XMQNode *car = rn.node;

    // NS_PARENT uses the same namespace as the parent element.
    rn = xmqAddKeyValue(doc, car, "model", "escargo", NS_PARENT);
    if (rn.status != XMQ_OK) return 1;

    rn = xmqAddKeyValue(doc, car, "color", "green", NS_PARENT);
    if (rn.status != XMQ_OK) return 1;

    XMQOutputSettings *os = xmqNewOutputSettings();
    xmqSetCompact(os, true);
    xmqSetOutputFormat(os, XMQ_CONTENT_XMQ);
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
