
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

void test_building_dom4()
{
    // Use ancestor to find the proper uri and use the found prefix.
    const char *exp = "poof:coffee(xmlns:poof=urn:shop){bar(xmlns=urn:barrista){poof:price=12.5 woot=money}}\n";

    XMQReturnDoc rd = xmqNewDoc();
    assert(rd.status == XMQ_OK);
    XMQDoc *doc = rd.doc;

    XMQReturnNode rn = xmqAddRootElement(doc, "coffee", NS_HERE("{poof}urn:shop"));
    assert(rn.status == XMQ_OK);
    XMQNode *coffee = rn.node;

    rn = xmqAddElement(doc, coffee, "bar", NS_HERE("urn:barrista"));
    assert(rn.status == XMQ_OK);
    XMQNode *bar = rn.node;

    xmqAddKeyValue(doc, bar, "price", "12.5", NS_ANCESTOR("urn:shop"));
    xmqAddKeyValue(doc, bar, "woot", "money", NS_PARENT);

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

void test_building_dom5()
{
    // We start with a default namespace urn:blue then later att an element
    // with an ancestor namespace with a prefix {c}urn:coffee.
    // This namespace does not exist and is added to the root node.
    const char *exp = "box(xmlns=urn:blue xmlns:c=urn:coffee){bar{c:price=99}}\n";

    XMQReturnDoc rd = xmqNewDoc();
    assert(rd.status == XMQ_OK);
    XMQDoc *doc = rd.doc;

    XMQReturnNode rn = xmqAddRootElement(doc, "box", NS_HERE("urn:blue"));
    assert(rn.status == XMQ_OK);
    XMQNode *box = rn.node;

    rn = xmqAddElement(doc, box, "bar", NS_PARENT);
    assert(rn.status == XMQ_OK);
    XMQNode *bar = rn.node;

    xmqAddKeyValue(doc, bar, "price", "99", NS_ANCESTOR("{c}urn:coffee"));

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

void test_building_dom6()
{
    // We start with a default namespace urn:blue then later att an element
    // with an ancestor namespace with a prefix {c}urn:coffee.
    // This namespace does exist in bar, and that is used.
    const char *exp = "box(xmlns=urn:blue xmlns:c=urn:coffee){bar{c:price=99}}\n";

    XMQReturnDoc rd = xmqNewDoc();
    assert(rd.status == XMQ_OK);
    XMQDoc *doc = rd.doc;

    XMQReturnNode rn = xmqAddRootElement(doc, "box", NS_HERE("urn:blue"));
    assert(rn.status == XMQ_OK);
    XMQNode *box = rn.node;

    rn = xmqAddElement(doc, box, "bar", NS_PARENT);
    assert(rn.status == XMQ_OK);
    XMQNode *bar = rn.node;

    xmqAddNamespace(doc, bar, NS_HERE("{c}urn:coffee"));

    xmqAddKeyValue(doc, bar, "price", "99", NS_ANCESTOR("{c}urn:coffee"));

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
    test_building_dom4();
    test_building_dom5();
}
