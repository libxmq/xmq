/* libxmq - Copyright (C) 2026 Fredrik Öhrström (spdx: MIT)

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

*/

/* Portions of this file are ported from libxml2 (uri.c, version 2.16.0 as found
   in the libxmq fork in 3rdparty/libxml2).
   libxml2 is covered by the following notice (from libxml2/Copyright):

 Copyright (C) 1998-2012 Daniel Veillard.  All Rights Reserved.
 Copyright (C) The Libxml2 Contributors.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is fur-
nished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FIT-
NESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

#ifndef BUILDING_DIST_XMQ

#include"xml_uri.h"

#include<limits.h>
#include<stdio.h>
#include<stdint.h>
#include<stdlib.h>
#include<string.h>

#endif

#ifdef XML_URI_MODULE

// The macros of libxml2 uri.c, not shared with the other parts (dist/xmq.c is one file).
#undef MAX_URI_LENGTH
#undef PORT_EMPTY
#undef PORT_EMPTY_SERVER
#undef IS_ALPHA
#undef IS_LOWALPHA
#undef IS_UPALPHA
#undef IS_DIGIT
#undef IS_ALPHANUM
#undef IS_MARK
#undef IS_UNWISE
#undef IS_RESERVED
#undef IS_UNRESERVED
#undef NEXT
#undef STRNDUP
#undef ISA_DIGIT
#undef ISA_ALPHA
#undef ISA_HEXDIG
#undef ISA_SUB_DELIM
#undef ISA_GEN_DELIM
#undef ISA_RESERVED
#undef ISA_STRICTLY_UNRESERVED
#undef ISA_PCT_ENCODED
#undef ISA_PCHAR
#undef ISA_UCSCHAR
#undef ISA_UNRESERVED
#undef XML_URI_ALLOW_UNWISE
#undef XML_URI_NO_UNESCAPE
#undef XML_URI_ALLOW_UCSCHAR

#define MAX_URI_LENGTH 1024 * 1024

#define PORT_EMPTY           0
#define PORT_EMPTY_SERVER   -1

static void xml_uri_clean(XmlURI *uri);

static char *xml_uri_strdup(const char *s)
{
    if (!s) return NULL;
    return strdup(s);
}

static char *xml_uri_strndup(const char *s, int len)
{
    if (!s || len < 0) return NULL;
    char *r = (char*)malloc((size_t)len + 1);
    if (!r) return NULL;
    memcpy(r, s, (size_t)len);
    r[len] = 0;
    return r;
}

static int xml_uri_strlen(const char *s)
{
    if (!s) return 0;
    return (int)strlen(s);
}

/** Like xmlStrcat: append add to s (reallocated), s may be NULL. */
static char *xml_uri_strcat(char *s, const char *add)
{
    if (!add) return s;
    if (!s) return xml_uri_strdup(add);
    size_t a = strlen(s), b = strlen(add);
    char *r = (char*)realloc(s, a + b + 1);
    if (!r)
    {
        free(s);
        return NULL;
    }
    memcpy(r + a, add, b + 1);
    return r;
}

static int xml_uri_streq(const char *a, const char *b)
{
    if (a == b) return 1;
    if (!a || !b) return 0;
    return !strcmp(a, b);
}

/** Port of xmlGrowCapacity (private/memory.h). */
static int xml_uri_grow_capacity(int capacity, size_t elemSize, int min, int max)
{
    if (capacity <= 0) return min;
    if ((capacity >= max) || ((size_t)capacity > SIZE_MAX / 2 / elemSize)) return -1;
    int extra = (capacity + 1) / 2;
    if (capacity > max - extra) return max;
    return capacity + extra;
}


/*
 * Old rule from 2396 used in legacy handling code
 * alpha    = lowalpha | upalpha
 */
#define IS_ALPHA(x) (IS_LOWALPHA(x) || IS_UPALPHA(x))


/*
 * lowalpha = "a" | "b" | "c" | "d" | "e" | "f" | "g" | "h" | "i" | "j" |
 *            "k" | "l" | "m" | "n" | "o" | "p" | "q" | "r" | "s" | "t" |
 *            "u" | "v" | "w" | "x" | "y" | "z"
 */
#define IS_LOWALPHA(x) (((x) >= 'a') && ((x) <= 'z'))

/*
 * upalpha = "A" | "B" | "C" | "D" | "E" | "F" | "G" | "H" | "I" | "J" |
 *           "K" | "L" | "M" | "N" | "O" | "P" | "Q" | "R" | "S" | "T" |
 *           "U" | "V" | "W" | "X" | "Y" | "Z"
 */
#define IS_UPALPHA(x) (((x) >= 'A') && ((x) <= 'Z'))

#ifdef IS_DIGIT
#undef IS_DIGIT
#endif
/*
 * digit = "0" | "1" | "2" | "3" | "4" | "5" | "6" | "7" | "8" | "9"
 */
#define IS_DIGIT(x) (((x) >= '0') && ((x) <= '9'))

/*
 * alphanum = alpha | digit
 */
#define IS_ALPHANUM(x) (IS_ALPHA(x) || IS_DIGIT(x))

/*
 * mark = "-" | "_" | "." | "!" | "~" | "*" | "'" | "(" | ")"
 */

#define IS_MARK(x) (((x) == '-') || ((x) == '_') || ((x) == '.') ||     \
    ((x) == '!') || ((x) == '~') || ((x) == '*') || ((x) == '\'') ||    \
    ((x) == '(') || ((x) == ')'))

/*
 * unwise = "{" | "}" | "|" | "\" | "^" | "`"
 */
#define IS_UNWISE(p)                                                    \
      (((*(p) == '{')) || ((*(p) == '}')) || ((*(p) == '|')) ||         \
       ((*(p) == '\\')) || ((*(p) == '^')) || ((*(p) == '[')) ||        \
       ((*(p) == ']')) || ((*(p) == '`')))

/*
 * reserved = ";" | "/" | "?" | ":" | "@" | "&" | "=" | "+" | "$" | "," |
 *            "[" | "]"
 */
#define IS_RESERVED(x) (((x) == ';') || ((x) == '/') || ((x) == '?') || \
        ((x) == ':') || ((x) == '@') || ((x) == '&') || ((x) == '=') || \
        ((x) == '+') || ((x) == '$') || ((x) == ',') || ((x) == '[') || \
        ((x) == ']'))

/*
 * unreserved = alphanum | mark
 */
#define IS_UNRESERVED(x) (IS_ALPHANUM(x) || IS_MARK(x))

/*
 * Skip to next pointer char, handle escaped sequences
 */
#define NEXT(p) ((*p == '%')? p += 3 : p++)

/*
 * Productions from the spec.
 *
 *    authority     = server | reg_name
 *    reg_name      = 1*( unreserved | escaped | "$" | "," |
 *                        ";" | ":" | "@" | "&" | "=" | "+" )
 *
 * path          = [ abs_path | opaque_part ]
 */
#define STRNDUP(s, n) (char*)xml_uri_strndup((const char *)(s), (n))

/************************************************************************
 *									*
 *                         RFC 3986 parser				*
 *									*
 ************************************************************************/

#define ISA_DIGIT(p) ((*(p) >= '0') && (*(p) <= '9'))
#define ISA_ALPHA(p) (((*(p) >= 'a') && (*(p) <= 'z')) ||		\
                      ((*(p) >= 'A') && (*(p) <= 'Z')))
#define ISA_HEXDIG(p)							\
       (ISA_DIGIT(p) || ((*(p) >= 'a') && (*(p) <= 'f')) ||		\
        ((*(p) >= 'A') && (*(p) <= 'F')))

/*
 *    sub-delims    = "!" / "$" / "&" / "'" / "(" / ")"
 *                     / "*" / "+" / "," / ";" / "="
 */
#define ISA_SUB_DELIM(p)						\
      (((*(p) == '!')) || ((*(p) == '$')) || ((*(p) == '&')) ||		\
       ((*(p) == '(')) || ((*(p) == ')')) || ((*(p) == '*')) ||		\
       ((*(p) == '+')) || ((*(p) == ',')) || ((*(p) == ';')) ||		\
       ((*(p) == '=')) || ((*(p) == '\'')))

/*
 *    gen-delims    = ":" / "/" / "?" / "\#" / "[" / "]" / "@"
 */
#define ISA_GEN_DELIM(p)						\
      (((*(p) == ':')) || ((*(p) == '/')) || ((*(p) == '?')) ||         \
       ((*(p) == '#')) || ((*(p) == '[')) || ((*(p) == ']')) ||         \
       ((*(p) == '@')))

