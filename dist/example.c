
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

    expect(model, "EsCarGo");
    expect_int(num_wheels, 36);
    expect_double(weight, 999.123);

    xmqFreeDoc(doc);
    XMQLineConfig *lc = xmqNewLineConfig();
    char *line = xmqLinePrintf(lc,
                               "car{",
                               "nw=", "%d", num_wheels,
                               "model=", "%s %d", "car go ", 3,
                               "decription=", "%s", "howdy\ndowdy",
                               "more=", "'''%s'''", "===",
                               "key=", "",
                               "}");
    const char *expect = "car{nw=36 model='car go  3'decription=('howdy'&#10;'dowdy')more=(&#39;&#39;&#39;'==='&#39;&#39;&#39;)key=''}";
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

    return 0;
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
