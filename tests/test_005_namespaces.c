
#include<assert.h>
#include<xmq.h>
#include"test.h"

char *test = __FILE__;

void test_building_dom0()
{
    // Build a document without namespace.
    const char *exp = "car{model=escargo color=green}\n";

    XMQReturnDoc rd = xmqNewDoc();
    assert(rd.status == XMQ_OK);
    XMQDoc *doc = rd.doc;
    void *ns;

    XMQReturnNode rn = xmqAddRootElement(doc, "car", NS_NONE);
    assert(rn.status == XMQ_OK);
    XMQNode *car = rn.node;
    xmqAddKeyValue(doc, car, "model", "escargo", NS_PARENT);
    xmqAddKeyValue(doc, car, "color", "green", NS_PARENT);

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

    if (strcmp(start, exp))
    {
        printf("Error.\nExpected:\%s\nGot:\n%s\n", exp, start);
        exit(1);
    }
    free(start);
}

void test_building_dom1()
{
    // Build a document with a default namespace.
    const char *exp = "car(xmlns=urn:cargo){model=escargo color=green}\n";

    XMQReturnDoc rd = xmqNewDoc();
    assert(rd.status == XMQ_OK);
    XMQDoc *doc = rd.doc;
    void *ns;

    XMQReturnNode rn = xmqAddRootElement(doc, "car", NS_HERE("urn:cargo"));
    assert(rn.status == XMQ_OK);
    XMQNode *car = rn.node;
    xmqAddKeyValue(doc, car, "model", "escargo", NS_PARENT);
    xmqAddKeyValue(doc, car, "color", "green", NS_PARENT);

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

    if (strcmp(start, exp))
    {
        printf("Error.\nExpected:\%s\nGot:\n%s\n", exp, start);
        exit(1);
    }
    free(start);
}

void test_building_dom2()
{
    // Build a document with a prefixed namespace.
    const char *exp = "abc:car(xmlns:abc=urn:cargo){abc:model=escargo abc:color=green}\n";

    XMQReturnDoc rd = xmqNewDoc();
    assert(rd.status == XMQ_OK);
    XMQDoc *doc = rd.doc;
    void *ns;

    XMQReturnNode rn = xmqAddRootElement(doc, "car", NS_HERE("{abc}urn:cargo"));
    assert(rn.status == XMQ_OK);
    XMQNode *car = rn.node;
    xmqAddKeyValue(doc, car, "model", "escargo", NS_PARENT);
    xmqAddKeyValue(doc, car, "color", "green", NS_PARENT);

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

    if (strcmp(start, exp))
    {
        printf("Error.\nExpected:\%s\nGot:\n%s\n", exp, start);
        exit(1);
    }
    free(start);
}

void test_building_dom3()
{
    // Use ancestor to find the proper uri.
    const char *exp = "coffee(xmlns=urn:shop){price=12.5}\n";

    XMQReturnDoc rd = xmqNewDoc();
    assert(rd.status == XMQ_OK);
    XMQDoc *doc = rd.doc;
    void *ns;

    XMQReturnNode rn = xmqAddRootElement(doc, "coffee", NS_HERE("urn:shop"));
    assert(rn.status == XMQ_OK);
    XMQNode *coffee = rn.node;
    xmqAddKeyValue(doc, coffee, "price", "12.5", NS_ANCESTOR("urn:shop"));

    XMQOutputSettings *os = xmqNewOutputSettings();

    xmqSetCompact(os, true);
    xmqSetUseColor(os, false);
    xmqSetOutputFormat(os, XMQ_CONTENT_XMQ);
    xmqSetRenderFormat(os, XMQ_RENDER_PLAIN);

    char *start, *stop;
    xmqSetupPrintMemory(os, &start, &stop);
    xmqPrint(doc, os);

    xmqFreeOutputSettings(os);

    if (strcmp(start, exp))
    {
        printf("Error.\nExpected:\%s\nGot:\n%s\n", exp, start);
        exit(1);
    }
    free(start);
}

int main(int argc, char **argv)
{
    test_building_dom0();
    test_building_dom1();
    test_building_dom2();
    test_building_dom3();
}