/*
 *    reserved      = gen-delims / sub-delims
 */
#define ISA_RESERVED(p) (ISA_GEN_DELIM(p) || (ISA_SUB_DELIM(p)))

/*
 *    unreserved    = ALPHA / DIGIT / "-" / "." / "_" / "~"
 */
#define ISA_STRICTLY_UNRESERVED(p)					\
      ((ISA_ALPHA(p)) || (ISA_DIGIT(p)) || ((*(p) == '-')) ||		\
       ((*(p) == '.')) || ((*(p) == '_')) || ((*(p) == '~')))

/*
 *    pct-encoded   = "%" HEXDIG HEXDIG
 */
#define ISA_PCT_ENCODED(p)						\
     ((*(p) == '%') && (ISA_HEXDIG(p + 1)) && (ISA_HEXDIG(p + 2)))

/*
 *    pchar         = unreserved / pct-encoded / sub-delims / ":" / "@"
 */
#define ISA_PCHAR(u, p)							\
     (ISA_UNRESERVED(u, p) || ISA_PCT_ENCODED(p) || ISA_SUB_DELIM(p) ||	\
      ((*(p) == ':')) || ((*(p) == '@')))

/*
 * From https://www.w3.org/TR/leiri/
 *
 * " " / "<" / ">" / '"' / "{" / "}" / "|"
 * / "\" / "^" / "`" / %x0-1F / %x7F-D7FF
 * / %xE000-FFFD / %x10000-10FFFF
 */
#define ISA_UCSCHAR(p) \
    ((*(p) <= 0x20) || (*(p) >= 0x7F) || (*(p) == '<') || (*(p) == '>') || \
     (*(p) == '"')  || (*(p) == '{')  || (*(p) == '}') || (*(p) == '|') || \
     (*(p) == '\\') || (*(p) == '^')  || (*(p) == '`'))

#define ISA_UNRESERVED(u, p) (xml_uri_is_unreserved(u, p))

#define XML_URI_ALLOW_UNWISE    1
#define XML_URI_NO_UNESCAPE     2
#define XML_URI_ALLOW_UCSCHAR   4

static int
xml_uri_is_unreserved(XmlURI *uri, const char *cur) {
    if (uri == NULL)
        return(0);

    if (ISA_STRICTLY_UNRESERVED(cur))
        return(1);

    if (uri->cleanup & XML_URI_ALLOW_UNWISE) {
        if (IS_UNWISE(cur))
            return(1);
    } else if (uri->cleanup & XML_URI_ALLOW_UCSCHAR) {
        if (ISA_UCSCHAR(cur))
            return(1);
    }

    return(0);
}

/**
 * Parse an URI scheme
 *
 * ALPHA *( ALPHA / DIGIT / "+" / "-" / "." )
 *
 * @param uri  pointer to an URI structure
 * @param str  pointer to the string to analyze
 * @returns 0 or the error code
 */
static int
xml_uri_parse_scheme(XmlURI *uri, const char **str) {
    const char *cur;

    cur = *str;
    if (!ISA_ALPHA(cur))
	return(1);
    cur++;

#if defined(_WIN32)
    /*
     * Don't treat Windows drive letters as scheme.
     */
    if (*cur == ':')
        return(1);
#endif

    while (ISA_ALPHA(cur) || ISA_DIGIT(cur) ||
           (*cur == '+') || (*cur == '-') || (*cur == '.')) cur++;
    if (uri != NULL) {
	if (uri->scheme != NULL) free(uri->scheme);
	uri->scheme = STRNDUP(*str, cur - *str);
        if (uri->scheme == NULL)
            return(-1);
    }
    *str = cur;
    return(0);
}

/**
 * Parse the query part of an URI
 *
 * fragment      = *( pchar / "/" / "?" )
 * NOTE: the strict syntax as defined by 3986 does not allow '[' and ']'
 *       in the fragment identifier but this is used very broadly for
 *       xpointer scheme selection, so we are allowing it here to not break
 *       for example all the DocBook processing chains.
 *
 * @param uri  pointer to an URI structure
 * @param str  pointer to the string to analyze
 * @returns 0 or the error code
 */
static int
xml_uri_parse_fragment(XmlURI *uri, const char **str)
{
    const char *cur;

    cur = *str;

    while ((ISA_PCHAR(uri, cur)) || (*cur == '/') || (*cur == '?') ||
           (*cur == '[') || (*cur == ']'))
        NEXT(cur);
    if (uri != NULL) {
        if (uri->fragment != NULL)
            free(uri->fragment);
	if (uri->cleanup & XML_URI_NO_UNESCAPE)
	    uri->fragment = STRNDUP(*str, cur - *str);
	else
	    uri->fragment = xml_uri_unescape_string(*str, cur - *str, NULL);
        if (uri->fragment == NULL)
            return (-1);
    }
    *str = cur;
    return (0);
}

/**
 * Parse the query part of an URI
 *
 * query = *uric
 *
 * @param uri  pointer to an URI structure
 * @param str  pointer to the string to analyze
 * @returns 0 or the error code
 */
static int
xml_uri_parse_query(XmlURI *uri, const char **str)
{
    const char *cur;

    cur = *str;

    while ((ISA_PCHAR(uri, cur)) || (*cur == '/') || (*cur == '?'))
        NEXT(cur);
    if (uri != NULL) {
        if (uri->query != NULL)
            free(uri->query);
	if (uri->cleanup & XML_URI_NO_UNESCAPE)
	    uri->query = STRNDUP(*str, cur - *str);
	else
	    uri->query = xml_uri_unescape_string(*str, cur - *str, NULL);
        if (uri->query == NULL)
            return (-1);

	/* Save the raw bytes of the query as well.
	 * See: http://mail.gnome.org/archives/xml/2007-April/thread.html#00114
	 */
	if (uri->query_raw != NULL)
	    free (uri->query_raw);
	uri->query_raw = STRNDUP (*str, cur - *str);
        if (uri->query_raw == NULL)
            return (-1);
    }
    *str = cur;
    return (0);
}

/**
 * Parse a port part and fills in the appropriate fields
 * of the `uri` structure
 *
 * port          = *DIGIT
 *
 * @param uri  pointer to an URI structure
 * @param str  the string to analyze
 * @returns 0 or the error code
 */
static int
xml_uri_parse_port(XmlURI *uri, const char **str)
{
    const char *cur = *str;
    int port = 0;

    if (ISA_DIGIT(cur)) {
	while (ISA_DIGIT(cur)) {
            int digit = *cur - '0';

            if (port > INT_MAX / 10)
                return(1);
            port *= 10;
            if (port > INT_MAX - digit)
                return(1);
	    port += digit;

	    cur++;
	}
	if (uri != NULL)
	    uri->port = port;
	*str = cur;
	return(0);
    }
    return(1);
}

/**
 * Parse an user information part and fills in the appropriate fields
 * of the `uri` structure
 *
 * userinfo      = *( unreserved / pct-encoded / sub-delims / ":" )
 *
 * @param uri  pointer to an URI structure
 * @param str  the string to analyze
 * @returns 0 or the error code
 */
static int
xml_uri_parse_userinfo(XmlURI *uri, const char **str)
{
    const char *cur;

    cur = *str;
    while (ISA_UNRESERVED(uri, cur) || ISA_PCT_ENCODED(cur) ||
           ISA_SUB_DELIM(cur) || (*cur == ':'))
	NEXT(cur);
    if (*cur == '@') {
	if (uri != NULL) {
	    if (uri->user != NULL) free(uri->user);
	    if (uri->cleanup & XML_URI_NO_UNESCAPE)
		uri->user = STRNDUP(*str, cur - *str);
	    else
		uri->user = xml_uri_unescape_string(*str, cur - *str, NULL);
            if (uri->user == NULL)
                return(-1);
	}
	*str = cur;
	return(0);
    }
    return(1);
}

/**
 *    dec-octet     = DIGIT                 ; 0-9
 *                  / %x31-39 DIGIT         ; 10-99
 *                  / "1" 2DIGIT            ; 100-199
 *                  / "2" %x30-34 DIGIT     ; 200-249
 *                  / "25" %x30-35          ; 250-255
 *
 * Skip a dec-octet.
 *
 * @param str  the string to analyze
 * @returns 0 if found and skipped, 1 otherwise
 */
