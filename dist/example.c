
#include"xmq.h"

#include<assert.h>
#include<stdio.h>
#include<string.h>
#include<sys/time.h>

void expect(const char *s, const char *e);
void expect_int(int32_t i, int32_t e);
void expect_double(double d, double e);

XMQProceed add_value(XMQDoc *doc, XMQNode *node, void *user_data);

void demonstrate_load_xmq_file()
{
    const char *file = "example.xmq";
    XMQReturnDoc rd = xmqNewDoc();
    assert(rd.status == XMQ_OK);
    XMQDoc *doc = rd.doc;

    bool ok = xmqParseFile(doc, file, "car", 0);
    if (!ok) {
        printf("Parse error in %s\n%s",
               file,
               xmqDocError(doc));
        exit(1);
    }
    const char *model = xmqGetString(doc, "/car/model");
    int32_t num_wheels = xmqGetInt(doc, "/car/num_wheels");
    double weight = xmqGetDouble(doc, "/car/weight");
    const char *not_found = xmqGetString(doc, "/car/not_found");
    const char *color = xmqGetString(doc, "/car/color");
    const char *history = xmqGetString(doc, "/car/history");

    expect(model, "EsCarGo");
    expect_int(num_wheels, 36);
    expect_double(weight, 999.123);

    xmqFreeDoc(doc);
}

void demonstrate_building_dom_0()
{
    XMQReturnDoc rd = xmqNewDoc();
    assert(rd.status == XMQ_OK);
    XMQDoc *doc = rd.doc;

    XMQReturnNode rn = xmqAddRootElement(doc, "greeting", NS_NONE);
    assert(rn.status == XMQ_OK);
    XMQNode *car = rn.node;
    rn = xmqAddKeyValue(doc, car, "hello", "world", NS_PARENT);
    assert(rn.status == XMQ_OK);

    XMQOutputSettings *os = xmqNewOutputSettings();

    xmqSetCompact(os, true);
    xmqSetOutputFormat(os, XMQ_CONTENT_XMQ);
    xmqSetRenderFormat(os, XMQ_RENDER_PLAIN);

    char *start, *stop;
    xmqSetupPrintMemory(os, &start, &stop);
    xmqPrint(doc, os);

    xmqFreeOutputSettings(os);

    const char *exp = "greeting{hello=world}\n";
    if (strcmp(start, exp))
    {
        printf("Building of dom 0 tree failed. Got: %s\nExpected: %s\n", start, exp);
        exit(1);
    }
    free(start);
}

