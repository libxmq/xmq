/* Declare an extra namespace on an element with xmqAddNamespace, then use it
   for a child with NS_ANCESTOR, which finds the prefix declared on an ancestor.
   Prints: flower(xmlns=urn:power xmlns:s=urn:soft){s:petals=many} */

#include "xmq.h"

#include <stdio.h>
#include <stdlib.h>

int main()
{
    XMQReturnDoc rd = xmqNewDoc();
    if (rd.status != XMQ_OK) return 1;
    XMQDoc *doc = rd.doc;

    XMQReturnNode rn = xmqAddRootElement(doc, "flower", NS_HERE("urn:power"));
    if (rn.status != XMQ_OK) return 1;
    XMQNode *flower = rn.node;

    XMQStatus rc = xmqAddNamespace(doc, flower, NS_HERE("{s}urn:soft"));
    if (rc != XMQ_OK) return 1;

    rn = xmqAddKeyValue(doc, flower, "petals", "many", NS_ANCESTOR("urn:soft"));
    if (rn.status != XMQ_OK) return 1;

    XMQOutputSettings *os = xmqNewOutputSettings();
    xmqSetCompact(os, true);
    xmqSetEscapeNewlines(os, true);
    xmqSetColorMode(os, XMQ_MONO);
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