static int
xml_uri_parse_dec_octet(const char **str) {
    const char *cur = *str;

    if (!(ISA_DIGIT(cur)))
        return(1);
    if (!ISA_DIGIT(cur+1))
	cur++;
    else if ((*cur != '0') && (ISA_DIGIT(cur + 1)) && (!ISA_DIGIT(cur+2)))
	cur += 2;
    else if ((*cur == '1') && (ISA_DIGIT(cur + 1)) && (ISA_DIGIT(cur + 2)))
	cur += 3;
    else if ((*cur == '2') && (*(cur + 1) >= '0') &&
	     (*(cur + 1) <= '4') && (ISA_DIGIT(cur + 2)))
	cur += 3;
    else if ((*cur == '2') && (*(cur + 1) == '5') &&
	     (*(cur + 2) >= '0') && (*(cur + 1) <= '5'))
	cur += 3;
    else
        return(1);
    *str = cur;
    return(0);
}
/**
 * Parse an host part and fills in the appropriate fields
 * of the `uri` structure
 *
 * host          = IP-literal / IPv4address / reg-name
 * IP-literal    = "[" ( IPv6address / IPvFuture  ) "]"
 * IPv4address   = dec-octet "." dec-octet "." dec-octet "." dec-octet
 * reg-name      = *( unreserved / pct-encoded / sub-delims )
 *
 * @param uri  pointer to an URI structure
 * @param str  the string to analyze
 * @returns 0 or the error code
 */
static int
xml_uri_parse_host(XmlURI *uri, const char **str)
{
    const char *cur = *str;
    const char *host;

    host = cur;
    /*
     * IPv6 and future addressing scheme are enclosed between brackets
     */
    if (*cur == '[') {
        cur++;
	while ((*cur != ']') && (*cur != 0))
	    cur++;
	if (*cur != ']')
	    return(1);
	cur++;
	goto found;
    }
    /*
     * try to parse an IPv4
     */
    if (ISA_DIGIT(cur)) {
        if (xml_uri_parse_dec_octet(&cur) != 0)
	    goto not_ipv4;
	if (*cur != '.')
	    goto not_ipv4;
	cur++;
        if (xml_uri_parse_dec_octet(&cur) != 0)
	    goto not_ipv4;
	if (*cur != '.')
	    goto not_ipv4;
        if (xml_uri_parse_dec_octet(&cur) != 0)
	    goto not_ipv4;
	if (*cur != '.')
	    goto not_ipv4;
        if (xml_uri_parse_dec_octet(&cur) != 0)
	    goto not_ipv4;
	goto found;
not_ipv4:
        cur = *str;
    }
    /*
     * then this should be a hostname which can be empty
     */
    while (ISA_UNRESERVED(uri, cur) ||
           ISA_PCT_ENCODED(cur) || ISA_SUB_DELIM(cur))
        NEXT(cur);
found:
    if (uri != NULL) {
	if (uri->authority != NULL) free(uri->authority);
	uri->authority = NULL;
	if (uri->server != NULL) free(uri->server);
	if (cur != host) {
	    if (uri->cleanup & XML_URI_NO_UNESCAPE)
		uri->server = STRNDUP(host, cur - host);
	    else
		uri->server = xml_uri_unescape_string(host, cur - host, NULL);
            if (uri->server == NULL)
                return(-1);
	} else
	    uri->server = NULL;
    }
    *str = cur;
    return(0);
}

/**
 * Parse an authority part and fills in the appropriate fields
 * of the `uri` structure
 *
 * authority     = [ userinfo "@" ] host [ ":" port ]
 *
 * @param uri  pointer to an URI structure
 * @param str  the string to analyze
 * @returns 0 or the error code
 */
static int
xml_uri_parse_authority(XmlURI *uri, const char **str)
{
    const char *cur;
    int ret;

    cur = *str;
    /*
     * try to parse an userinfo and check for the trailing @
     */
    ret = xml_uri_parse_userinfo(uri, &cur);
    if (ret < 0)
        return(ret);
    if ((ret != 0) || (*cur != '@'))
        cur = *str;
    else
        cur++;
    ret = xml_uri_parse_host(uri, &cur);
    if (ret != 0) return(ret);
    if (*cur == ':') {
        cur++;
        ret = xml_uri_parse_port(uri, &cur);
	if (ret != 0) return(ret);
    }
    *str = cur;
    return(0);
}

/**
 * Parse a segment and fills in the appropriate fields
 * of the `uri` structure
 *
 * segment       = *pchar
 * segment-nz    = 1*pchar
 * segment-nz-nc = 1*( unreserved / pct-encoded / sub-delims / "@" )
 *               ; non-zero-length segment without any colon ":"
 *
 * @param uri  the URI
 * @param str  the string to analyze
 * @param forbid  an optional forbidden character
 * @param empty  allow an empty segment
 * @returns 0 or the error code
 */
static int
xml_uri_parse_segment(XmlURI *uri, const char **str, char forbid, int empty)
{
    const char *cur;

    cur = *str;
    if (!ISA_PCHAR(uri, cur) || (*cur == forbid)) {
        if (empty)
	    return(0);
	return(1);
    }
    NEXT(cur);

#if defined(_WIN32)
    /*
     * Allow Windows drive letters.
     */
    if ((forbid == ':') && (*cur == forbid))
        NEXT(cur);
#endif

    while (ISA_PCHAR(uri, cur) && (*cur != forbid))
        NEXT(cur);
    *str = cur;
    return (0);
}

/**
 * Parse an path absolute or empty and fills in the appropriate fields
 * of the `uri` structure
 *
 * path-abempty  = *( "/" segment )
 *
 * @param uri  pointer to an URI structure
 * @param str  the string to analyze
 * @returns 0 or the error code
 */
static int
xml_uri_parse_path_ab_empty(XmlURI *uri, const char **str)
{
    const char *cur;
    int ret;

    cur = *str;

    while (*cur == '/') {
        cur++;
	ret = xml_uri_parse_segment(uri, &cur, 0, 1);
	if (ret != 0) return(ret);
    }
    if (uri != NULL) {
	if (uri->path != NULL) free(uri->path);
        if (*str != cur) {
            if (uri->cleanup & XML_URI_NO_UNESCAPE)
                uri->path = STRNDUP(*str, cur - *str);
            else
                uri->path = xml_uri_unescape_string(*str, cur - *str, NULL);
            if (uri->path == NULL)
                return (-1);
        } else {
            uri->path = NULL;
        }
    }
    *str = cur;
    return (0);
}

/**
 * Parse an path absolute and fills in the appropriate fields
 * of the `uri` structure
 *
 * path-absolute = "/" [ segment-nz *( "/" segment ) ]
 *
 * @param uri  pointer to an URI structure
 * @param str  the string to analyze
 * @returns 0 or the error code
 */
static int
xml_uri_parse_path_absolute(XmlURI *uri, const char **str)
{
    const char *cur;
    int ret;

    cur = *str;

    if (*cur != '/')
        return(1);
    cur++;
    ret = xml_uri_parse_segment(uri, &cur, 0, 0);
    if (ret == 0) {
	while (*cur == '/') {
	    cur++;
	    ret = xml_uri_parse_segment(uri, &cur, 0, 1);
	    if (ret != 0) return(ret);
	}
    }
    if (uri != NULL) {
	if (uri->path != NULL) free(uri->path);
        if (cur != *str) {
            if (uri->cleanup & XML_URI_NO_UNESCAPE)
                uri->path = STRNDUP(*str, cur - *str);
            else
                uri->path = xml_uri_unescape_string(*str, cur - *str, NULL);
            if (uri->path == NULL)
                return (-1);
        } else {
            uri->path = NULL;
        }
    }
    *str = cur;
    return (0);
}

/**
 * Parse an path without root and fills in the appropriate fields
 * of the `uri` structure
 *
 * path-rootless = segment-nz *( "/" segment )
 *
 * @param uri  pointer to an URI structure
 * @param str  the string to analyze
 * @returns 0 or the error code
 */
static int
xml_uri_parse_path_rootless(XmlURI *uri, const char **str)
{
    const char *cur;
    int ret;

    cur = *str;

    ret = xml_uri_parse_segment(uri, &cur, 0, 0);
    if (ret != 0) return(ret);
    while (*cur == '/') {
        cur++;
	ret = xml_uri_parse_segment(uri, &cur, 0, 1);
	if (ret != 0) return(ret);
    }
    if (uri != NULL) {
	if (uri->path != NULL) free(uri->path);
        if (cur != *str) {
            if (uri->cleanup & XML_URI_NO_UNESCAPE)
                uri->path = STRNDUP(*str, cur - *str);
            else
                uri->path = xml_uri_unescape_string(*str, cur - *str, NULL);
            if (uri->path == NULL)
                return (-1);
        } else {
            uri->path = NULL;
        }
    }
    *str = cur;
    return (0);
}

