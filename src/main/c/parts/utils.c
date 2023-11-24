
#ifndef BUILDING_XMQ

#include"utils.h"
#include<stdio.h>
#include<stdlib.h>
#include<assert.h>
#include<string.h>

#endif

#ifdef UTILS_MODULE

void check_malloc(void *a)
{
    if (!a)
    {
        PRINT_ERROR("libxmq: Out of memory!\n");
        exit(1);
    }
}

#endif // UTILS_MODULE
