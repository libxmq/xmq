
#include<assert.h>
#include<xmq.h>
#include"test.h"

char *test = __FILE__;

void test_building_dom1()
{
    XMQReturnDoc rd = xmqNewDoc();
    assert(rd.status == XMQ_OK);
    XMQDoc *doc = rd.doc;
    void *ns;

    XMQReturnNode rn = xmqAddRootNode(doc, "car", "urn:cargo");
    assert(rn.status == XMQ_OK);
    XMQNode *car = rn.node;
    xmqAddKeyValue(doc, car, "model", "escargo");
    xmqAddKeyValue(doc, car, "color", "green");

    XMQOutputSettings *os = xmqNewOutputSettings();

    xmqSetCompact(os, true);
    xmqSetEscapeNewlines(os, true);
    xmqSetUseColor(os, false);
    xmqSetOutputFormat(os, XMQ_CONTENT_XMQ);
    xmqSetRenderFormat(os, XMQ_RENDER_PLAIN);

    char *start, *stop;
    xmqSetupPrintMemory(os, &start, &stop);
    xmqPrint(doc, os);

    xmqFreeOutputSettings(os);

    const char *exp = "car(xmlns=urn:cargo){model=escargo color=green}\n";
    if (strcmp(start, exp))
    {
        printf("Building of dom tree failed. Got: %s\nExpected: %s\n", start, exp);
        exit(1);
    }
    free(start);
}

void test_building_dom2()
{
    XMQReturnDoc rd = xmqNewDoc();
    assert(rd.status == XMQ_OK);
    XMQDoc *doc = rd.doc;
    void *ns;

    XMQReturnNode rn = xmqAddRootNode(doc, "book", XMQ_NO_NAMESPACE);
    assert(rn.status == XMQ_OK);
    XMQNode *book = rn.node;
    xmqAddKeyValue(doc, book, "name", "100 years of solitude");

    XMQOutputSettings *os = xmqNewOutputSettings();

    xmqSetCompact(os, true);
    xmqSetUseColor(os, false);
    xmqSetOutputFormat(os, XMQ_CONTENT_XMQ);
    xmqSetRenderFormat(os, XMQ_RENDER_PLAIN);

    char *start, *stop;
    xmqSetupPrintMemory(os, &start, &stop);
    xmqPrint(doc, os);

    xmqFreeOutputSettings(os);

    const char *exp = "book{name='100 years of solitude'}\n";
    if (strcmp(start, exp))
    {
        printf("Building of dom tree failed. Got: %s\nExpected: %s\n", start, exp);
        exit(1);
    }
    free(start);
}

int main(int argc, char **argv)
{
    test_building_dom1();
    test_building_dom2();
}
