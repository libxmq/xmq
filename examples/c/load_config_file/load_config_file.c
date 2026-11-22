/* Load a configuration file written in xmq and read values from it using xpaths.
   The file car.xmq is parsed into a document, then strings, integers and doubles
   are fetched with xmqGetString, xmqGetInt and xmqGetDouble. */

#include "xmq.h"

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    const char *file = argc > 1 ? argv[1] : "car.xmq";

    XMQReturnDoc rd = xmqNewDoc();
    if (rd.status != XMQ_OK) return 1;
    XMQDoc *doc = rd.doc;

    // The implicit root "car" is used if the file has no root element.
    bool ok = xmqParseFile(doc, file, "car", 0);
    if (!ok)
    {
        printf("Parse error in %s\n%s", file, xmqDocError(doc));
        xmqFreeDoc(doc);
        return 1;
    }

    const char *model = xmqGetString(doc, "/car/model");
    int32_t num_wheels = xmqGetInt(doc, "/car/num_wheels");
    double weight = xmqGetDouble(doc, "/car/weight");
    const char *history = xmqGetString(doc, "/car/history");
    // A missing value returns NULL.
    const char *color = xmqGetString(doc, "/car/color");

    printf("model      = %s\n", model);
    printf("num_wheels = %d\n", num_wheels);
    printf("weight     = %.3f\n", weight);
    printf("color      = %s\n", color ? color : "(not set)");
    printf("history    =\n%s\n", history);

    xmqFreeDoc(doc);
    return 0;
}
