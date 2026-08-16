
#include<assert.h>
#include<xmq.h>
#include"test.h"

char *test = __FILE__;

void test_building_doc_0()
{
    // Build a document without namespace.
    const char *exp = "car{model=escargo color=green}\n";
    // <car><model>escargo</model><color>green</color></car>

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

void test_building_doc_1()
{
    // Build a document with a default namespace.
    const char *exp = "car(xmlns=urn:cargo){model=escargo color=green}\n";
    // <car xmlns="urn:cargo"><model>escargo</model><color>green</color></car>

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

void test_building_doc_2()
{
    // Build a document with a prefixed namespace.
    const char *exp = "abc:car(xmlns:abc=urn:cargo){abc:model=escargo abc:color=green}\n";
    // <abc:car xmlns:abc="urn:cargo"><abc:model>escargo</abc:model><abc:color>green</abc:color></abc:car>
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

void test_building_doc_3()
{
    // Use ancestor to find the proper namespace using only the uri.
    const char *exp = "coffee(xmlns=urn:shop){price=12.5}\n";
    // <coffee xmlns="urn:shop"><price>12.5</price></coffee>

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

void test_building_doc_4()
{
    // Use ancestor to find the namespace using only the uri. However the found namespace has a prefix, use it!
    const char *exp = "poof:coffee(xmlns:poof=urn:shop){bar(xmlns=urn:barrista){poof:price=12.5 woot=money}}\n";
    // <poof:coffee xmlns:poof="urn:shop"><bar xmlns="urn:barrista"><poof:price>12.5</poof:price><woot>money</woot></poof:coffee>

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

void test_building_doc_5()
{
    // We start with a default namespace urn:blue then later att an element
    // with an ancestor namespace with a prefix {c}urn:coffee.
    // This namespace does not exist and is added to the root node.
    const char *exp = "box(xmlns=urn:blue xmlns:c=urn:coffee){bar{c:price=99}}\n";
    // <box xmlns="urn:blue" xmlns:c="urn:coffee"><bar><c:price>99</c:price></bar></box>

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

void test_building_doc_6()
{
    // We start with a default namespace urn:blue then later add an element
    // with an ancestor namespace without a prefix urn:coffee.
    // This namespace does not exist and so a new unique prefix is selected.
    const char *exp = "box(xmlns=urn:blue xmlns:ns1=urn:coffee){bar{ns1:price=99}}\n";
    // <box xmlns="urn:blue" xmlns:ns1="urn:coffee"><bar><ns1:price>99</ns1:price></bar></box>

    XMQReturnDoc rd = xmqNewDoc();
    assert(rd.status == XMQ_OK);
    XMQDoc *doc = rd.doc;

    XMQReturnNode rn = xmqAddRootElement(doc, "box", NS_HERE("urn:blue"));
    assert(rn.status == XMQ_OK);
    XMQNode *box = rn.node;

    rn = xmqAddElement(doc, box, "bar", NS_PARENT);
    assert(rn.status == XMQ_OK);
    XMQNode *bar = rn.node;

    xmqAddKeyValue(doc, bar, "price", "99", NS_ANCESTOR("urn:coffee"));

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

void test_building_doc_7()
{
    // We start with a default namespace urn:blue then later
    // add key values with ANCESTOR namespaces. They are automatically
    // assigned unique prefixes.
    const char *exp = "box(xmlns=urn:blue xmlns:ns1=urn:soft xmlns:ns2=urn:bar){ns1:flower=many ns1:power=123 ns2:soft=petal}\n";
    // <box xmlns="urn:blue" xmlns:ns1="urn:soft" xmlns:ns2="urn:bar"><ns1:flower>many</ns1:flower><ns1:power>123</ns1:power><ns2:soft>petal</ns2:soft></box>
    XMQReturnDoc rd = xmqNewDoc();
    assert(rd.status == XMQ_OK);
    XMQDoc *doc = rd.doc;

    XMQReturnNode rn = xmqAddRootElement(doc, "box", NS_HERE("urn:blue"));
    assert(rn.status == XMQ_OK);
    XMQNode *box = rn.node;

    xmqAddKeyValue(doc, box, "flower", "many", NS_ANCESTOR("urn:soft")); // gets the prefix ns1
    xmqAddKeyValue(doc, box, "power", "123", NS_ANCESTOR("urn:soft")); // gets the same prefix.
    xmqAddKeyValue(doc, box, "soft", "petal", NS_ANCESTOR("urn:bar")); // gets a new prefix ns2.

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
        printf("Building of dom tree failed. Got: %s\nExpected: %s\n", start, exp);
        exit(1);
    }
    free(start);
}

void test_building_doc_8()
{
    // Add an explicit namespace before adding the child element. The NS_ANCESTOR
    // will find this namespace.
    const char *exp = "flower(xmlns=urn:power xmlns:s=urn:soft){s:petals=many}\n";
    // <flower xmlns="urn:power" xmlns:s="urn:soft"><s:petals>many</s:petals></flower>

    XMQReturnDoc rd = xmqNewDoc();
    assert(rd.status == XMQ_OK);
    XMQDoc *doc = rd.doc;

    XMQReturnNode rn = xmqAddRootElement(doc, "flower", NS_HERE("urn:power"));
    assert(rn.status == XMQ_OK);

    XMQStatus rc = xmqAddNamespace(doc, rn.node, NS_HERE("{s}urn:soft"));
    assert(rc == XMQ_OK);

    XMQNode *robot = rn.node;
    xmqAddKeyValue(doc, robot, "petals", "many", NS_ANCESTOR("urn:soft"));

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
        printf("Building of dom tree failed. Got: %s\nExpected: %s\n", start, exp);
        exit(1);
    }
    free(start);
}

int main(int argc, char **argv)
{
    test_building_doc_0();
    test_building_doc_1();
    test_building_doc_2();
    test_building_doc_3();
    test_building_doc_4();
    test_building_doc_5();
    test_building_doc_6();
    test_building_doc_7();
    test_building_doc_8();
}