void demonstrate_building_html()
{
    // Create html output.
    const char *exp = "<!DOCTYPE html>\n<html><body><a href=\"https://libxmq.org\"></a></body></html>\n";

    XMQReturnDoc page = xmqNewDoc();
    assert(page.status == XMQ_OK);
    XMQDoc *doc = page.doc;

    XMQStatus status = xmqSetDocType(doc, "html");
    assert(status == XMQ_OK);

    XMQReturnNode html = xmqAddRootElement(doc, "html", NS_NONE);
    assert(html.status == XMQ_OK);

    XMQReturnNode body = xmqAddElement(doc, html.node, "body", NS_PARENT);
    assert(body.status == XMQ_OK);

    XMQReturnNode a = xmqAddElementWithAttrs(doc, body.node, "a", NS_PARENT,
                                             XMQ_ATTRS( { "href", "https://libxmq.org" } ));
    assert(a.status == XMQ_OK);

    XMQOutputSettings *os = xmqNewOutputSettings();

    xmqSetCompact(os, true);
    xmqSetEscapeNewlines(os, true);
    xmqSetUseColor(os, false);
    xmqSetOutputFormat(os, XMQ_CONTENT_HTML);
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

void demonstrate_building_json()
{
    XMQReturnDoc rd = xmqNewDoc();
    assert(rd.status == XMQ_OK);
    XMQDoc *doc = rd.doc;

    XMQReturnNode rn = xmqAddRootElement(doc, "request", NS_NONE);
    assert(rn.status == XMQ_OK);
    XMQNode *request = rn.node;

    rn = xmqAddKeyValueWithAttrs(doc, request, "id", "123", NS_NONE,
                                 XMQ_ATTRS( { "S", "" } ));
    assert(rn.status == XMQ_OK);

    XMQReturnNode names = xmqAddElementWithAttrs(doc, request, "names", NS_NONE,
                                                 XMQ_ATTRS( { "A", "" } ));

    rn = xmqAddKeyValue(doc, names.node, "_", "samuel", NS_NONE);
    assert(rn.status == XMQ_OK);

    rn = xmqAddKeyValue(doc, names.node, "_", "isildur", NS_NONE);
    assert(rn.status == XMQ_OK);

    XMQOutputSettings *os = xmqNewOutputSettings();

    xmqSetCompact(os, true);
    xmqSetEscapeNewlines(os, true);
    xmqSetUseColor(os, false);
    xmqSetOutputFormat(os, XMQ_CONTENT_JSON);
    xmqSetRenderFormat(os, XMQ_RENDER_PLAIN);

    char *start, *stop;
    xmqSetupPrintMemory(os, &start, &stop);
    xmqPrint(doc, os);

    xmqFreeOutputSettings(os);

    const char *exp = "{\"_\":\"request\",\"id\":\"123\",\"names\":[\"samuel\",\"isildur\"]}\n";
    if (strcmp(start, exp))
    {
        printf("Building of dom tree failed. Got: %s\nExpected: %s\n", start, exp);
        exit(1);
    }
    free(start);
}

void demonstrate_building_dom_1()
{
    XMQReturnDoc rd = xmqNewDoc();
    assert(rd.status == XMQ_OK);
    XMQDoc *doc = rd.doc;

    XMQReturnNode rn = xmqAddRootElement(doc, "car", NS_HERE("urn:cargo"));
    assert(rn.status == XMQ_OK);
    XMQNode *car = rn.node;

    rn = xmqAddKeyValue(doc, car, "model", "escargo", NS_PARENT);
    assert(rn.status == XMQ_OK);

    rn = xmqAddKeyValue(doc, car, "color", "green", NS_PARENT);
    assert(rn.status == XMQ_OK);

    XMQOutputSettings *os = xmqNewOutputSettings();

    xmqSetCompact(os, true);
    xmqSetOutputFormat(os, XMQ_CONTENT_XMQ);
    xmqSetRenderFormat(os, XMQ_RENDER_PLAIN);

    char *start, *stop;
    xmqSetupPrintMemory(os, &start, &stop);
    xmqPrint(doc, os);

    xmqFreeOutputSettings(os);

    const char *exp = "car(xmlns=urn:cargo){model=escargo color=green}\n";
    if (strcmp(start, exp))
    {
        printf("Building of dom 1 tree failed. Got: %s\nExpected: %s\n", start, exp);
        exit(1);
    }
    free(start);
}

void demonstrate_building_dom_2()
{
    XMQReturnDoc rd = xmqNewDoc();
    assert(rd.status == XMQ_OK);
    XMQDoc *doc = rd.doc;

    XMQReturnNode rn = xmqAddRootElement(doc, "robot", NS_HERE("{krf}urn:kraftwerk"));
    assert(rn.status == XMQ_OK);
    XMQNode *robot = rn.node;
    rn = xmqAddKeyValue(doc, robot, "who", "we are", NS_PARENT);
    assert(rn.status == XMQ_OK);

    rn = xmqAddKeyValue(doc, robot, "the", "robots", NS_PARENT);
    assert(rn.status == XMQ_OK);

    rn = xmqAddElement(doc, robot, "car", NS_HERE("{c}urn:cargo"));
    assert(rn.status == XMQ_OK);

    rn = xmqAddKeyValue(doc, rn.node, "model", "escargo", NS_PARENT);
    assert(rn.status == XMQ_OK);

    rn = xmqAddElement(doc, robot, "box", NS_NONE);
    assert(rn.status == XMQ_OK);

    rn = xmqAddKeyValue(doc, rn.node, "color", "blue", NS_PARENT);
    assert(rn.status == XMQ_OK);

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

    const char *exp = "krf:robot(xmlns:krf=urn:kraftwerk){krf:who='we are'krf:the=robots c:car(xmlns:c=urn:cargo){c:model=escargo}box(xmlns=''){color=blue}}\n";
    if (strcmp(start, exp))
    {
        printf("Building of dom tree failed. Got: %s\nExpected: %s\n", start, exp);
        exit(1);
    }
    free(start);
}

void demonstrate_building_dom_3()
{
    XMQReturnDoc rd = xmqNewDoc();
    assert(rd.status == XMQ_OK);
    XMQDoc *doc = rd.doc;

    XMQReturnNode rn = xmqAddRootElement(doc, "flower", NS_HERE("urn:power"));
    assert(rn.status == XMQ_OK);

    XMQStatus rc = xmqAddNamespace(doc, rn.node, NS_HERE("{s}urn:soft"));
    assert(rc == XMQ_OK);

    XMQNode *robot = rn.node;
    rn = xmqAddKeyValue(doc, robot, "petals", "many", NS_ANCESTOR("urn:soft"));
    assert(rn.status == XMQ_OK);

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

    const char *exp = "flower(xmlns=urn:power xmlns:s=urn:soft){s:petals=many}\n";
    if (strcmp(start, exp))
    {
        printf("Building of dom tree failed. Got: %s\nExpected: %s\n", start, exp);
        exit(1);
    }
    free(start);
}

void demonstrate_building_dom_4()
{
    XMQReturnDoc rd = xmqNewDoc();
    assert(rd.status == XMQ_OK);
    XMQDoc *doc = rd.doc;

    XMQReturnNode rn = xmqAddRootElement(doc, "box", NS_HERE("urn:blue"));
    assert(rn.status == XMQ_OK);
    XMQNode *box = rn.node;

    rn = xmqAddKeyValue(doc, box, "flower", "many", NS_ANCESTOR("urn:soft"));
    assert(rn.status == XMQ_OK);

    rn = xmqAddKeyValue(doc, box, "power", "123", NS_ANCESTOR("urn:soft"));
    assert(rn.status == XMQ_OK);

    rn = xmqAddKeyValue(doc, box, "soft", "petal", NS_ANCESTOR("urn:bar"));
    assert(rn.status == XMQ_OK);

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

    const char *exp = "box(xmlns=urn:blue xmlns:ns1=urn:soft xmlns:ns2=urn:bar){ns1:flower=many ns1:power=123 ns2:soft=petal}\n";
    if (strcmp(start, exp))
    {
        printf("Building of dom tree failed. Got: %s\nExpected: %s\n", start, exp);
        exit(1);
    }
    free(start);
}

void demonstrate_ixml_parse()
{
    XMQReturnDoc rd = xmqNewDoc();
    assert(rd.status == XMQ_OK);
    XMQDoc *ixml = rd.doc;
    bool ok = xmqParseBufferWithType(ixml,
                                     "decode = -'a', B++-','. B=[N]+.",
                                     NULL, NULL, XMQ_CONTENT_IXML, 0);
    assert(ok);

    struct timeval stop, start;
    gettimeofday(&start, NULL);

    bool b = false;
    for (int i=0; i<10000; ++i)
    {
        XMQReturnDoc rd = xmqNewDoc();
        assert(rd.status == XMQ_OK);
        XMQDoc *decode = rd.doc;
        b = xmqParseBufferWithIXML(decode,
                                   "a123,2,444",
                                   NULL,
                                   ixml,
                                   0);
        int sum = 0;
        xmqForeach(decode, "//B", add_value, &sum);
        xmqFreeDoc(decode);

        if (sum != (123+2+444))
        {
            printf("Expected sum %d but got %d\n", (123+2+444), sum);
        }
    }
    xmqFreeDoc(ixml);

    gettimeofday(&stop, NULL);
    double time = (stop.tv_sec - start.tv_sec) * 1000000 + stop.tv_usec - start.tv_usec;
    int itime = (int)(time/10000);

    printf("each ixml parse and foreach took %d us\n", itime);
}

void demonstrate_xmq_line_printf()
{
    int num_wheels = 36;

    XMQLineConfig *lc = xmqNewLineConfig();
    char *line = xmqLinePrintf(lc,
                               "car{",
                               "nw=", "%d", num_wheels,
                               "model=", "%s %d", "car go ", 3,
                               "decription=", "%s", "howdy\ndowdy",
                               "more=", "'''%s'''", "===",
                               "key=", "",
                               "}");
    const char *expect = "car{nw=36 model='car go  3'decription=('howdy'&#10;'dowdy')more=\"'''==='''\"key=''}";
    if (strcmp(line, expect))
    {
        printf("Expected >%s<\n but got >%s<\n", expect, line);
    }

    free(line);

    line = xmqLinePrintf(lc, "work=", "pi is %f", 3.141590);

    expect = "work='pi is 3.141590'";
    if (strcmp(line, expect))
    {
        printf("Expected >%s<\n but got >%s<\n", expect, line);
    }

    xmqSetLineHumanReadable(lc, true);
    line = xmqLinePrintf(lc, "work=", "pi is %f", 3.141590);

    expect = "(work) pi is 3.141590";
    if (strcmp(line, expect))
    {
        printf("Expected >%s<\n but got >%s<\n", expect, line);
    }
}

int main(int argc, char **argv)
{
    demonstrate_building_dom_0();
    demonstrate_building_html();
    demonstrate_building_json();
    demonstrate_building_dom_1();
    demonstrate_building_dom_2();
    demonstrate_building_dom_3();
    demonstrate_building_dom_4();

    demonstrate_load_xmq_file();

    demonstrate_ixml_parse();

    demonstrate_xmq_line_printf();

    return 0;
}

XMQProceed add_value(XMQDoc *doc, XMQNode *node, void *user_data)
{
    int *sum = (int*)user_data;
    int v = xmqGetIntRel(doc, ".", node);
    *sum += v;
    return XMQ_CONTINUE;
}

void expect(const char *s, const char *e)
{
    if (strcmp(s, e))
    {
        printf("Expected %s but got %s\n", e, s);
        exit(1);
    }
}

void expect_int(int32_t i, int32_t e)
{
    if (i != e)
    {
        printf("Expected %d but got %d\n", e, i);
        exit(1);
    }
}

void expect_double(double d, double e)
{
    if (d != e)
    {
        printf("Expected %f but got %f\n", e, d);
        exit(1);
    }
}