/**
 * Parse an path which is not a scheme and fills in the appropriate fields
 * of the `uri` structure
 *
 * path-noscheme = segment-nz-nc *( "/" segment )
 *
 * @param uri  pointer to an URI structure
 * @param str  the string to analyze
 * @returns 0 or the error code
 */
static int
xml_uri_parse_path_no_scheme(XmlURI *uri, const char **str)
{
    const char *cur;
    int ret;

    cur = *str;

    ret = xml_uri_parse_segment(uri, &cur, ':', 0);
    if (ret != 0) return(ret);
    while (*cur == '/') {
        cur++;
	ret = xml_uri_parse_segment(uri, &cur, 0, 1);
	if (ret != 0) return(ret);
    }
    if (uri != NULL) {
	if (uri->path != NULL) free(uri->path);
        if (cur != *str) {
            if (uri->cleanup & XML_URI_NO_UNESCAPE)
                uri->path = STRNDUP(*str, cur - *str);
            else
                uri->path = xml_uri_unescape_string(*str, cur - *str, NULL);
            if (uri->path == NULL)
                return (-1);
        } else {
            uri->path = NULL;
        }
    }
    *str = cur;
    return (0);
}

/**
 * Parse an hierarchical part and fills in the appropriate fields
 * of the `uri` structure
 *
 * hier-part     = "//" authority path-abempty
 *                / path-absolute
 *                / path-rootless
 *                / path-empty
 *
 * @param uri  pointer to an URI structure
 * @param str  the string to analyze
 * @returns 0 or the error code
 */
static int
xml_uri_parse_hier_part(XmlURI *uri, const char **str)
{
    const char *cur;
    int ret;

    cur = *str;

    if ((*cur == '/') && (*(cur + 1) == '/')) {
        cur += 2;
	ret = xml_uri_parse_authority(uri, &cur);
	if (ret != 0) return(ret);
        /*
         * An empty server is marked with a special URI value.
         */
	if ((uri->server == NULL) && (uri->port == PORT_EMPTY))
	    uri->port = PORT_EMPTY_SERVER;
	ret = xml_uri_parse_path_ab_empty(uri, &cur);
	if (ret != 0) return(ret);
	*str = cur;
	return(0);
    } else if (*cur == '/') {
        ret = xml_uri_parse_path_absolute(uri, &cur);
	if (ret != 0) return(ret);
    } else if (ISA_PCHAR(uri, cur)) {
        ret = xml_uri_parse_path_rootless(uri, &cur);
	if (ret != 0) return(ret);
    } else {
	/* path-empty is effectively empty */
	if (uri != NULL) {
	    if (uri->path != NULL) free(uri->path);
	    uri->path = NULL;
	}
    }
    *str = cur;
    return (0);
}

/**
 * Parse an URI string and fills in the appropriate fields
 * of the `uri` structure
 *
 * relative-ref  = relative-part [ "?" query ] [ "\#" fragment ]
 * relative-part = "//" authority path-abempty
 *               / path-absolute
 *               / path-noscheme
 *               / path-empty
 *
 * @param uri  pointer to an URI structure
 * @param str  the string to analyze
 * @returns 0 or the error code
 */
static int
xml_uri_parse_relative_ref(XmlURI *uri, const char *str) {
    int ret;

    if ((*str == '/') && (*(str + 1) == '/')) {
        str += 2;
	ret = xml_uri_parse_authority(uri, &str);
	if (ret != 0) return(ret);
	ret = xml_uri_parse_path_ab_empty(uri, &str);
	if (ret != 0) return(ret);
    } else if (*str == '/') {
	ret = xml_uri_parse_path_absolute(uri, &str);
	if (ret != 0) return(ret);
    } else if (ISA_PCHAR(uri, str)) {
        ret = xml_uri_parse_path_no_scheme(uri, &str);
	if (ret != 0) return(ret);
    } else {
	/* path-empty is effectively empty */
	if (uri != NULL) {
	    if (uri->path != NULL) free(uri->path);
	    uri->path = NULL;
	}
    }

    if (*str == '?') {
	str++;
	ret = xml_uri_parse_query(uri, &str);
	if (ret != 0) return(ret);
    }
    if (*str == '#') {
	str++;
	ret = xml_uri_parse_fragment(uri, &str);
	if (ret != 0) return(ret);
    }
    if (*str != 0) {
	xml_uri_clean(uri);
	return(1);
    }
    return(0);
}


/**
 * Parse an URI string and fills in the appropriate fields
 * of the `uri` structure
 *
 * scheme ":" hier-part [ "?" query ] [ "\#" fragment ]
 *
 * @param uri  pointer to an URI structure
 * @param str  the string to analyze
 * @returns 0 or the error code
 */
static int
xml_uri_parse_uri(XmlURI *uri, const char *str) {
    int ret;

    ret = xml_uri_parse_scheme(uri, &str);
    if (ret != 0) return(ret);
    if (*str != ':') {
	return(1);
    }
    str++;
    ret = xml_uri_parse_hier_part(uri, &str);
    if (ret != 0) return(ret);
    if (*str == '?') {
	str++;
	ret = xml_uri_parse_query(uri, &str);
	if (ret != 0) return(ret);
    }
    if (*str == '#') {
	str++;
	ret = xml_uri_parse_fragment(uri, &str);
	if (ret != 0) return(ret);
    }
    if (*str != 0) {
	xml_uri_clean(uri);
	return(1);
    }
    return(0);
}

/**
 * Parse an URI reference string and fills in the appropriate fields
 * of the `uri` structure
 *
 * URI-reference = URI / relative-ref
 *
 * @param uri  pointer to an URI structure
 * @param str  the string to analyze
 * @returns 0 or the error code
 */
static int
xml_uri_parse_uri_reference(XmlURI *uri, const char *str) {
    int ret;

    if (str == NULL)
	return(-1);
    xml_uri_clean(uri);

    /*
     * Try first to parse absolute refs, then fallback to relative if
     * it fails.
     */
    ret = xml_uri_parse_uri(uri, str);
    if (ret < 0)
        return(ret);
    if (ret != 0) {
	xml_uri_clean(uri);
        ret = xml_uri_parse_relative_ref(uri, str);
	if (ret != 0) {
	    xml_uri_clean(uri);
	    return(ret);
	}
    }
    return(0);
}

/**
 * Parse an URI based on RFC 3986
 *
 * URI-reference = [ absoluteURI | relativeURI ] [ "\#" fragment ]
 *
 * @since 2.13.0
 *
 * @param str  the URI string to analyze
 * @param uriOut  optional pointer to parsed URI
 * @returns 0 on success, an error code (typically 1) if the URI is invalid
 * or -1 if a memory allocation failed.
 */
int
xml_uri_parse_safe(const char *str, XmlURI **uriOut) {
    XmlURI *uri;
    int ret;

    if (uriOut == NULL)
        return(1);
    *uriOut = NULL;
    if (str == NULL)
	return(1);

    uri = xml_uri_create();
    if (uri == NULL)
        return(-1);

    ret = xml_uri_parse_uri_reference(uri, str);
    if (ret) {
        xml_uri_free(uri);
        return(ret);
    }

    *uriOut = uri;
    return(0);
}

/**
 * Parse an URI based on RFC 3986
 *
 * URI-reference = [ absoluteURI | relativeURI ] [ "\#" fragment ]
 *
 * @param str  the URI string to analyze
 * @returns a newly built XmlURI or NULL in case of error
 */
XmlURI *
xml_uri_parse(const char *str) {
    XmlURI *uri;
    xml_uri_parse_safe(str, &uri);
    return(uri);
}

/**
 * Parse an URI reference string based on RFC 3986 and fills in the
 * appropriate fields of the `uri` structure
 *
 * URI-reference = URI / relative-ref
 *
 * @param uri  pointer to an URI structure
 * @param str  the string to analyze
 * @returns 0 or the error code
 */
int
xml_uri_parse_reference(XmlURI *uri, const char *str) {
    return(xml_uri_parse_uri_reference(uri, str));
}

/**
 * Parse an URI but allows to keep intact the original fragments.
 *
 * URI-reference = URI / relative-ref
 *
 * @param str  the URI string to analyze
 * @param raw  if 1 unescaping of URI pieces are disabled
 * @returns a newly built XmlURI or NULL in case of error
 */
