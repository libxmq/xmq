
#ifndef BUILDING_XMQ

#include"always.h"
#include<stdio.h>
#include<stdlib.h>
#include<assert.h>
#include<string.h>
#include<stdarg.h>

#endif

#ifdef ALWAYS_MODULE

void check_malloc(void *a)
{
    if (!a)
    {
        PRINT_ERROR("libxmq: Out of memory!\n");
        exit(1);
    }
}

bool verbose_enabled_ = false;

void verbose__(const char* fmt, ...)
{
    if (verbose_enabled_) {
        va_list args;
        va_start(args, fmt);
        vfprintf(stderr, fmt, args);
        va_end(args);
    }
}

bool debug_enabled_ = false;

void debug__(const char* fmt, ...)
{
    if (debug_enabled_) {
        va_list args;
        va_start(args, fmt);
        vfprintf(stderr, fmt, args);
        va_end(args);
    }
}

#ifdef PLATFORM_WINAPI
char *strndup(const char *s, size_t l)
{
    char *buf = (char*)malloc(l+1);
    size_t i = 0;
    for (; i < l; ++i)
    {
        buf[i] = s[i];
        if (!s[i]) break;
    }
    if (i < l)
    {
        // The string s was shorter than l. We have already copied the null terminator.
        // Just resize.
        buf = (char*)realloc(buf, i+1);
    }
    else
    {
        // We copied l bytes. Store a null-terminator in the position i (== l).
        buf[i] = 0;
    }
    return buf;
}
#endif


#endif // ALWAYS_MODULE
