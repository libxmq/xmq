
#ifndef BUILDING_XMQ

#include"text.h"
#include<stdio.h>
#include<stdlib.h>
#include<assert.h>
#include<string.h>

#endif

#ifdef TEXT_MODULE

bool is_lowercase_hex(char c)
{
    return
        (c >= '0' && c <= '9') ||
        (c >= 'a' && c <= 'f');
}

size_t num_utf8_bytes(char c)
{
    if ((c & 0x80) == 0) return 1;
    if ((c & 0xe0) == 0xc0) return 2;
    if ((c & 0xf0) == 0xe0) return 3;
    if ((c & 0xf8) == 0xf0) return 4;
    return 0; // Error
}

/**
   peek_utf8_char: Peek 1 to 4 chars from s belonging to the next utf8 code point and store them in uc.
   @start: Read utf8 from this string.
   @stop: Points to byte after last byte in string. If NULL assume start is null terminated.
   @uc: Store the UTF8 char here.

   Return the number of bytes peek UTF8 char, use this number to skip ahead to next char in s.
*/
size_t peek_utf8_char(const char *start, const char *stop, UTF8Char *uc)
{
    char a = *start;
    size_t n = num_utf8_bytes(a);

    if (n == 1)
    {
        uc->bytes[0] = a;
        uc->bytes[1] = 0;
        uc->bytes[2] = 0;
        uc->bytes[3] = 0;
        return 1;
    }

    char b = *(start+1);
    if (n == 2)
    {
        uc->bytes[0] = a;
        uc->bytes[1] = b;
        uc->bytes[2] = 0;
        uc->bytes[3] = 0;
        return 2;
    }

    char c = *(start+2);
    if (n == 3)
    {
        uc->bytes[0] = a;
        uc->bytes[1] = b;
        uc->bytes[2] = c;
        uc->bytes[3] = 0;
        return 3;
    }

    char d = *(start+3);
    if (n == 4)
    {
        uc->bytes[0] = a;
        uc->bytes[1] = b;
        uc->bytes[2] = c;
        uc->bytes[3] = d;
        return 4;
    }

    return 0;
}

/**
   utf8_char_to_codepoint_string: Decode an utf8 char and store as a string "U+123"
   @uc: The utf8 char to decode.
   @buf: Store the codepoint string here must have space for 9 bytes, i.e. U+10FFFF and a NULL byte.
*/
bool utf8_char_to_codepoint_string(UTF8Char *uc, char *buf)
{
    int cp = 0;
    size_t len = 0;
    bool ok = decode_utf8(uc->bytes, uc->bytes+4, &cp, &len);
    if (!ok)
    {
        snprintf(buf, 16, "U+error");
        return false;
    }
    snprintf(buf, 16, "U+%X", cp);
    return true;
}

/**
   decode_utf8: Peek 1 to 4 chars from start and calculate unicode codepoint.
   @start: Read utf8 from this string.
   @stop: Points to byte after last byte in string. If NULL assume start is null terminated.
   @out_char: Store the unicode code point here.
   @out_len: How many bytes the utf8 char used.

   Return true if valid utf8 char.
*/
bool decode_utf8(const char *start, const char *stop, int *out_char, size_t *out_len)
{
    int c = (int)(unsigned char)(*start);

    if ((c & 0x80) == 0)
    {
        *out_char = c;
        *out_len = 1;
        return true;
    }

    if ((c & 0xe0) == 0xc0)
    {
        if (start+1 < stop)
        {
            unsigned char cc = *(start+1);
            if ((cc & 0xc0) == 0x80)
            {
                *out_char = ((c & 0x1f) << 6) | (cc & 0x3f);
                *out_len = 2;
                return true;
            }
        }
    }
    else if ((c & 0xf0) == 0xe0)
    {
        if (start+2 < stop)
        {
            unsigned char cc = *(start+1);
            unsigned char ccc = *(start+2);
            if (((cc & 0xc0) == 0x80) && ((ccc & 0xc0) == 0x80))
            {
                *out_char = ((c & 0x0f) << 12) | ((cc & 0x3f) << 6) | (ccc & 0x3f) ;
                *out_len = 3;
                return true;
            }
        }
    }
    else if ((c & 0xf8) == 0xf0)
    {
        if (start+3 < stop)
        {
            unsigned char cc = *(start+1);
            unsigned char ccc = *(start+2);
            unsigned char cccc = *(start+3);
            if (((cc & 0xc0) == 0x80) && ((ccc & 0xc0) == 0x80) && ((cccc & 0xc0) == 0x80))
            {
                *out_char = ((c & 0x07) << 18) | ((cc & 0x3f) << 12) | ((ccc & 0x3f) << 6) | (cccc & 0x3f);
                *out_len = 4;
                return true;
            }
        }
    }

    // Illegal utf8.
    *out_char = 1;
    *out_len = 1;
    return false;
}

