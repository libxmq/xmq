#ifndef BUILDING_XMQ

#include"utils.h"
#include"membuffer.h"
#include<assert.h>
#include<stdbool.h>
#include<string.h>

#endif

#ifdef MEMBUFFER_MODULE

/** Allocate a automatically growing membuffer. */
MemBuffer *new_membuffer()
{
    MemBuffer *mb = (MemBuffer*)malloc(sizeof(MemBuffer));
    check_malloc(mb);
    memset(mb, 0, sizeof(*mb));
    return mb;
}

/** Free the MemBuffer support struct but return the actual contents. */
char *free_membuffer_but_return_trimmed_content(MemBuffer *mb)
{
    char *b = mb->buffer_;
    b = (char*)realloc(b, mb->used_);
    free(mb);
    return b;
}

void free_membuffer_and_free_content(MemBuffer *mb)
{
    if (mb->buffer_) free(mb->buffer_);
    mb->buffer_ = NULL;
    free(mb);
}

size_t pick_buffer_new_size(size_t max, size_t used, size_t add)
{
    assert(used <= max);
    if (used + add > max)
    {
        // Increment buffer with 1KiB.
        max = max + 1024;
    }
    if (used + add > max)
    {
        // Still did not fit? The add was more than 1 Kib. Lets add that.
        max = max + add;
    }
    assert(used + add <= max);

    return max;
}

void membuffer_append_region(MemBuffer *mb, const char *start, const char *stop)
{
    if (!start) return;
    if (!stop) stop = start + strlen(start);
    size_t add = stop-start;
    size_t max = pick_buffer_new_size(mb->max_, mb->used_, add);
    if (max > mb->max_)
    {
        mb->buffer_ = (char*)realloc(mb->buffer_, max);
        mb->max_ = max;
    }
    memcpy(mb->buffer_+mb->used_, start, add);
    mb->used_ += add;
}

void membuffer_append(MemBuffer *mb, const char *start)
{
    const char *i = start;
    char *to = mb->buffer_+mb->used_;
    const char *stop = mb->buffer_+mb->max_;

    while (*i)
    {
        if (to >= stop)
        {
            mb->used_ = to - mb->buffer_;
            size_t max = pick_buffer_new_size(mb->max_, mb->used_, 1);
            assert(max >= mb->max_);
            mb->buffer_ = (char*)realloc(mb->buffer_, max);
            mb->max_ = max;
            stop = mb->buffer_+mb->max_;
            to = mb->buffer_+mb->used_;
        }
        *to = *i;
        i++;
        to++;
    }
    mb->used_ = to-mb->buffer_;
}

void membuffer_append_char(MemBuffer *mb, char c)
{
    size_t max = pick_buffer_new_size(mb->max_, mb->used_, 1);
    if (max > mb->max_)
    {
        mb->buffer_ = (char*)realloc(mb->buffer_, max);
        mb->max_ = max;
    }
    memcpy(mb->buffer_+mb->used_, &c, 1);
    mb->used_ ++;
}

void membuffer_append_null(MemBuffer *mb)
{
    membuffer_append_char(mb, 0);
}

void membuffer_append_entity(MemBuffer *mb, char c)
{
    if (c == ' ') membuffer_append(mb, "&#32;");
    else if (c == '\n') membuffer_append(mb, "&#10;");
    else if (c == '\t') membuffer_append(mb, "&#9;");
    else if (c == '\r') membuffer_append(mb, "&#13;");
    else
    {
        assert(false);
    }
}

#endif // MEMBUFFER_MODULE
