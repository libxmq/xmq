/* Build a document that mixes namespaces with prefixes. The prefix is given
   inside {} in NS_HERE, eg NS_HERE("{krf}urn:kraftwerk"), and NS_NONE puts
   an element in no namespace at all.
   Prints: krf:robot(xmlns:krf=urn:kraftwerk){krf:who='we are'krf:the=robots
           c:car(xmlns:c=urn:cargo){c:model=escargo}box(xmlns=''){color=blue}} */

#include "xmq.h"

#include <stdio.h>
#include <stdlib.h>

int main()
{
    XMQReturnDoc rd = xmqNewDoc();
    if (rd.status != XMQ_OK) return 1;
    XMQDoc *doc = rd.doc;

    XMQReturnNode rn = xmqAddRootElement(doc, "robot", NS_HERE("{krf}urn:kraftwerk"));
    if (rn.status != XMQ_OK) return 1;
    XMQNode *robot = rn.node;

    rn = xmqAddKeyValue(doc, robot, "who", "we are", NS_PARENT);
    if (rn.status != XMQ_OK) return 1;

    rn = xmqAddKeyValue(doc, robot, "the", "robots", NS_PARENT);
    if (rn.status != XMQ_OK) return 1;

    rn = xmqAddElement(doc, robot, "car", NS_HERE("{c}urn:cargo"));
    if (rn.status != XMQ_OK) return 1;

    rn = xmqAddKeyValue(doc, rn.node, "model", "escargo", NS_PARENT);
    if (rn.status != XMQ_OK) return 1;

    rn = xmqAddElement(doc, robot, "box", NS_NONE);
    if (rn.status != XMQ_OK) return 1;

    rn = xmqAddKeyValue(doc, rn.node, "color", "blue", NS_PARENT);
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
