
#ifndef BUILDING_XMQ

#include"utf8.h"
#include"parts/xmq_internals.h"

#endif

#ifdef UTF8_MODULE

size_t print_utf8_char(XMQPrintState *ps, const char *start, const char *stop)
{
    XMQOutputSettings *os = ps->output_settings;
    XMQWrite write = os->content.write;
    void *writer_state = os->content.writer_state;

    const char *i = start;

    // Find next utf8 char....
    const char *j = i+1;
    while (j < stop && (*j & 0xc0) == 0x80) j++;

    // Is the utf8 char a unicode whitespace and not space,tab,cr,nl?
    bool uw = is_unicode_whitespace(i, j);
    //bool tw = *i == '\t';

    // If so, then color it. This will typically red underline the non-breakable space.
    if (uw) print_color_pre(ps, COLOR_unicode_whitespace);

    if (*i == ' ')
    {
        write(writer_state, os->explicit_space, NULL);
    }
    else
    if (*i == '\t')
    {
        write(writer_state, os->explicit_tab, NULL);
    }
    else
    {
        const char *e = needs_escape(ps->output_settings->render_to, i, j);
        if (!e)
        {
            write(writer_state, i, j);
        }
        else
        {
            write(writer_state, e, NULL);
        }
    }
    if (uw) print_color_post(ps, COLOR_unicode_whitespace);

    ps->last_char = *i;
    ps->current_indent++;

    return j-start;
}

/**
   print_utf8_internal: Print a single string
   ps: The print state.
   start: Points to bytes to be printed.
   stop: Points to byte after last byte to be printed. If NULL then assume start is null-terminated.

   Returns number of bytes printed.
*/
size_t print_utf8_internal(XMQPrintState *ps, const char *start, const char *stop)
{
    if (stop == start || *start == 0) return 0;

    XMQOutputSettings *os = ps->output_settings;
    XMQWrite write = os->content.write;
    void *writer_state = os->content.writer_state;

    size_t u_len = 0;

    const char *i = start;
    while (*i && (!stop || i < stop))
    {
        // Find next utf8 char....
        const char *j = i+1;
        while (j < stop && (*j & 0xc0) == 0x80) j++;

        // Is the utf8 char a unicode whitespace and not space,tab,cr,nl?
        bool uw = is_unicode_whitespace(i, j);
        //bool tw = *i == '\t';

        // If so, then color it. This will typically red underline the non-breakable space.
        if (uw) print_color_pre(ps, COLOR_unicode_whitespace);

        if (*i == ' ')
        {
            write(writer_state, os->explicit_space, NULL);
        }
        else if (*i == '\t')
        {
            write(writer_state, os->explicit_tab, NULL);
        }
        else
        {
            const char *e = needs_escape(ps->output_settings->render_to, i, j);
            if (!e)
            {
                write(writer_state, i, j);
            }
            else
            {
                write(writer_state, e, NULL);
            }
        }
        if (uw) print_color_post(ps, COLOR_unicode_whitespace);
        u_len++;
        i = j;
    }

    ps->last_char = *(i-1);
    ps->current_indent += u_len;
    return i-start;
}

/**
   print_utf8:
   @ps: The print state.
   @c:  The color.
   @num_pairs:  Number of start, stop pairs.
   @start: First utf8 byte to print.
   @stop: Points to byte after the last utf8 content.

   Returns the number of bytes used after start.
*/
size_t print_utf8(XMQPrintState *ps, XMQColor color, size_t num_pairs, ...)
{
    XMQOutputSettings *os = ps->output_settings;
    XMQWrite write = os->content.write;
    void *writer_state = os->content.writer_state;

    const char *pre, *post;
    getThemeStrings(os, color, &pre, &post);
    const char *previous_color = NULL;

    if (pre)
    {
        write(writer_state, pre, NULL);
        previous_color = ps->replay_active_color_pre;
        ps->replay_active_color_pre = pre;
    }

    size_t b_len = 0;

    va_list ap;
    va_start(ap, num_pairs);
    for (size_t x = 0; x < num_pairs; ++x)
    {
        const char *start = va_arg(ap, const char *);
        const char *stop = va_arg(ap, const char *);
        b_len += print_utf8_internal(ps, start, stop);
    }
    va_end(ap);

    if (post)
    {
        write(writer_state, post, NULL);
    }
    if (previous_color)
    {
        ps->replay_active_color_pre = previous_color;
    }

    return b_len;
}

#endif // UTF8_MODULE
