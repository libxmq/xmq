
#ifndef BUILDING_XMQ

#include"always.h"
#include"text.h"
#include<stdio.h>
#include<stdlib.h>
#include<assert.h>
#include<string.h>

#endif

#ifdef TEXT_MODULE

const char *has_leading_space_nl(const char *start, const char *stop, size_t *only_newlines)
{
    const char *i = start;
    bool found_nl = false;
    size_t only_nls = 0;

    if (only_newlines != NULL) *only_newlines = 0;

    // Look for leading newlines.
    while (i < stop && *i == '\n')
    {
        i++;
        only_nls++;
    }
    const char *middle = NULL;

    // Yep, we found some leading pure newlines.
    if (only_nls > 0)
    {
        found_nl = true;
        middle = i;
    }

    // Scan other leading whitespace, perhaps none.
    while (i < stop)
    {
        if (*i == '\n') found_nl = true;
        if (!is_xml_whitespace(*i)) break;
        i++;
    }

    // No newline found before content, so leading spaces/tabs will not be trimmed.
    if (!found_nl) return 0;

    if (middle == i)
    {
        // We have for example "\ncontent" this we can represent in xmq with a visible empty line, eg:
        // '
        //
        // content'
        if (only_newlines != NULL) *only_newlines = only_nls;
    }
    return i;
}

const char *has_ending_nl_space(const char *start, const char *stop, size_t *only_newlines)
{
    const char *i = stop;
    bool found_nl = false;
    size_t only_nls = 0;

    if (only_newlines != NULL) *only_newlines = 0;

    // Look for ending newlines.
    i--;
    while (i >= start && *i == '\n')
    {
        i--;
        only_nls++;
        found_nl = true;
    }
    const char *middle = i;

    while (i >= start)
    {
        if (*i == '\n') found_nl = true;
        if (!is_xml_whitespace(*i)) break;
        i--;
    }
    // No newline found after content, so ending spaces/tabs will not be trimmed.
    if (!found_nl) return 0;

    if (middle == i)
    {
        // We have for example "content\n" this we can represent in xmq with a visible empty line, eg:
        // 'content
        //
        // '
        if (only_newlines != NULL) *only_newlines = only_nls;
    }

    i++;
    return i;
}

bool has_leading_ending_quote(const char *start, const char *stop)
{
    return start < stop && ( *start == '\'' || *(stop-1) == '\'');
}

bool has_newlines(const char *start, const char *stop)
{
    for (const char *i = start; i < stop; ++i)
    {
        if (*i == '\n') return true;
    }
    return false;
}

bool has_must_escape_chars(const char *start, const char *stop)
{
    for (const char *i = start; i < stop; ++i)
    {
        if (*i == '\n') return true;
    }
    return false;
}

bool has_all_quotes(const char *start, const char *stop)
{
    for (const char *i = start; i < stop; ++i)
    {
        if (*i != '\'') return false;
    }
    return true;
}

bool has_all_whitespace(const char *start, const char *stop, bool *all_space, bool *only_newlines)
{
    *all_space = true;
    *only_newlines = true;
    for (const char *i = start; i < stop; ++i)
    {
        if (!is_xml_whitespace(*i))
        {
            *all_space = false;
            *only_newlines = false;
            return false;
        }
        if (*i != ' ' && *all_space == true)
        {
            *all_space = false;
        }
        if (*i != '\n' && *only_newlines == true)
        {
            *only_newlines = false;
        }
    }
    return true;
}

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
   encode_utf8: Convert an integer unicode code point into utf8 bytes.
   @uc: The unicode code point to encode as utf8
   @out_char: Store the unicode code point here.
   @out_len: How many bytes the utf8 char used.

   Return true if valid utf8 char.
