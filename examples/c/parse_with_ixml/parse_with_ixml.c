/* Parse an ixml grammar once, then use it to parse text into a document.
   The values found by the grammar are summed using xmqForeach over an xpath.
   The parse is repeated to measure how long each parse takes. */

#include "xmq.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>

#define ROUNDS 10000

// Called by xmqForeach for each node found by the xpath.
XMQProceed add_value(XMQDoc *doc, XMQNode *node, void *user_data)
{
    int *sum = (int*)user_data;
    *sum += xmqGetIntRel(doc, ".", node);
    return XMQ_CONTINUE;
}

int main()
{
    // The grammar: skip an a, then a comma separated list of numbers B.
    XMQReturnDoc rd = xmqNewDoc();
    if (rd.status != XMQ_OK) return 1;
    XMQDoc *ixml = rd.doc;

    bool ok = xmqParseBufferWithType(ixml,
                                     "decode = -'a', B++-','. B=[N]+.",
                                     NULL, NULL, XMQ_CONTENT_IXML, 0);
    if (!ok)
    {
        printf("Grammar error\n%s", xmqDocError(ixml));
        return 1;
    }

    struct timeval start, stop;
    gettimeofday(&start, NULL);

    int sum = 0;
    for (int i = 0; i < ROUNDS; ++i)
    {
        rd = xmqNewDoc();
        if (rd.status != XMQ_OK) return 1;
        XMQDoc *decode = rd.doc;

        ok = xmqParseBufferWithIXML(decode, "a123,2,444", NULL, ixml, 0);
        if (!ok)
        {
            printf("Parse error\n%s", xmqDocError(decode));
            return 1;
        }

        sum = 0;
        xmqForeach(decode, "//B", add_value, &sum);
        xmqFreeDoc(decode);
    }
    gettimeofday(&stop, NULL);
    xmqFreeDoc(ixml);

    double us = (stop.tv_sec - start.tv_sec) * 1000000.0 + (stop.tv_usec - start.tv_usec);

    printf("sum of a123,2,444 is %d\n", sum);
    printf("each ixml parse and foreach took %d us\n", (int)(us / ROUNDS));
    return 0;
}
