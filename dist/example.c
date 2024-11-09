
#include"xmq.h"

#include<stdio.h>
#include<string.h>

void expect(const char *s, const char *e);
void expect_int(int32_t i, int32_t e);
void expect_double(double d, double e);

int main(int argc, char **argv)
{
    const char *file = "example.xmq";
    XMQDoc *doc = xmqNewDoc();

    bool ok = xmqParseFile(doc, file, "car", 0);
    if (!ok) {
        printf("Parse error in %s\n%s",
               file,
               xmqDocError(doc));
        return 1;
    }
    const char *model = xmqGetString(doc, "/car/model");
    int32_t num_wheels = xmqGetInt(doc, "/car/num_wheels");
    double weight = xmqGetDouble(doc, "/car/weight");
    const char *not_found = xmqGetString(doc, "/car/not_found");
    const char *color = xmqGetString(doc, "/car/color");
    const char *history = xmqGetString(doc, "/car/history");

    char *line = xmqLogElement("car{",
                               "nw=", "%d", num_wheels,
                               "model=", "%d", 42,
                               "}");
    printf("%s\n", line);

    expect(model, "EsCarGo");
    expect_int(num_wheels, 36);
    expect_double(weight, 999.123);

    xmqFreeDoc(doc);

    return ok ? 0 : 1;
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
