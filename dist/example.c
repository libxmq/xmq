
#include"xmq.h"

#include<stdio.h>

int main(int argc, char **argv)
{
    const char *file = "example.xmq";
    XMQDoc *doc = xmqNewDoc();

    bool ok = xmqParseFile(doc, file, "car");
    if (!ok) {
        printf("Could not load file %s.\n", file);
        return 1;
    }
    const char *model = xmqGetString(doc, NULL, "/car/model");
    int32_t speed = xmqGetInt(doc, NULL, "/car/speed");
    double weight = xmqGetDouble(doc, NULL, "/car/weight");
    const char *registration = xmqGetString(doc, NULL, "/car/registration");
    const char *color = xmqGetString(doc, NULL, "/car/color");
    const char *history = xmqGetString(doc, NULL, "/car/history");

    xmqFreeDoc(doc);

    return ok ? 0 : 1;
}
