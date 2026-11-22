/* Use NS_ANCESTOR with a namespace that no ancestor has declared. The namespace
   is then declared on the root element with a generated prefix, ns1, ns2...
   Prints: box(xmlns=urn:blue xmlns:ns1=urn:soft xmlns:ns2=urn:bar)
           {ns1:flower=many ns1:power=123 ns2:soft=petal} */

#include "xmq.h"

#include <stdio.h>
#include <stdlib.h>

int main()
{
    XMQReturnDoc rd = xmqNewDoc();
    if (rd.status != XMQ_OK) return 1;
    XMQDoc *doc = rd.doc;

    XMQReturnNode rn = xmqAddRootElement(doc, "box", NS_HERE("urn:blue"));
    if (rn.status != XMQ_OK) return 1;
    XMQNode *box = rn.node;

    rn = xmqAddKeyValue(doc, box, "flower", "many", NS_ANCESTOR("urn:soft"));
    if (rn.status != XMQ_OK) return 1;

    // The second use of urn:soft finds the ns1 prefix declared above.
    rn = xmqAddKeyValue(doc, box, "power", "123", NS_ANCESTOR("urn:soft"));
    if (rn.status != XMQ_OK) return 1;

    rn = xmqAddKeyValue(doc, box, "soft", "petal", NS_ANCESTOR("urn:bar"));
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
