// libxmq - Copyright 2023 Fredrik Öhrström (spdx: MIT)

#include<stdio.h>
#include<string.h>
#include<stdbool.h>

bool expectString(char *test, const char *s, const char *e)
{
    if (e == NULL || s == NULL || strcmp(s, e))
    {
        printf("ERR: %s expected \"%s\" but got \"%s\"\n", test, e, s);
        return false;
    }
    return true;
}

bool expectInteger(char *test, int s, int e)
{
    if (s != e)
    {
        printf("ERR: %s expected %d but got %d\n", test, e, s);
        return false;
    }
    return true;
}

bool expectDouble(char *test, double s, double e)
{
    if (s != e)
    {
        printf("ERR: %s expected %f but got %f\n", test, e, s);
        return false;
    }
    return true;
}
