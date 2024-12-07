
#include<xmq.h>
#include"test.h"

char *test = __FILE__;

bool ok = true;

void expect(const char *q, const char *expected)
{
    if (strcmp(q, expected))
    {
        printf("EXPECTED >%s<\nBUT GOT  >%s<\n", expected, q);
        ok = false;
    }
}

void testq(const char *c, const char *e)
{
    char *q = xmqCompactQuote(c);
    expect(q, e);
    free(q);
}

int main(int argc, char **argv)
{
    testq("123", "123");
    testq("John", "John");
    testq("John Doe", "'John Doe'");
    testq("There's light!", "'''There's light!'''");
    testq("\na line\n", "(&#10;'a line'&#10;)");
    return ok ? 0 : 1;
}