XmlURI *
xml_uri_parse_raw(const char *str, int raw) {
    XmlURI *uri;
    int ret;

    if (str == NULL)
	return(NULL);
    uri = xml_uri_create();
    if (uri != NULL) {
        if (raw) {
	    uri->cleanup |= XML_URI_NO_UNESCAPE;
	}
	ret = xml_uri_parse_reference(uri, str);
        if (ret) {
	    xml_uri_free(uri);
	    return(NULL);
	}
    }
    return(uri);
}

/************************************************************************
 *									*
 *			Generic URI structure functions			*
 *									*
 ************************************************************************/

/**
 * Simply creates an empty XmlURI
 *
 * @returns the new structure or NULL in case of error
 */
XmlURI *
xml_uri_create(void) {
    XmlURI *ret;

    ret = (XmlURI *) malloc(sizeof(XmlURI));
    if (ret == NULL)
	return(NULL);
    memset(ret, 0, sizeof(XmlURI));
    ret->port = PORT_EMPTY;
    return(ret);
}

/**
 * Function to handle properly a reallocation when saving an URI
 * Also imposes some limit on the length of an URI string output
 */
static char *
xml_uri_save_realloc(char *ret, int *max) {
    char *temp;
    int newSize;

    newSize = xml_uri_grow_capacity(*max, 1, 80, MAX_URI_LENGTH);
    if (newSize < 0)
        return(NULL);
    temp = (char*)realloc(ret, newSize + 1);
    if (temp == NULL)
        return(NULL);
    *max = newSize;
    return(temp);
}

/**
 * Save the URI as an escaped string
 *
 * @param uri  pointer to an XmlURI
 * @returns a new string (to be deallocated by caller)
 */
char *
xml_uri_save(XmlURI *uri) {
    char *ret = NULL;
    char *temp;
    const char *p;
    int len;
    int max;

    if (uri == NULL) return(NULL);


    max = 80;
    ret = (char*)malloc(max + 1);
    if (ret == NULL)
	return(NULL);
    len = 0;

    if (uri->scheme != NULL) {
	p = uri->scheme;
	while (*p != 0) {
	    if (len >= max) {
                temp = xml_uri_save_realloc(ret, &max);
                if (temp == NULL) goto mem_error;
		ret = temp;
	    }
	    ret[len++] = *p++;
	}
	if (len >= max) {
            temp = xml_uri_save_realloc(ret, &max);
            if (temp == NULL) goto mem_error;
            ret = temp;
	}
	ret[len++] = ':';
    }
    if (uri->opaque != NULL) {
	p = uri->opaque;
	while (*p != 0) {
	    if (len + 3 >= max) {
                temp = xml_uri_save_realloc(ret, &max);
                if (temp == NULL) goto mem_error;
                ret = temp;
	    }
	    if (IS_RESERVED(*(p)) || IS_UNRESERVED(*(p)))
		ret[len++] = *p++;
	    else {
		int val = *(unsigned char *)p++;
		int hi = val / 0x10, lo = val % 0x10;
		ret[len++] = '%';
		ret[len++] = hi + (hi > 9? 'A'-10 : '0');
		ret[len++] = lo + (lo > 9? 'A'-10 : '0');
	    }
	}
    } else {
	if ((uri->server != NULL) || (uri->port != PORT_EMPTY)) {
	    if (len + 3 >= max) {
                temp = xml_uri_save_realloc(ret, &max);
                if (temp == NULL) goto mem_error;
                ret = temp;
	    }
	    ret[len++] = '/';
	    ret[len++] = '/';
	    if (uri->user != NULL) {
		p = uri->user;
		while (*p != 0) {
		    if (len + 3 >= max) {
                        temp = xml_uri_save_realloc(ret, &max);
                        if (temp == NULL) goto mem_error;
                        ret = temp;
		    }
		    if ((IS_UNRESERVED(*(p))) ||
			((*(p) == ';')) || ((*(p) == ':')) ||
			((*(p) == '&')) || ((*(p) == '=')) ||
			((*(p) == '+')) || ((*(p) == '$')) ||
			((*(p) == ',')))
			ret[len++] = *p++;
		    else {
			int val = *(unsigned char *)p++;
			int hi = val / 0x10, lo = val % 0x10;
			ret[len++] = '%';
			ret[len++] = hi + (hi > 9? 'A'-10 : '0');
			ret[len++] = lo + (lo > 9? 'A'-10 : '0');
		    }
		}
		if (len + 3 >= max) {
                    temp = xml_uri_save_realloc(ret, &max);
                    if (temp == NULL) goto mem_error;
                    ret = temp;
		}
		ret[len++] = '@';
	    }
	    if (uri->server != NULL) {
		p = uri->server;
		while (*p != 0) {
		    if (len >= max) {
			temp = xml_uri_save_realloc(ret, &max);
			if (temp == NULL) goto mem_error;
			ret = temp;
		    }
                    /* TODO: escaping? */
		    ret[len++] = (char) *p++;
		}
	    }
            if (uri->port > 0) {
                if (len + 10 >= max) {
                    temp = xml_uri_save_realloc(ret, &max);
                    if (temp == NULL) goto mem_error;
                    ret = temp;
                }
                len += snprintf((char*)&ret[len], max - len, ":%d", uri->port);
            }
	} else if (uri->authority != NULL) {
	    if (len + 3 >= max) {
                temp = xml_uri_save_realloc(ret, &max);
                if (temp == NULL) goto mem_error;
                ret = temp;
	    }
	    ret[len++] = '/';
	    ret[len++] = '/';
	    p = uri->authority;
	    while (*p != 0) {
		if (len + 3 >= max) {
                    temp = xml_uri_save_realloc(ret, &max);
                    if (temp == NULL) goto mem_error;
                    ret = temp;
		}
		if ((IS_UNRESERVED(*(p))) ||
                    ((*(p) == '$')) || ((*(p) == ',')) || ((*(p) == ';')) ||
                    ((*(p) == ':')) || ((*(p) == '@')) || ((*(p) == '&')) ||
                    ((*(p) == '=')) || ((*(p) == '+')))
		    ret[len++] = *p++;
		else {
		    int val = *(unsigned char *)p++;
		    int hi = val / 0x10, lo = val % 0x10;
		    ret[len++] = '%';
		    ret[len++] = hi + (hi > 9? 'A'-10 : '0');
		    ret[len++] = lo + (lo > 9? 'A'-10 : '0');
		}
	    }
	} else if (uri->scheme != NULL) {
	    if (len + 3 >= max) {
                temp = xml_uri_save_realloc(ret, &max);
                if (temp == NULL) goto mem_error;
                ret = temp;
	    }
	}
	if (uri->path != NULL) {
	    p = uri->path;
	    /*
	     * the colon in file:///d: should not be escaped or
	     * Windows accesses fail later.
	     */
	    if ((uri->scheme != NULL) &&
		(p[0] == '/') &&
		(((p[1] >= 'a') && (p[1] <= 'z')) ||
		 ((p[1] >= 'A') && (p[1] <= 'Z'))) &&
		(p[2] == ':') &&
	        (xml_uri_streq(uri->scheme, "file"))) {
		if (len + 3 >= max) {
                    temp = xml_uri_save_realloc(ret, &max);
                    if (temp == NULL) goto mem_error;
                    ret = temp;
		}
		ret[len++] = *p++;
		ret[len++] = *p++;
		ret[len++] = *p++;
	    }
	    while (*p != 0) {
		if (len + 3 >= max) {
                    temp = xml_uri_save_realloc(ret, &max);
                    if (temp == NULL) goto mem_error;
                    ret = temp;
		}
		if ((IS_UNRESERVED(*(p))) || ((*(p) == '/')) ||
                    ((*(p) == ';')) || ((*(p) == '@')) || ((*(p) == '&')) ||
	            ((*(p) == '=')) || ((*(p) == '+')) || ((*(p) == '$')) ||
	            ((*(p) == ',')))
		    ret[len++] = *p++;
		else {
		    int val = *(unsigned char *)p++;
		    int hi = val / 0x10, lo = val % 0x10;
		    ret[len++] = '%';
		    ret[len++] = hi + (hi > 9? 'A'-10 : '0');
		    ret[len++] = lo + (lo > 9? 'A'-10 : '0');
		}
	    }
	}
	if (uri->query_raw != NULL) {
	    if (len + 1 >= max) {
                temp = xml_uri_save_realloc(ret, &max);
                if (temp == NULL) goto mem_error;
                ret = temp;
	    }
	    ret[len++] = '?';
	    p = uri->query_raw;
	    while (*p != 0) {
		if (len + 1 >= max) {
                    temp = xml_uri_save_realloc(ret, &max);
                    if (temp == NULL) goto mem_error;
                    ret = temp;
		}
		ret[len++] = *p++;
	    }
	} else if (uri->query != NULL) {
	    if (len + 3 >= max) {
                temp = xml_uri_save_realloc(ret, &max);
                if (temp == NULL) goto mem_error;
                ret = temp;
	    }
	    ret[len++] = '?';
	    p = uri->query;
	    while (*p != 0) {
		if (len + 3 >= max) {
                    temp = xml_uri_save_realloc(ret, &max);
                    if (temp == NULL) goto mem_error;
                    ret = temp;
		}
		if ((IS_UNRESERVED(*(p))) || (IS_RESERVED(*(p))))
		    ret[len++] = *p++;
		else {
		    int val = *(unsigned char *)p++;
		    int hi = val / 0x10, lo = val % 0x10;
		    ret[len++] = '%';
		    ret[len++] = hi + (hi > 9? 'A'-10 : '0');
		    ret[len++] = lo + (lo > 9? 'A'-10 : '0');
		}
	    }
	}
    }
    if (uri->fragment != NULL) {
	if (len + 3 >= max) {
            temp = xml_uri_save_realloc(ret, &max);
            if (temp == NULL) goto mem_error;
            ret = temp;
	}
	ret[len++] = '#';
	p = uri->fragment;
	while (*p != 0) {
	    if (len + 3 >= max) {
                temp = xml_uri_save_realloc(ret, &max);
                if (temp == NULL) goto mem_error;
                ret = temp;
	    }
	    if ((IS_UNRESERVED(*(p))) || (IS_RESERVED(*(p))))
		ret[len++] = *p++;
	    else {
		int val = *(unsigned char *)p++;
		int hi = val / 0x10, lo = val % 0x10;
		ret[len++] = '%';
		ret[len++] = hi + (hi > 9? 'A'-10 : '0');
		ret[len++] = lo + (lo > 9? 'A'-10 : '0');
	    }
	}
    }
    if (len >= max) {
        temp = xml_uri_save_realloc(ret, &max);
        if (temp == NULL) goto mem_error;
        ret = temp;
    }
    ret[len] = 0;
    return(ret);

mem_error:
    free(ret);
    return(NULL);
}