*/
size_t encode_utf8(int uc, UTF8Char *utf8)
{
    utf8->bytes[0] = 0;
    utf8->bytes[1] = 0;
    utf8->bytes[2] = 0;
    utf8->bytes[3] = 0;

    if (uc <= 0x7f)
    {
        utf8->bytes[0] = uc;
        return 1;
    }
    else if (uc <= 0x7ff)
    {
        utf8->bytes[0] = (0xc0 | ((uc >> 6) & 0x1f));
        utf8->bytes[1] = (0x80 | (uc & 0x3f));
        return 2;
    }
    else if (uc <= 0xffff)
    {
        utf8->bytes[0] = (0xe0 | ((uc >> 12) & 0x0f));
        utf8->bytes[1] = (0x80 | ((uc >> 6) & 0x3f));
        utf8->bytes[2] = (0x80 | (uc & 0x3f));
        return 3;
    }
    assert (uc <= 0x10ffff);
    utf8->bytes[0] = (0xf0 | ((uc >> 18) & 0x07));
    utf8->bytes[1] = (0x80 | ((uc >> 12) & 0x3f));
    utf8->bytes[2] = (0x80 | ((uc >> 6) & 0x3f));
    utf8->bytes[3] = (0x80 | (uc & 0x3f));
    return 4;
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

bool is_xmq_element_name(const char *start, const char *stop, const char **colon)
{
    const char *i = start;
    *colon = NULL;
    if (!is_xmq_element_start(*i)) return false;
    i++;

    for (; i < stop; ++i)
    {
        char c = *i;
        if (!is_xmq_text_name(c)) return false;
        if (c == ':') *colon = i;
    }

    return true;
}

bool is_xmq_token_whitespace(char c)
{
    if (c == ' ' || c == '\n' || c == '\r')
    {
        return true;
    }
    return false;
}

bool is_xml_whitespace(char c)
{
    if (c == ' ' || c == '\n' || c == '\t' || c == '\r')
    {
        return true;
    }
    return false;
}

bool is_all_xml_whitespace(const char *s)
{
    if (!s) return false;

    for (const char *i = s; *i; ++i)
    {
        if (!is_xml_whitespace(*i)) return false;
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
char *xmq_quote_as_c(const char *start, const char *stop, bool add_quotes)
{
    if (!stop) stop = start+strlen(start);
    if (stop == start)
    {
        if (add_quotes)
        {
            char *tmp = (char*)malloc(3);
            tmp[0] = '"';
            tmp[1] = '"';
            tmp[2] = 0;
            return tmp;
        }
        else
        {
            char *tmp = (char*)malloc(1);
            tmp[0] = 0;
            return tmp;
        }
    }
    assert(stop > start);
    size_t len = 1+(stop-start)*4+2; // Worst case expansion of all chars. +2 for qutes.
    char *buf = (char*)malloc(len);

    const char *i = start;
    char *o = buf;
    size_t real = 0;

    if (add_quotes)
    {
        real++;
        *o++ = '"';
    }

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
        if (c >= ' ' && c <= 126 && c != '"' && c != '\\') { *o++ = *i; real++;}
        else if (c == '\\') { *o++ = '\\'; *o++ = '\\'; real+=2; }
        else if (c == '"') { *o++ = '\\'; *o++ = '"'; real+=2; }
        else if (c == '\a') { *o++ = '\\'; *o++ = 'a'; real+=2; }
        else if (c == '\b') { *o++ = '\\'; *o++ = 'b'; real+=2; }
        else if (c == '\t') { *o++ = '\\'; *o++ = 't'; real+=2; }
        else if (c == '\n') { *o++ = '\\'; *o++ = 'n'; real+=2; }
        else if (c == '\v') { *o++ = '\\'; *o++ = 'v'; real+=2; }
        else if (c == '\f') { *o++ = '\\'; *o++ = 'f'; real+=2; }
        else if (c == '\r') { *o++ = '\\'; *o++ = 'r'; real+=2; }
        else { *o++ = '\\'; *o++ = 'x'; *o++ = to_hex((c>>4)&0xf); *o++ = to_hex(c&0xf); real+=4; }
        if (c == 0) break;
    }
    if (add_quotes) {
        real++;
        *o++ = '"';
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
char *xmq_unquote_as_c(const char *start, const char *stop, bool remove_quotes)
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

    if (remove_quotes)
    {
        for (; i < stop && is_xml_whitespace(*i); ++i);
        if (*i != '"') return strdup("[Not a valid C escaped string]");
        i++;
    }

    for (; i < stop && (!remove_quotes || *i != '"'); ++i, real++)
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
    if (remove_quotes)
    {
        if (*i != '"') return strdup("[Not a valid C escaped string]");
    }
    real++;
    *o = 0;
    buf = (char*)realloc(buf, real);
    return buf;
}

char *potentially_add_leading_ending_space(const char *start, const char *stop)
{
    char *content = NULL;
    int prefix = *start == '\'' ? 1 : 0;
    int postfix = *(stop-1) == '\'' ? 1 : 0;
    if (prefix || postfix)
    {
        size_t len = stop-start;
        len += prefix;
        len += postfix;
        content = (char*)malloc(len+1);
        if (prefix)
        {
            content[0] = ' ';
        }
        if (postfix)
        {
            content[len-1] = ' ';
        }
        memcpy(content+prefix, start, stop-start);
        content[len] = 0;
    }
    else
    {
        content = strndup(start, stop-start);
    }
    return content;
}

bool find_line_col(const char *start, const char *stop, size_t at, int *out_line, int *out_col)
{
    const char *i = start;
    int line = 1;
    int col = 1;
    while (i < stop && at > 0)
    {
        int oc;
        size_t ol;
        bool ok = decode_utf8(i, stop, &oc, &ol);
        assert(ok);
        i += ol;
        col++;
        if (oc == 10)
        {
            line++;
            col=0;
        }
        at--;
    }

    *out_line = line;
    *out_col = col;
    return true;
}

int unicode_Zs_[] = { 0x0020, 0x00A0, 0x1680, 0x2000, 0x2001, 0x2002, 0x2003, 0x2004, 0x2005, 0x2006, 0x2007, 0x2008, 0x2009, 0x200A, 0x202F, 0x205F, 0x3000 };

size_t unicode_Zs_len_ = sizeof(unicode_Zs_)/sizeof(unicode_Zs_[0]);

int unicode_Ll_[] = {0x0061,0x0062,0x0063,0x0064,0x0065,0x0066,0x0067,0x0068,0x0069,0x006A,0x006B,0x006C,0x006D,0x006E,0x006F,0x0070,0x0071,0x0072,0x0073,0x0074,0x0075,0x0076,0x0077,0x0078,0x0079,0x007A,0x00B5,0x00DF,0x00E0,0x00E1,0x00E2,0x00E3,0x00E4,0x00E5,0x00E6,0x00E7,0x00E8,0x00E9,0x00EA,0x00EB,0x00EC,0x00ED,0x00EE,0x00EF,0x00F0,0x00F1,0x00F2,0x00F3,0x00F4,0x00F5,0x00F6,0x00F8,0x00F9,0x00FA,0x00FB,0x00FC,0x00FD,0x00FE,0x00FF,0x0101,0x0103,0x0105,0x0107,0x0109,0x010B,0x010D,0x010F,0x0111,0x0113,0x0115,0x0117,0x0119,0x011B,0x011D,0x011F,0x0121,0x0123,0x0125,0x0127,0x0129,0x012B,0x012D,0x012F,0x0131,0x0133,0x0135,0x0137,0x0138,0x013A,0x013C,0x013E,0x0140,0x0142,0x0144,0x0146,0x0148,0x0149,0x014B,0x014D,0x014F,0x0151,0x0153,0x0155,0x0157,0x0159,0x015B,0x015D,0x015F,0x0161,0x0163,0x0165,0x0167,0x0169,0x016B,0x016D,0x016F,0x0171,0x0173,0x0175,0x0177,0x017A,0x017C,0x017E,0x017F,0x0180,0x0183,0x0185,0x0188,0x018C,0x018D,0x0192,0x0195,0x0199,0x019A,0x019B,0x019E,0x01A1,0x01A3,0x01A5,0x01A8,0x01AA,0x01AB,0x01AD,0x01B0,0x01B4,0x01B6,0x01B9,0x01BA,0x01BD,0x01BE,0x01BF,0x01C6,0x01C9,0x01CC,0x01CE,0x01D0,0x01D2,0x01D4,0x01D6,0x01D8,0x01DA,0x01DC,0x01DD,0x01DF,0x01E1,0x01E3,0x01E5,0x01E7,0x01E9,0x01EB,0x01ED,0x01EF,0x01F0,0x01F3,0x01F5,0x01F9,0x01FB,0x01FD,0x01FF};

size_t unicode_Ll_len_ = sizeof(unicode_Ll_)/sizeof(unicode_Ll_[0]);

int unicode_Lu_[] = {0x0041,0x0042,0x0043,0x0044,0x0045,0x0046,0x0047,0x0048,0x0049,0x004A,0x004B,0x004C,0x004D,0x004E,0x004F,0x0050,0x0051,0x0052,0x0053,0x0054,0x0055,0x0056,0x0057,0x0058,0x0059,0x005A,0x00C0,0x00C1,0x00C2,0x00C3,0x00C4,0x00C5,0x00C6,0x00C7,0x00C8,0x00C9,0x00CA,0x00CB,0x00CC,0x00CD,0x00CE,0x00CF,0x00D0,0x00D1,0x00D2,0x00D3,0x00D4,0x00D5,0x00D6,0x00D8,0x00D9,0x00DA,0x00DB,0x00DC,0x00DD,0x00DE,0x0100,0x0102,0x0104,0x0106,0x0108,0x010A,0x010C,0x010E,0x0110,0x0112,0x0114,0x0116,0x0118,0x011A,0x011C,0x011E,0x0120,0x0122,0x0124,0x0126,0x0128,0x012A,0x012C,0x012E,0x0130,0x0132,0x0134,0x0136,0x0139,0x013B,0x013D,0x013F,0x0141,0x0143,0x0145,0x0147,0x014A,0x014C,0x014E,0x0150,0x0152,0x0154,0x0156,0x0158,0x015A,0x015C,0x015E,0x0160,0x0162,0x0164,0x0166,0x0168,0x016A,0x016C,0x016E,0x0170,0x0172,0x0174,0x0176,0x0178,0x0179,0x017B,0x017D,0x0181,0x0182,0x0184,0x0186,0x0187,0x0189,0x018A,0x018B,0x018E,0x018F,0x0190,0x0191,0x0193,0x0194,0x0196,0x0197,0x0198,0x019C,0x019D,0x019F,0x01A0,0x01A2,0x01A4,0x01A6,0x01A7,0x01A9,0x01AC,0x01AE,0x01AF,0x01B1,0x01B2,0x01B3,0x01B5,0x01B7,0x01B8,0x01BC,0x01C4,0x01C7,0x01CA,0x01CD,0x01CF,0x01D1,0x01D3,0x01D5,0x01D7,0x01D9,0x01DB,0x01DE,0x01E0,0x01E2,0x01E4,0x01E6,0x01E8,0x01EA,0x01EC,0x01EE,0x01F1,0x01F4,0x01F6,0x01F7,0x01F8,0x01FA,0x01FC,0x01FE,0};

size_t unicode_Lu_len_ = sizeof(unicode_Lu_)/sizeof(unicode_Lu_[0]);

unsigned short int unicode_none_[] = {0};

bool unicode_category(const char *name, int **out, size_t *out_len)
{
    if (!strcmp(name, "Zs"))
    {
        *out = unicode_Zs_;
        *out_len = unicode_Zs_len_;
        return true;
    }
    if (!strcmp(name, "Ll"))
    {
        *out = unicode_Ll_;
        *out_len = unicode_Ll_len_;
        return true;
    }
    if (!strcmp(name, "Lu")) {
        *out = unicode_Lu_;
        *out_len = unicode_Lu_len_;
        return true;
    }

    return false;
}

bool category_find(int code, int *cat, size_t cat_len)
{
    if (cat_len == 0) return false;
    if (cat_len == 1) return *cat == code;

    // Points to first item.
    int *start = cat;
    // Points to last item.
    int *stop = cat+cat_len-1;

    for (;;)
    {
        if (start == stop) return *start == code;
        if (start+1 == stop) return *start == code || *stop == code;

        // Take the middle.
        int *i = start+((stop-start)>>1);
        if (*i == code) return true;
        if (*i < code)
        {
            // Move to upper half.
            start = i+1;
        }
        else
        {
            // Move to lower half.
            stop = i-1;
        }
    }
}

#endif // TEXT_MODULE
