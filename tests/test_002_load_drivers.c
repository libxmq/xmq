
#include<assert.h>
#include<xmq.h>
#include"test.h"

char *test = __FILE__;

XMQProceed add_field(XMQDoc *doc, XMQNode *field, void *user_data)
{
    const char *name = xmqGetStringRel(doc, "name", field);

    printf("add %s %s\n", xmqGetName(field), name);

    return XMQ_CONTINUE;
}

XMQProceed add_driver(XMQDoc *doc, XMQNode *driver, void *user_data)
{
    const char *name = xmqGetStringRel(doc, "name", driver);
    int32_t trigger = xmqGetIntRel(doc, "trigger", driver);

    printf("add %s %s %d\n", xmqGetName(driver), name, trigger);

    XMQNode *node = xmqGetNodeRel(doc, "trigger", driver);
    xmqSetContent(node, "hejsan");
    const char *t = xmqGetContent(node);
    if (strcmp(t, "hejsan"))
    {
        printf("Internal error expected \"hejsan\" but got \"%s\".\n", t);
        exit(1);
    }

    xmqForeachRel(doc, "field", add_field, NULL, driver);
    return XMQ_CONTINUE;
}

int main(int argc, char **argv)
{
    XMQReturnDoc rd = xmqNewDoc();
    assert(rd.status == XMQ_OK);
    XMQDoc *doc = rd.doc;

    bool ok = xmqParseFile(doc, argv[1], "config", 0);
    if (!ok) {
        printf("Could not load file %s.\n", argv[1]);
        return 1;
    }

    xmqForeach(doc, "/config/driver", add_driver, NULL);

    xmqFreeDoc(doc);

    return ok ? 0 : 1;
}