/**
 * Make sure the XmlURI struct is free of content
 *
 * @param uri  pointer to an XmlURI
 */
static void
xml_uri_clean(XmlURI *uri) {
    if (uri == NULL) return;

    if (uri->scheme != NULL) free(uri->scheme);
    uri->scheme = NULL;
    if (uri->server != NULL) free(uri->server);
    uri->server = NULL;
    if (uri->user != NULL) free(uri->user);
    uri->user = NULL;
    if (uri->path != NULL) free(uri->path);
    uri->path = NULL;
    if (uri->fragment != NULL) free(uri->fragment);
    uri->fragment = NULL;
    if (uri->opaque != NULL) free(uri->opaque);
    uri->opaque = NULL;
    if (uri->authority != NULL) free(uri->authority);
    uri->authority = NULL;
    if (uri->query != NULL) free(uri->query);
    uri->query = NULL;
    if (uri->query_raw != NULL) free(uri->query_raw);
    uri->query_raw = NULL;
}

/**
 * Free up the XmlURI struct
 *
 * @param uri  pointer to an XmlURI
 */
void
xml_uri_free(XmlURI *uri) {
    if (uri == NULL) return;

    if (uri->scheme != NULL) free(uri->scheme);
    if (uri->server != NULL) free(uri->server);
    if (uri->user != NULL) free(uri->user);
    if (uri->path != NULL) free(uri->path);
    if (uri->fragment != NULL) free(uri->fragment);
    if (uri->opaque != NULL) free(uri->opaque);
    if (uri->authority != NULL) free(uri->authority);
    if (uri->query != NULL) free(uri->query);
    if (uri->query_raw != NULL) free(uri->query_raw);
    free(uri);
}

/************************************************************************
 *									*
 *			Helper functions				*
 *									*
 ************************************************************************/

static int
xml_uri_is_path_separator(int c, int isFile) {
    (void) isFile;

    if (c == '/')
        return(1);

#if defined(_WIN32)
    if (isFile && (c == '\\'))
        return(1);
#endif

    return(0);
}

/**
 * Normalize a filesystem path or URI.
 *
 * @param path  pointer to the path string
 * @param isFile  true for filesystem paths, false for URIs
 * @returns 0 or an error code
 */
static int
xml_uri_normalize_path(char *path, int isFile) {
    char *cur, *out;
    int numSeg = 0;

    if (path == NULL)
	return(-1);

    cur = path;
    out = path;

    if (*cur == 0)
        return(0);

    if (xml_uri_is_path_separator(*cur, isFile)) {
        cur++;
        *out++ = '/';
    }

    while (*cur != 0) {
        /*
         * At this point, out is either empty or ends with a separator.
         * Collapse multiple separators first.
         */
        while (xml_uri_is_path_separator(*cur, isFile)) {
#if defined(_WIN32)
            /* Allow two separators at start of path */
            if ((isFile) && (out == path + 1))
                *out++ = '/';
#endif
            cur++;
        }

        if (*cur == '.') {
            if (cur[1] == 0) {
                /* Ignore "." at end of path */
                break;
            } else if (xml_uri_is_path_separator(cur[1], isFile)) {
                /* Skip "./" */
                cur += 2;
                continue;
            } else if ((cur[1] == '.') &&
                       ((cur[2] == 0) || xml_uri_is_path_separator(cur[2], isFile))) {
                if (numSeg > 0) {
                    /* Handle ".." by removing last segment */
                    do {
                        out--;
                    } while ((out > path) &&
                             !xml_uri_is_path_separator(out[-1], isFile));
                    numSeg--;

                    if (cur[2] == 0)
                        break;
                    cur += 3;
                    continue;
                } else if (out[0] == '/') {
                    /* Ignore extraneous ".." in absolute paths */
                    if (cur[2] == 0)
                        break;
                    cur += 3;
                    continue;
                } else {
                    /* Keep "../" at start of relative path */
                    numSeg--;
                }
            }
        }

        /* Copy segment */
        while ((*cur != 0) && !xml_uri_is_path_separator(*cur, isFile)) {
            *out++ = *cur++;
        }

        /* Copy separator */
        if (*cur != 0) {
            cur++;
            *out++ = '/';
        }

        numSeg++;
    }

    /* Keep "." if output is empty and it's a file */
    if ((isFile) && (out <= path))
        *out++ = '.';
    *out = 0;

    return(0);
}

/**
 * Applies the 5 normalization steps to a path string--that is, RFC 2396
 * Section 5.2, steps 6.c through 6.g.
 *
 * Normalization occurs directly on the string, no new allocation is done
 *
 * @param path  pointer to the path string
 * @returns 0 or an error code
 */
int
xml_uri_normalize_uri_path(char *path) {
    return(xml_uri_normalize_path(path, 0));
}

static int xml_uri_is_hex(char c) {
    if (((c >= '0') && (c <= '9')) ||
        ((c >= 'a') && (c <= 'f')) ||
        ((c >= 'A') && (c <= 'F')))
	return(1);
    return(0);
}

/**
 * Unescaping routine, but does not check that the string is an URI. The
 * output is a direct unsigned char translation of %XX values (no encoding)
 * Note that the length of the result can only be smaller or same size as
 * the input string.
 *
 * @param str  the string to unescape
 * @param len  the length in bytes to unescape (or <= 0 to indicate full string)
 * @param target  optional destination buffer
 * @returns a copy of the string, but unescaped, will return NULL only in case
 * of error
 */
char *
xml_uri_unescape_string(const char *str, int len, char *target) {
    char *ret, *out;
    const char *in;

    if (str == NULL)
	return(NULL);
    if (len <= 0) len = strlen(str);
    if (len < 0) return(NULL);

    if (target == NULL) {
	ret = (char*)malloc(len + 1);
	if (ret == NULL)
	    return(NULL);
    } else
	ret = target;
    in = str;
    out = ret;
    while(len > 0) {
	if ((len > 2) && (*in == '%') && (xml_uri_is_hex(in[1])) && (xml_uri_is_hex(in[2]))) {
            int c = 0;
	    in++;
	    if ((*in >= '0') && (*in <= '9'))
	        c = (*in - '0');
	    else if ((*in >= 'a') && (*in <= 'f'))
	        c = (*in - 'a') + 10;
	    else if ((*in >= 'A') && (*in <= 'F'))
	        c = (*in - 'A') + 10;
	    in++;
	    if ((*in >= '0') && (*in <= '9'))
	        c = c * 16 + (*in - '0');
	    else if ((*in >= 'a') && (*in <= 'f'))
	        c = c * 16 + (*in - 'a') + 10;
	    else if ((*in >= 'A') && (*in <= 'F'))
	        c = c * 16 + (*in - 'A') + 10;
	    in++;
	    len -= 3;
            /* Explicit sign change */
	    *out++ = (char) c;
	} else {
	    *out++ = *in++;
	    len--;
	}
    }
    *out = 0;
    return(ret);
}

