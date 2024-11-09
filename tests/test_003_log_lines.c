
#include<xmq.h>
#include"test.h"

char *test = __FILE__;

int main(int argc, char **argv)
{
    bool ok = true;

    int id = 123;
    char *user = "fredrik";
    char *first_name = "Ann";
    char *initials = "K.";
    char *surname = "Mantra";

    char *l1 = xmqLogElement("login{",
                             "user=", "%s", user,
                             "id=", "%d", id,
                             "name=", "%s %s %s", first_name, initials, surname,
                             "date=", "%04d-%02d-%02d", 2024, 10, 23,
                             "}");

    const char *expected = "login{user=fredrik id=123 name='Ann K. Mantra' date=2024-10-23}";
    if (strcmp(l1, expected))
    {
        printf("EXPECTED >%s<\nBUT GOT  >%s<\n", expected, l1);
        ok = false;
    }
    free(l1);

    return ok ? 0 : 1;
}