/**
    str_b_u_len: Count bytes and unicode characters.
    @start:
    @stop
    @b_len:
    @u_len:

    Store the number of actual bytes. Which is stop-start, and strlen(start) if stop is NULL.
    Count actual unicode characters between start and stop (ie all bytes not having the two msb bits set to 0x10xxxxxx).
    This will have to be improved if we want to handle indentation with characters with combining diacritics.
*/
void str_b_u_len(const char *start, const char *stop, size_t *b_len, size_t *u_len)
{
    assert(start);
    if (stop)
    {
        *b_len = stop - start;
        size_t u = 0;
        for (const char *i = start; i < stop; ++i)
        {
            if ((*i & 0xc0) != 0x80) u++;
        }
        *u_len = u;
        return;
    }

    size_t b = 0;
    size_t u = 0;
    for (const char *i = start; *i != 0; ++i)
    {
        if ((*i & 0xc0) != 0x80) u++;
        b++;
    }
    *b_len = b;
    *u_len = u;
}

bool is_xmq_text_name(char c)
{
    if (c >= 'a' && c <= 'z') return true;
    if (c >= 'A' && c <= 'Z') return true;
    if (c >= '0' && c <= '9') return true;
    if (c == '-' || c == '_' || c == '.' || c == ':' || c == '#') return true;
    return false;
}

bool is_xmq_element_start(char c)
{
    if (c >= 'a' && c <= 'z') return true;
    if (c >= 'A' && c <= 'Z') return true;
    if (c == '_') return true;
    return false;
}

bool is_xmq_element_name(const char *start, const char *stop)
{
    const char *i = start;
    if (!is_xmq_element_start(*i)) return false;
    i++;

    for (; i < stop; ++i)
    {
        char c = *i;
        if (!is_xmq_text_name(c)) return false;
    }

    return true;
}

char to_hex(int c)
{
    if (c >= 0 && c <= 9) return '0'+c;
    return 'A'-10+c;
}

/**
    xmq_quote_as_c:

    Escape the in string using c/json quotes. I.e. Surround with " and newline becomes \n and " become \" etc.
*/
char *xmq_quote_as_c(const char *start, const char *stop)
{
    if (!stop) stop = start+strlen(start);
    if (stop == start)
    {
        char *tmp = (char*)malloc(1);
        tmp[0] = 0;
        return tmp;
    }
    assert(stop > start);
    size_t len = 1+(stop-start)*4; // Worst case expansion of all chars.
    char *buf = (char*)malloc(len);

    const char *i = start;
    char *o = buf;
    size_t real = 0;

    for (; i < stop; ++i)
    {
        UTF8Char uc;
        size_t n = peek_utf8_char(i, stop, &uc);
        if (n > 1)
        {
            while (n) {
                *o++ = *i;
                real++;
                n--;
                i++;
            }
            i--;
            continue;
        }
        char c = *i;
        if (c >= ' ' && c <= 126 && c != '"') { *o++ = *i; real++;}
        else if (c == '"') { *o++ = '\\'; *o++ = '"'; real+=2; }
        else if (c == '\a') { *o++ = '\\'; *o++ = 'a'; real+=2; }
        else if (c == '\b') { *o++ = '\\'; *o++ = 'b'; real+=2; }
        else if (c == '\t') { *o++ = '\\'; *o++ = 't'; real+=2; }
        else if (c == '\n') { *o++ = '\\'; *o++ = 'n'; real+=2; }
        else if (c == '\v') { *o++ = '\\'; *o++ = 'v'; real+=2; }
        else if (c == '\f') { *o++ = '\\'; *o++ = 'f'; real+=2; }
        else if (c == '\r') { *o++ = '\\'; *o++ = 'r'; real+=2; }
        else { *o++ = '\\'; *o++ = 'x'; *o++ = to_hex((c>>4)&0xf); *o++ = to_hex(c&0xf); real+=4; }
    }
    real++;
    *o = 0;
    buf = (char*)realloc(buf, real);
    return buf;
}

/**
    xmq_unquote_as_c:

    Unescape the in string using c/json quotes. I.e. Replace \" with ", \n with newline etc.
*/
char *xmq_unquote_as_c(const char *start, const char *stop)
{
    if (stop == start)
    {
        char *tmp = (char*)malloc(1);
        tmp[0] = 0;
        return tmp;
    }
    assert(stop > start);
    size_t len = 1+stop-start; // It gets shorter when unescaping. Worst case no escape was found.
    char *buf = (char*)malloc(len);

    const char *i = start;
    char *o = buf;
    size_t real = 0;

    for (; i < stop; ++i, real++)
    {
        char c = *i;
        if (c == '\\') {
            i++;
            if (i >= stop) break;
            c = *i;
            if (c == '"') *o++ = '"';
            else if (c == 'n') *o++ = '\n';
            else if (c == 'a') *o++ = '\a';
            else if (c == 'b') *o++ = '\b';
            else if (c == 't') *o++ = '\t';
            else if (c == 'v') *o++ = '\v';
            else if (c == 'f') *o++ = '\f';
            else if (c == 'r') *o++ = '\r';
            // Ignore or what?
        }
        else
        {
            *o++ = *i;
        }
    }
    real++;
    *o = 0;
    buf = (char*)realloc(buf, real);
    return buf;
}

#endif // TEXT_MODULE