/************************************************************************
 *									*
 *			Public functions				*
 *									*
 ************************************************************************/

static int
xml_uri_is_absolute_path(const char *path) {
    int c = path[0];

    if (xml_uri_is_path_separator(c, 1))
        return(1);

#if defined(_WIN32)
    if ((((c >= 'A') && (c <= 'Z')) ||
         ((c >= 'a') && (c <= 'z'))) &&
        (path[1] == ':'))
        return(1);
#endif

    return(0);
}

/**
 * Resolves a filesystem path from a base path.
 *
 * @param escRef  the filesystem path
 * @param base  the base value
 * @param out  pointer to result URI
 * @returns 0 on success, -1 if a memory allocation failed or an error
 * code if URI or base are invalid.
 */
static int
xml_uri_resolve_path(const char *escRef, const char *base, char **out) {
    const char *fragment;
    char *tmp = NULL;
    char *ref = NULL;
    char *result = NULL;
    int ret = -1;
    int i;

    if (out == NULL)
        return(1);
    *out = NULL;

    if ((escRef == NULL) || (escRef[0] == 0)) {
        if ((base == NULL) || (base[0] == 0))
            return(1);
        ref = xml_uri_strdup(base);
        if (ref == NULL)
            goto err_memory;
        *out = ref;
        return(0);
    }

    /*
     * If a URI is resolved, we can assume it is a valid URI and not
     * a filesystem path. This means we have to unescape the part
     * before the fragment.
     */
    fragment = strchr(escRef, '#');
    if (fragment != NULL) {
        tmp = xml_uri_strndup(escRef, fragment - escRef);
        if (tmp == NULL)
            goto err_memory;
        escRef = tmp;
    }

    ref = (char*)xml_uri_unescape_string((char*)escRef, -1, NULL);
    if (ref == NULL)
        goto err_memory;

    if ((base == NULL) || (base[0] == 0))
        goto done;

    if (xml_uri_is_absolute_path(ref))
        goto done;

    /*
     * Remove last segment from base
     */
    i = xml_uri_strlen(base);
    while ((i > 0) && !xml_uri_is_path_separator(base[i-1], 1))
        i--;

    /*
     * Concatenate base and ref
     */
    if (i > 0) {
        int refLen = xml_uri_strlen(ref);

        result = (char*)malloc(i + refLen + 1);
        if (result == NULL)
            goto err_memory;

        memcpy(result, base, i);
        memcpy(result + i, ref, refLen + 1);
    }

    /*
     * Normalize
     */
    xml_uri_normalize_path((char*)result, 1);

done:
    if (result == NULL) {
        result = ref;
        ref = NULL;
    }

    if (fragment != NULL) {
        result = xml_uri_strcat(result, fragment);
        if (result == NULL)
            goto err_memory;
    }

    *out = result;
    ret = 0;

err_memory:
    free(tmp);
    free(ref);
    return(ret);
}

/**
 * Computes he final URI of the reference done by checking that
 * the given URI is valid, and building the final URI using the
 * base URI. This is processed according to section 5.2 of the
 * RFC 2396
 *
 * 5.2. Resolving Relative References to Absolute Form
 *
 * @since 2.13.0
 *
 * @param URI  the URI instance found in the document
 * @param base  the base value
 * @param valPtr  pointer to result URI
 * @returns 0 on success, -1 if a memory allocation failed or an error
 * code if URI or base are invalid.
 */
