
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

void verbose(const char* fmt, ...)
{
    if (verbose_enabled_) {
        va_list args;
        va_start(args, fmt);
        vfprintf(stderr, fmt, args);
        va_end(args);
    }
}

bool debug_enabled_ = false;

void debug(const char* fmt, ...)
{
    if (debug_enabled_) {
        va_list args;
        va_start(args, fmt);
        vfprintf(stderr, fmt, args);
        va_end(args);
    }
}

#endif // ALWAYS_MODULE
