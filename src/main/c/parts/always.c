
#ifndef BUILDING_DIST_XMQ

#include"always.h"
#include"membuffer.h"
#include<stdio.h>
#include<stdlib.h>
#include<assert.h>
#include<string.h>
#include<stdarg.h>

char *xmqLineVPrintf(XMQLineConfig *lc, const char *element_name, va_list ap);

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

XMQLineConfig xmq_log_line_config_;

void error__(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    char *line = xmqLineVPrintf(&xmq_log_line_config_, fmt, args);
    fprintf(stderr, "%s\n", line);
    free(line);
    va_end(args);
}

void warning__(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    char *line = xmqLineVPrintf(&xmq_log_line_config_, fmt, args);
    fprintf(stderr, "%s\n", line);
    free(line);
    va_end(args);
}

bool xmq_verbose_enabled_ = false;

void verbose__(const char* fmt, ...)
{
    if (xmq_verbose_enabled_)
    {
        if (!xmq_log_filter_ || !strncmp(fmt, xmq_log_filter_, strlen(xmq_log_filter_)))
        {
            va_list args;
            va_start(args, fmt);
            char *line = xmqLineVPrintf(&xmq_log_line_config_, fmt, args);
            fprintf(stderr, "%s\n", line);
            free(line);
            va_end(args);
        }
    }
}

bool xmq_debug_enabled_ = false;

void debug__(const char* fmt, ...)
{
    assert(fmt);

    if (xmq_debug_enabled_)
    {
        if (!xmq_log_filter_ || !strncmp(fmt, xmq_log_filter_, strlen(xmq_log_filter_)))
        {
            va_list args;
            va_start(args, fmt);
            char *line = xmqLineVPrintf(&xmq_log_line_config_, fmt, args);
            fprintf(stderr, "%s\n", line);
            free(line);
            va_end(args);
        }
    }
}

void debug_mb__(const char* module, MemBuffer *mb)
{
    if (xmq_debug_enabled_)
    {
        if (!xmq_log_filter_ || !strncmp(module, xmq_log_filter_, strlen(xmq_log_filter_)))
        {
            debug__(module, "%.*s", mb->used_, mb->buffer_);
        }
    }
}

bool xmq_trace_enabled_ = false;
const char *xmq_log_filter_ = NULL;

void trace__(const char* fmt, ...)
{
    if (xmq_trace_enabled_)
    {
        if (!xmq_log_filter_ || !strncmp(fmt, xmq_log_filter_, strlen(xmq_log_filter_)))
        {
            va_list args;
            va_start(args, fmt);
            char *line = xmqLineVPrintf(&xmq_log_line_config_, fmt, args);
            fprintf(stderr, "%s\n", line);
            free(line);
            va_end(args);
        }
    }
}

void trace_mb__(const char* module, MemBuffer *mb)
{
    if (xmq_trace_enabled_)
    {
        if (!xmq_log_filter_ || !strncmp(module, xmq_log_filter_, strlen(xmq_log_filter_)))
        {
            trace__(module, "%.*s", mb->used_, mb->buffer_);
        }
    }
}

char *buf_vsnprintf(const char *format, va_list ap)
{
    size_t buf_size = 1024;
    char *buf = (char*)malloc(buf_size);

    for (;;)
    {
        va_list tmp;
        va_copy(tmp, ap);
        size_t n = vsnprintf(buf, buf_size, format, tmp);
        va_end(tmp);

        if (n < buf_size) break;
        buf_size = n+32;
        free(buf);
        // Why not realloc? Well, we are goint to redo the printf anyway overwriting
        // the buffer again. So why allow realloc to spend time copying the content before
        // it is overwritten?
        buf = (char*)malloc(buf_size);
    }
    return buf;
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

static char *helper(size_t scale, size_t s, const char *suffix)
{
    size_t o = s;
    s /= scale;
    size_t diff = o-(s*scale);
    if (diff == 0) {
        char *buf = (char*)malloc(64);
        snprintf(buf, 64, "%zu.00%s", s, suffix);
        return buf;
    }
    size_t dec = (int)(100*(diff+1) / scale);
    char *buf = (char*)malloc(64);
    snprintf(buf, 64, "%zu.%02zu%s", s, dec, suffix);
    return buf;
}

#define KB 1024

char *humanReadableTwoDecimals(size_t s)
{
    if (s < KB)
    {
        return helper(1, s, " B");
    }
    if (s < KB * KB)
    {
        return helper(KB, s, " KiB");
    }
    if (s < KB * KB * KB)
    {
        return helper(KB*KB, s, " MiB");
    }
#if SIZEOF_SIZE_T == 8
    if (s < KB * KB * KB * KB)
    {
     	return helper(KB*KB*KB, s, " GiB");
    }
    if (s < KB * KB * KB * KB * KB)
    {
        return helper(KB*KB*KB*KB, s, " TiB");
    }
    return helper(KB*KB*KB*KB*KB, s, " PiB");
#else
    return helper(KB*KB*KB, s, " GiB");
#endif
}


#endif // ALWAYS_MODULE