int
xml_uri_build_safe(const char *URI, const char *base, char **valPtr) {
    char *val = NULL;
    int ret, len, indx, cur, out;
    XmlURI *ref = NULL;
    XmlURI *bas = NULL;
    XmlURI *res = NULL;

    if (valPtr == NULL)
        return(1);
    *valPtr = NULL;

    if (URI == NULL)
        return(1);

    if (base == NULL) {
        val = xml_uri_strdup(URI);
        if (val == NULL)
            return(-1);
        *valPtr = val;
        return(0);
    }

    /*
     * 1) The URI reference is parsed into the potential four components and
     *    fragment identifier, as described in Section 4.3.
     *
     *    NOTE that a completely empty URI is treated by modern browsers
     *    as a reference to "." rather than as a synonym for the current
     *    URI.  Should we do that here?
     */
    if (URI[0] != 0)
        ret = xml_uri_parse_safe((const char*)URI, &ref);
    else
        ret = 0;
    if (ret != 0)
	goto done;
    if ((ref != NULL) && (ref->scheme != NULL)) {
	/*
	 * The URI is absolute don't modify.
	 */
	val = xml_uri_strdup(URI);
        if (val == NULL)
            ret = -1;
	goto done;
    }

    /*
     * If base has no scheme or authority, it is assumed to be a
     * filesystem path.
     */
    if (strstr(base, "://") == NULL) {
        xml_uri_free(ref);
        return(xml_uri_resolve_path(URI, base, valPtr));
    }

#if defined(_WIN32)
    /*
     * Resolve paths with a Windows drive letter as filesystem path
     * even if base has a scheme.
     */
    if ((ref != NULL) && (ref->path != NULL)) {
        int c = ref->path[0];

        if ((((c >= 'A') && (c <= 'Z')) ||
             ((c >= 'a') && (c <= 'z'))) &&
            (ref->path[1] == ':')) {
            xml_uri_free(ref);
            return(xml_uri_resolve_path(URI, base, valPtr));
        }
    }
#endif

    ret = xml_uri_parse_safe((const char*)base, &bas);
    if (ret < 0)
        goto done;
    if (ret != 0) {
	if (ref) {
            ret = 0;
	    val = xml_uri_save(ref);
            if (val == NULL)
                ret = -1;
        }
	goto done;
    }
    if (ref == NULL) {
	/*
	 * the base fragment must be ignored
	 */
	if (bas->fragment != NULL) {
	    free(bas->fragment);
	    bas->fragment = NULL;
	}
	val = xml_uri_save(bas);
        if (val == NULL)
            ret = -1;
	goto done;
    }

    /*
     * 2) If the path component is empty and the scheme, authority, and
     *    query components are undefined, then it is a reference to the
     *    current document and we are done.  Otherwise, the reference URI's
     *    query and fragment components are defined as found (or not found)
     *    within the URI reference and not inherited from the base URI.
     *
     *    NOTE that in modern browsers, the parsing differs from the above
     *    in the following aspect:  the query component is allowed to be
     *    defined while still treating this as a reference to the current
     *    document.
     */
    ret = -1;
    res = xml_uri_create();
    if (res == NULL)
	goto done;
    if ((ref->scheme == NULL) && (ref->path == NULL) &&
	((ref->authority == NULL) && (ref->server == NULL) &&
         (ref->port == PORT_EMPTY))) {
	if (bas->scheme != NULL) {
	    res->scheme = xml_uri_strdup(bas->scheme);
            if (res->scheme == NULL)
                goto done;
        }
	if (bas->authority != NULL) {
	    res->authority = xml_uri_strdup(bas->authority);
            if (res->authority == NULL)
                goto done;
        } else {
	    if (bas->server != NULL) {
		res->server = xml_uri_strdup(bas->server);
                if (res->server == NULL)
                    goto done;
            }
	    if (bas->user != NULL) {
		res->user = xml_uri_strdup(bas->user);
                if (res->user == NULL)
                    goto done;
            }
	    res->port = bas->port;
	}
	if (bas->path != NULL) {
	    res->path = xml_uri_strdup(bas->path);
            if (res->path == NULL)
                goto done;
        }
	if (ref->query_raw != NULL) {
	    res->query_raw = xml_uri_strdup (ref->query_raw);
            if (res->query_raw == NULL)
                goto done;
        } else if (ref->query != NULL) {
	    res->query = xml_uri_strdup(ref->query);
            if (res->query == NULL)
                goto done;
        } else if (bas->query_raw != NULL) {
	    res->query_raw = xml_uri_strdup(bas->query_raw);
            if (res->query_raw == NULL)
                goto done;
        } else if (bas->query != NULL) {
	    res->query = xml_uri_strdup(bas->query);
            if (res->query == NULL)
                goto done;
        }
	if (ref->fragment != NULL) {
	    res->fragment = xml_uri_strdup(ref->fragment);
            if (res->fragment == NULL)
                goto done;
        }
	goto step_7;
    }

    /*
     * 3) If the scheme component is defined, indicating that the reference
     *    starts with a scheme name, then the reference is interpreted as an
     *    absolute URI and we are done.  Otherwise, the reference URI's
     *    scheme is inherited from the base URI's scheme component.
     */
    if (ref->scheme != NULL) {
	val = xml_uri_save(ref);
        if (val != NULL)
            ret = 0;
	goto done;
    }
    if (bas->scheme != NULL) {
	res->scheme = xml_uri_strdup(bas->scheme);
        if (res->scheme == NULL)
            goto done;
    }

    if (ref->query_raw != NULL) {
	res->query_raw = xml_uri_strdup(ref->query_raw);
        if (res->query_raw == NULL)
            goto done;
    } else if (ref->query != NULL) {
	res->query = xml_uri_strdup(ref->query);
        if (res->query == NULL)
            goto done;
    }
    if (ref->fragment != NULL) {
	res->fragment = xml_uri_strdup(ref->fragment);
        if (res->fragment == NULL)
            goto done;
    }

    /*
     * 4) If the authority component is defined, then the reference is a
     *    network-path and we skip to step 7.  Otherwise, the reference
     *    URI's authority is inherited from the base URI's authority
     *    component, which will also be undefined if the URI scheme does not
     *    use an authority component.
     */
    if ((ref->authority != NULL) || (ref->server != NULL) ||
         (ref->port != PORT_EMPTY)) {
	if (ref->authority != NULL) {
	    res->authority = xml_uri_strdup(ref->authority);
            if (res->authority == NULL)
                goto done;
        } else {
            if (ref->server != NULL) {
                res->server = xml_uri_strdup(ref->server);
                if (res->server == NULL)
                    goto done;
            }
	    if (ref->user != NULL) {
		res->user = xml_uri_strdup(ref->user);
                if (res->user == NULL)
                    goto done;
            }
            res->port = ref->port;
	}
	if (ref->path != NULL) {
	    res->path = xml_uri_strdup(ref->path);
            if (res->path == NULL)
                goto done;
        }
	goto step_7;
    }
    if (bas->authority != NULL) {
	res->authority = xml_uri_strdup(bas->authority);
        if (res->authority == NULL)
            goto done;
    } else if ((bas->server != NULL) || (bas->port != PORT_EMPTY)) {
	if (bas->server != NULL) {
	    res->server = xml_uri_strdup(bas->server);
            if (res->server == NULL)
                goto done;
        }
	if (bas->user != NULL) {
	    res->user = xml_uri_strdup(bas->user);
            if (res->user == NULL)
                goto done;
        }
	res->port = bas->port;
    }

    /*
     * 5) If the path component begins with a slash character ("/"), then
     *    the reference is an absolute-path and we skip to step 7.
     */
    if ((ref->path != NULL) && (ref->path[0] == '/')) {
	res->path = xml_uri_strdup(ref->path);
        if (res->path == NULL)
            goto done;
	goto step_7;
    }


    /*
     * 6) If this step is reached, then we are resolving a relative-path
     *    reference.  The relative path needs to be merged with the base
     *    URI's path.  Although there are many ways to do this, we will
     *    describe a simple method using a separate string buffer.
     *
     * Allocate a buffer large enough for the result string.
     */
    len = 2; /* extra / and 0 */
    if (ref->path != NULL)
	len += strlen(ref->path);
    if (bas->path != NULL)
	len += strlen(bas->path);
    res->path = (char*)malloc(len);
    if (res->path == NULL)
	goto done;
    res->path[0] = 0;

    /*
     * a) All but the last segment of the base URI's path component is
     *    copied to the buffer.  In other words, any characters after the
     *    last (right-most) slash character, if any, are excluded.
     */
    cur = 0;
    out = 0;
    if (bas->path != NULL) {
	while (bas->path[cur] != 0) {
	    while ((bas->path[cur] != 0) && (bas->path[cur] != '/'))
		cur++;
	    if (bas->path[cur] == 0)
		break;

	    cur++;
	    while (out < cur) {
		res->path[out] = bas->path[out];
		out++;
	    }
	}
    }
    res->path[out] = 0;

    /*
     * b) The reference's path component is appended to the buffer
     *    string.
     */
    if (ref->path != NULL && ref->path[0] != 0) {
	indx = 0;
	/*
	 * Ensure the path includes a '/'
	 */
	if ((out == 0) && ((bas->server != NULL) || bas->port != PORT_EMPTY))
	    res->path[out++] = '/';
	while (ref->path[indx] != 0) {
	    res->path[out++] = ref->path[indx++];
	}
    }
    res->path[out] = 0;

    /*
     * Steps c) to h) are really path normalization steps
     */
    xml_uri_normalize_uri_path(res->path);

step_7:

    /*
     * 7) The resulting URI components, including any inherited from the
     *    base URI, are recombined to give the absolute form of the URI
     *    reference.
     */
    val = xml_uri_save(res);
    if (val != NULL)
        ret = 0;

done:
    if (ref != NULL)
	xml_uri_free(ref);
    if (bas != NULL)
	xml_uri_free(bas);
    if (res != NULL)
	xml_uri_free(res);
    *valPtr = val;
    return(ret);
}

/**
 * Computes he final URI of the reference done by checking that
 * the given URI is valid, and building the final URI using the
 * base URI. This is processed according to section 5.2 of the
 * RFC 2396
 *
 * 5.2. Resolving Relative References to Absolute Form
 *
 * @param URI  the URI instance found in the document
 * @param base  the base value
 * @returns a new URI string (to be freed by the caller) or NULL in case
 *         of error.
 */
char *
xml_uri_build(const char *URI, const char *base) {
    char *out;

    xml_uri_build_safe(URI, base, &out);
    return(out);
}

/**
 * Port of xmlURIEscapeStr: escape a string to hex, except unreserved characters,
 * "@" and the characters in list.
 */
char *
xml_uri_escape_str(const char *str, const char *list) {
    if (str == NULL)
	return(NULL);
    if (str[0] == 0)
	return(xml_uri_strdup(str));
    size_t len = strlen(str) + 20;
    size_t out = 0;
    char *ret = (char*)malloc(len);
    if (ret == NULL)
	return(NULL);
    for (const unsigned char *in = (const unsigned char*)str; *in != 0; ) {
	if (len - out <= 3) {
            len *= 2;
            char *temp = (char*)realloc(ret, len);
	    if (temp == NULL) {
		free(ret);
		return(NULL);
	    }
	    ret = temp;
	}
	unsigned char ch = *in;
	if ((ch != '@') && (!IS_UNRESERVED(ch)) && (!(list && strchr(list, ch)))) {
	    unsigned char val;
	    ret[out++] = '%';
	    val = ch >> 4;
	    ret[out++] = (char)(val <= 9 ? '0' + val : 'A' + val - 0xA);
	    val = ch & 0xF;
	    ret[out++] = (char)(val <= 9 ? '0' + val : 'A' + val - 0xA);
	    in++;
	} else {
	    ret[out++] = (char)*in++;
	}
    }
    ret[out] = 0;
    return(ret);
}

#undef MAX_URI_LENGTH
#undef PORT_EMPTY
#undef PORT_EMPTY_SERVER
#undef IS_ALPHA
#undef IS_LOWALPHA
#undef IS_UPALPHA
#undef IS_DIGIT
#undef IS_ALPHANUM
#undef IS_MARK
#undef IS_UNWISE
#undef IS_RESERVED
#undef IS_UNRESERVED
#undef NEXT
#undef STRNDUP
#undef ISA_DIGIT
#undef ISA_ALPHA
#undef ISA_HEXDIG
#undef ISA_SUB_DELIM
#undef ISA_GEN_DELIM
#undef ISA_RESERVED
#undef ISA_STRICTLY_UNRESERVED
#undef ISA_PCT_ENCODED
#undef ISA_PCHAR
#undef ISA_UCSCHAR
#undef ISA_UNRESERVED
#undef XML_URI_ALLOW_UNWISE
#undef XML_URI_NO_UNESCAPE
#undef XML_URI_ALLOW_UCSCHAR

#endif // XML_URI_MODULE
