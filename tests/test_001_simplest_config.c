
#include<xmq.h>
#include"test.h"

char *test = __FILE__;

int main(int argc, char **argv)
{
    XMQDoc *doc = xmqNewDoc();

    bool ok = xmqParseFile(doc, argv[1], "car", 0);
    if (!ok) {
        printf("Could not load file %s.\n", argv[1]);
        return 1;
    }
    const char *model = xmqGetString(doc, "/car/model");
    int32_t speed = xmqGetInt(doc, "/car/speed");
    double weight = xmqGetDouble(doc, "/car/weight");
    const char *registration = xmqGetString(doc, "/car/registration");
    const char *color = xmqGetString(doc, "/car/color");
    const char *history = xmqGetString(doc, "/car/history");

    ok = true;

    ok &= expectString(test, model, "Saab");
    ok &= expectInteger(test, speed, 123);
    ok &= expectDouble(test, weight, 500.123);
    ok &= expectString(test, registration, "ABC 999");
    ok &= expectString(test, color, "red");
    ok &= expectString(test, history, "Bought 1983\nSold   1999");

    xmqFreeDoc(doc);

    return ok ? 0 : 1;
}
