/* This file is ported from libxml2 (pattern.c, version 2.16.0 as found
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

#include"xml_pattern.h"
#include"xml_dom.h"

#include<stdint.h>
#include<stdlib.h>
#include<string.h>

#endif

#ifdef XML_PATTERN_MODULE

/*
 * pattern.c: Implementation of selectors for nodes
 *
 * Reference:
 *   http://www.w3.org/TR/2001/REC-xmlschema-1-20010502/
 *   to some extent
 *   http://www.w3.org/TR/1999/REC-xml-19991116
 *
 * Author: Daniel Veillard
 */

/*
 * TODO:
 * - compilation flags to check for specific syntaxes
 *   using flags of #xmlPatterncompile
 * - making clear how pattern starting with / or . need to be handled,
 *   currently push(NULL, NULL) means a reset of the streaming context
 *   and indicating we are on / (the document node), probably need
 *   something similar for .
 */

// The macros of libxml2 pattern.c, not shared with the other parts (dist/xmq.c is one file).
#undef ERROR
#undef ERROR5
#undef XML_STREAM_STEP_DESC
#undef XML_STREAM_STEP_FINAL
#undef XML_STREAM_STEP_ROOT
#undef XML_STREAM_STEP_ATTR
#undef XML_STREAM_STEP_NODE
#undef XML_STREAM_STEP_IN_SET
#undef XML_STREAM_FINAL_IS_ANY_NODE
#undef XML_STREAM_FROM_ROOT
#undef XML_STREAM_DESC
#undef XML_STREAM_ANY_NODE
#undef XML_PATTERN_NOTPATTERN
#undef XML_STREAM_XS_IDC
#undef XML_STREAM_XS_IDC_SEL
#undef XML_STREAM_XS_IDC_FIELD
#undef XML_PAT_COPY_NSNAME
#undef XML_PAT_FREE_STRING
#undef XML_PAT_MAX_ITEMS
#undef XML_PAT_MAX_NAME_LENGTH
#undef XML_PAT_XML_NAMESPACE
#undef XML_PAT_ELEMENT_NODE
#undef XML_PAT_ATTRIBUTE_NODE
#undef PAT_FROM_ROOT
#undef PAT_FROM_CUR
#undef CUR
#undef SKIP
#undef NXT
#undef PEEKPREV
#undef CUR_PTR
#undef SKIP_BLANKS
#undef CURRENT
#undef NEXT
#undef PUSH
#undef XML_PAT_IS_BLANK_CH

/* libxml2 2.16 compiles the error messages of pattern.c away. */
#define ERROR(a, b, c, d)
#define ERROR5(a, b, c, d, e)

#define XML_STREAM_STEP_DESC	1
#define XML_STREAM_STEP_FINAL	2
#define XML_STREAM_STEP_ROOT	4
#define XML_STREAM_STEP_ATTR	8
#define XML_STREAM_STEP_NODE	16
#define XML_STREAM_STEP_IN_SET	32

/*
* NOTE: Those private flags (XML_STREAM_xxx) are used
*   in XmlStreamCtxt->flag. They extend the public
*   pattern flags, so be careful not to interfere with the
*   reserved values for the pattern flags.
*/
#define XML_STREAM_FINAL_IS_ANY_NODE 1<<14
#define XML_STREAM_FROM_ROOT 1<<15
#define XML_STREAM_DESC 1<<16

/*
* XML_STREAM_ANY_NODE is used for comparison against
* node type values, to indicate a node of any type.
*/
#define XML_STREAM_ANY_NODE 100

#define XML_PATTERN_NOTPATTERN  (XML_PATTERN_FLAG_XPATH | \
				 XML_PATTERN_FLAG_XSSEL | \
				 XML_PATTERN_FLAG_XSFIELD)

#define XML_STREAM_XS_IDC(c) ((c)->flags & \
    (XML_PATTERN_FLAG_XSSEL | XML_PATTERN_FLAG_XSFIELD))

#define XML_STREAM_XS_IDC_SEL(c) ((c)->flags & XML_PATTERN_FLAG_XSSEL)

#define XML_STREAM_XS_IDC_FIELD(c) ((c)->flags & XML_PATTERN_FLAG_XSFIELD)

/* Without a dictionary (libxml2 xmlDict) the strings are always copied. */
#define XML_PAT_COPY_NSNAME(c, r, nsname) \
    r = xml_pattern_strdup(nsname);

#define XML_PAT_FREE_STRING(c, r) free(r);

/* XML_MAX_ITEMS (private/memory.h) and XML_MAX_NAME_LENGTH (parserInternals.h). */
#define XML_PAT_MAX_ITEMS 1000000000
#define XML_PAT_MAX_NAME_LENGTH 50000
#define XML_PAT_XML_NAMESPACE "http://www.w3.org/XML/1998/namespace"

/* The libxml2 xmlElementType values, the same as the XmlNodeKind values. */
#define XML_PAT_ELEMENT_NODE 1
#define XML_PAT_ATTRIBUTE_NODE 2

/* IS_BLANK_CH of libxml2. */
#define XML_PAT_IS_BLANK_CH(c) (((c) == 0x20) || ((0x9 <= (c)) && ((c) <= 0xa)) || ((c) == 0xd))

typedef struct XmlPatStreamStep XmlPatStreamStep;
struct XmlPatStreamStep {
    int flags;			/* properties of that step */
    const char *name;		/* first string value if NULL accept all */
    const char *ns;		/* second string value */
    int nodeType;		/* type of node */
};

typedef struct XmlPatStreamComp XmlPatStreamComp;
struct XmlPatStreamComp {
    int nbStep;			/* number of steps in the automata */
    int maxStep;		/* allocated number of steps */
    XmlPatStreamStep *steps;	/* the array of steps */
    int flags;
};

struct XmlStreamCtxt {
    struct XmlStreamCtxt *next;/* link to next sub pattern if | */
    XmlPatStreamComp *comp;	/* the compiled stream */
    int nbState;		/* number of states in the automata */
    int maxState;		/* allocated number of states */
    int level;			/* how deep are we ? */
    int *states;		/* the array of step indexes */
    int flags;			/* validation options */
    int blockLevel;
};

static void xml_stream_comp_free(XmlPatStreamComp *comp);

/*
 * Types are private:
 */

typedef enum {
    XML_PAT_OP_END=0,
    XML_PAT_OP_ROOT,
    XML_PAT_OP_ELEM,
    XML_PAT_OP_CHILD,
    XML_PAT_OP_ATTR,
    XML_PAT_OP_PARENT,
    XML_PAT_OP_ANCESTOR,
    XML_PAT_OP_NS,
    XML_PAT_OP_ALL
} XmlPatOp;


typedef struct XmlPatStepState XmlPatStepState;
struct XmlPatStepState {
    int step;
    XMQNode *node;
};

typedef struct XmlPatStepStates XmlPatStepStates;
struct XmlPatStepStates {
    int nbstates;
    int maxstates;
    XmlPatStepState *states;
};

typedef struct XmlPatStepOp XmlPatStepOp;
struct XmlPatStepOp {
    XmlPatOp op;
    const char *value;
    const char *value2; /* The namespace name */
};

#define PAT_FROM_ROOT	(1<<8)
#define PAT_FROM_CUR	(1<<9)

struct XmlPattern {
    void *data;		/* the associated template */
    struct XmlPattern *next;	/* next pattern if | is used */
    const char *pattern;	/* the pattern */
    int flags;			/* flags */
    int nbStep;
    int maxStep;
    XmlPatStepOp *steps;        /* ops for computation */
    XmlPatStreamComp *stream;	/* the streaming data if any */
};

typedef struct XmlPatParserContext XmlPatParserContext;
struct XmlPatParserContext {
    const char *cur;			/* the current char being parsed */
    const char *base;			/* the full expression */
    int	        error;			/* error code */
    XmlPattern *comp;			/* the result */
    XMQNode    *elem;			/* the current node if any */
    const char **namespaces;		/* the namespaces definitions */
    int   nb_namespaces;		/* the number of namespaces */
};

/************************************************************************
 *									*
 *			Helpers (libxml2 xmlstring.c, memory.h, parser.c)	*
 *									*
 ************************************************************************/

static char *xml_pattern_strdup(const char *s)
{
    if (!s) return NULL;
    return strdup(s);
}

/** Port of xmlStrndup. */
static char *xml_pattern_strndup(const char *s, size_t len)
{
    if (!s) return NULL;
    char *r = (char*)malloc(len + 1);
    if (!r) return NULL;
    memcpy(r, s, len);
    r[len] = 0;
    return r;
}

/** Port of xmlStrEqual. */
static int xml_pattern_str_equal(const char *a, const char *b)
{
    if (a == b) return 1;
    if (!a || !b) return 0;
    return !strcmp(a, b);
}

/** Port of xmlGrowCapacity (private/memory.h). */
static int xml_pattern_grow_capacity(int capacity, size_t elemSize, int min, int max)
{
    if (capacity <= 0) return min;
    if ((capacity >= max) || ((size_t)capacity > SIZE_MAX / 2 / elemSize)) return -1;
    int extra = (capacity + 1) / 2;
    if (capacity > max - extra) return max;
    return capacity + extra;
}

/** Port of xmlGetUTF8Char (xmlstring.c). */
static int xml_pattern_get_utf8_char(const unsigned char *utf, int *len)
{
    unsigned int c;

    c = utf[0];
    if (c < 0x80) {
        if (*len < 1)
            goto error;
        /* 1-byte code */
        *len = 1;
    } else {
        if ((*len < 2) || ((utf[1] & 0xc0) != 0x80))
            goto error;
        if (c < 0xe0) {
            if (c < 0xc2)
                goto error;
            /* 2-byte code */
            *len = 2;
            c = (c & 0x1f) << 6;
            c |= utf[1] & 0x3f;
        } else {
            if ((*len < 3) || ((utf[2] & 0xc0) != 0x80))
                goto error;
            if (c < 0xf0) {
                /* 3-byte code */
                *len = 3;
                c = (c & 0xf) << 12;
                c |= (utf[1] & 0x3f) << 6;
                c |= utf[2] & 0x3f;
                if ((c < 0x800) || ((c >= 0xd800) && (c < 0xe000)))
                    goto error;
            } else {
                if ((*len < 4) || ((utf[3] & 0xc0) != 0x80))
                    goto error;
                *len = 4;
                /* 4-byte code */
                c = (c & 0x7) << 18;
                c |= (utf[1] & 0x3f) << 12;
                c |= (utf[2] & 0x3f) << 6;
                c |= utf[3] & 0x3f;
                if ((c < 0x10000) || (c >= 0x110000))
                    goto error;
            }
        }
    }
    return (int)c;

error:
    *len = 0;
    return(-1);
}

/** Port of xmlIsNameStartCharNew (parser.c). */
static int xml_pattern_is_name_start_char(int c)
{
    if ((c != ' ') && (c != '>') && (c != '/') && /* accelerators */
        (((c >= 'a') && (c <= 'z')) ||
         ((c >= 'A') && (c <= 'Z')) ||
         (c == '_') || (c == ':') ||
         ((c >= 0xC0) && (c <= 0xD6)) ||
         ((c >= 0xD8) && (c <= 0xF6)) ||
         ((c >= 0xF8) && (c <= 0x2FF)) ||
         ((c >= 0x370) && (c <= 0x37D)) ||
         ((c >= 0x37F) && (c <= 0x1FFF)) ||
         ((c >= 0x200C) && (c <= 0x200D)) ||
         ((c >= 0x2070) && (c <= 0x218F)) ||
         ((c >= 0x2C00) && (c <= 0x2FEF)) ||
         ((c >= 0x3001) && (c <= 0xD7FF)) ||
         ((c >= 0xF900) && (c <= 0xFDCF)) ||
         ((c >= 0xFDF0) && (c <= 0xFFFD)) ||
         ((c >= 0x10000) && (c <= 0xEFFFF))))
        return(1);
    return(0);
}

/** Port of xmlIsNameCharNew (parser.c). */
static int xml_pattern_is_name_char(int c)
{
    if ((c != ' ') && (c != '>') && (c != '/') && /* accelerators */
        (((c >= 'a') && (c <= 'z')) ||
         ((c >= 'A') && (c <= 'Z')) ||
         ((c >= '0') && (c <= '9')) || /* !start */
         (c == '_') || (c == ':') ||
         (c == '-') || (c == '.') || (c == 0xB7) || /* !start */
         ((c >= 0xC0) && (c <= 0xD6)) ||
         ((c >= 0xD8) && (c <= 0xF6)) ||
         ((c >= 0xF8) && (c <= 0x2FF)) ||
         ((c >= 0x300) && (c <= 0x36F)) || /* !start */
         ((c >= 0x370) && (c <= 0x37D)) ||
         ((c >= 0x37F) && (c <= 0x1FFF)) ||
         ((c >= 0x200C) && (c <= 0x200D)) ||
         ((c >= 0x203F) && (c <= 0x2040)) || /* !start */
         ((c >= 0x2070) && (c <= 0x218F)) ||
         ((c >= 0x2C00) && (c <= 0x2FEF)) ||
         ((c >= 0x3001) && (c <= 0xD7FF)) ||
         ((c >= 0xF900) && (c <= 0xFDCF)) ||
         ((c >= 0xFDF0) && (c <= 0xFFFD)) ||
         ((c >= 0x10000) && (c <= 0xEFFFF))))
         return(1);
    return(0);
}

/**
 * Port of xmlScanName (parser.c), for XML 1.0 5th edition names.
 * Scan an XML Name (nc 0) or NCName (nc 1).
 *
 * Returns a pointer to the end of the name on success. If the
 * name is invalid, returns `ptr`. If the name is longer than
 * `maxSize` bytes, returns NULL.
 */
static const char *xml_pattern_scan_name_end(const char *ptr, size_t maxSize, int nc)
{
    int stop = nc ? ':' : 0;
    int first = 1;

    while (1) {
        int c, len;

        c = *(const unsigned char*)ptr;
        if (c < 0x80) {
            if (c == stop)
                break;
            len = 1;
        } else {
            len = 4;
            c = xml_pattern_get_utf8_char((const unsigned char*)ptr, &len);
            if (c < 0)
                break;
        }

        if (first ? !xml_pattern_is_name_start_char(c) : !xml_pattern_is_name_char(c))
            break;

        if ((size_t) len > maxSize)
            return(NULL);
        ptr += len;
        maxSize -= len;
        first = 0;
    }

    return(ptr);
}

/************************************************************************
 *									*
 *			Type functions					*
 *									*
 ************************************************************************/

/**
 * Port of xmlNewPattern.
 * Create a new XSLT Pattern
 *
 * @returns the newly allocated XmlPattern or NULL in case of error
 */
static XmlPattern *
xml_pattern_new(void) {
    XmlPattern *cur;

    cur = (XmlPattern *) malloc(sizeof(XmlPattern));
    if (cur == NULL) {
	ERROR(NULL, NULL, NULL,
		"xmlNewPattern : malloc failed\n");
	return(NULL);
    }
    memset(cur, 0, sizeof(XmlPattern));
    cur->steps = NULL;
    cur->maxStep = 0;
    return(cur);
}

/**
 * Port of xmlFreePattern.
 * Free up the memory allocated by `comp`
 *
 * @param comp  an XSLT comp
 */
void
xml_pattern_free(XmlPattern *comp) {
    xml_pattern_free_list(comp);
}

/** Port of xmlFreePatternInternal. */
static void
xml_pattern_free_internal(XmlPattern *comp) {
    XmlPatStepOp *op;
    int i;

    if (comp == NULL)
	return;
    if (comp->stream != NULL)
        xml_stream_comp_free(comp->stream);
    if (comp->pattern != NULL)
	free((char *)comp->pattern);
    if (comp->steps != NULL) {
	for (i = 0;i < comp->nbStep;i++) {
	    op = &comp->steps[i];
	    if (op->value != NULL)
		free((char *) op->value);
	    if (op->value2 != NULL)
		free((char *) op->value2);
	}
	free(comp->steps);
    }

    memset(comp, -1, sizeof(XmlPattern));
    free(comp);
}

/**
 * Port of xmlFreePatternList.
 * Free up the memory allocated by all the elements of `comp`
 *
 * @param comp  an XSLT comp list
 */
void
xml_pattern_free_list(XmlPattern *comp) {
    XmlPattern *cur;

    while (comp != NULL) {
	cur = comp;
	comp = comp->next;
	cur->next = NULL;
	xml_pattern_free_internal(cur);
    }
}

/**
 * Port of xmlNewPatParserContext.
 * Create a new XML pattern parser context
 *
 * @param pattern  the pattern context
 * @param namespaces  the prefix definitions, array of [URI, prefix] terminated
 *              with [NULL, NULL] or NULL if no namespace is used
 * @returns the newly allocated XmlPatParserContext or NULL in case of error
 */
static XmlPatParserContext *
xml_pattern_new_parser_context(const char *pattern, const char **namespaces) {
    XmlPatParserContext *cur;

    if (pattern == NULL)
        return(NULL);

    cur = (XmlPatParserContext *) malloc(sizeof(XmlPatParserContext));
    if (cur == NULL) {
	ERROR(NULL, NULL, NULL,
		"xmlNewPatParserContext : malloc failed\n");
	return(NULL);
    }
    memset(cur, 0, sizeof(XmlPatParserContext));
    cur->cur = pattern;
    cur->base = pattern;
    if (namespaces != NULL) {
        int i;
        for (i = 0;namespaces[2 * i] != NULL;i++)
            ;
        cur->nb_namespaces = i;
    } else {
        cur->nb_namespaces = 0;
    }
    cur->namespaces = namespaces;
    return(cur);
}

/**
 * Port of xmlFreePatParserContext.
 * Free up the memory allocated by `ctxt`
 *
 * @param ctxt  an XSLT parser context
 */
static void
xml_pattern_free_parser_context(XmlPatParserContext *ctxt) {
    if (ctxt == NULL)
	return;
    memset(ctxt, -1, sizeof(XmlPatParserContext));
    free(ctxt);
}

/** Port of xmlPatternGrow. */
static int
xml_pattern_grow(XmlPattern *comp) {
    XmlPatStepOp *temp;
    int newSize;

    newSize = xml_pattern_grow_capacity(comp->maxStep, sizeof(temp[0]),
                              10, XML_PAT_MAX_ITEMS);
    if (newSize < 0)
        return(-1);
    temp = (XmlPatStepOp *) realloc(comp->steps, newSize * sizeof(temp[0]));
    if (temp == NULL)
        return(-1);
    comp->steps = temp;
    comp->maxStep = newSize;

    return(0);
}

/**
 * Port of xmlPatternAdd.
 * Add a step to an XSLT Compiled Match
 *
 * @param ctxt  the pattern parser context
 * @param comp  the compiled match expression
 * @param op  an op
 * @param value  the first value
 * @param value2  the second value
 * @returns -1 in case of failure, 0 otherwise.
 */
static int
xml_pattern_add(XmlPatParserContext *ctxt, XmlPattern *comp,
              XmlPatOp op, char * value, char * value2)
{
    if (comp->nbStep >= comp->maxStep) {
        if (xml_pattern_grow(comp) < 0) {
            ctxt->error = -1;
            return(-1);
        }
    }
    comp->steps[comp->nbStep].op = op;
    comp->steps[comp->nbStep].value = value;
    comp->steps[comp->nbStep].value2 = value2;
    comp->nbStep++;
    return(0);
}

/**
 * Port of xmlReversePattern.
 * reverse all the stack of expressions
 *
 * @param comp  the compiled match expression
 * @returns 0 in case of success and -1 in case of error.
 */
static int
xml_pattern_reverse(XmlPattern *comp) {
    int i, j;

    /*
     * remove the leading // for //a or .//a
     */
    if ((comp->nbStep > 0) && (comp->steps[0].op == XML_PAT_OP_ANCESTOR)) {
        for (i = 0, j = 1;j < comp->nbStep;i++,j++) {
	    comp->steps[i].value = comp->steps[j].value;
	    comp->steps[i].value2 = comp->steps[j].value2;
	    comp->steps[i].op = comp->steps[j].op;
	}
	comp->nbStep--;
    }

    /*
     * Grow to add OP_END later
     */
    if (comp->nbStep >= comp->maxStep) {
        if (xml_pattern_grow(comp) < 0)
            return(-1);
    }

    i = 0;
    j = comp->nbStep - 1;
    while (j > i) {
	const char *tmp;
	XmlPatOp op;
	tmp = comp->steps[i].value;
	comp->steps[i].value = comp->steps[j].value;
	comp->steps[j].value = tmp;
	tmp = comp->steps[i].value2;
	comp->steps[i].value2 = comp->steps[j].value2;
	comp->steps[j].value2 = tmp;
	op = comp->steps[i].op;
	comp->steps[i].op = comp->steps[j].op;
	comp->steps[j].op = op;
	j--;
	i++;
    }

    comp->steps[comp->nbStep].value = NULL;
    comp->steps[comp->nbStep].value2 = NULL;
    comp->steps[comp->nbStep++].op = XML_PAT_OP_END;
    return(0);
}

/************************************************************************
 *									*
 *		The interpreter for the precompiled patterns		*
 *									*
 ************************************************************************/

/** Port of xmlPatPushState. */
static int
xml_pattern_push_state(XmlPatStepStates *states, int step, XMQNode *node) {
    if (states->maxstates <= states->nbstates) {
        XmlPatStepState *tmp;
        int newSize;

        newSize = xml_pattern_grow_capacity(states->maxstates, sizeof(tmp[0]),
                                  4, XML_PAT_MAX_ITEMS);
        if (newSize < 0)
	    return(-1);
	tmp = (XmlPatStepState *) realloc(states->states, newSize * sizeof(tmp[0]));
	if (tmp == NULL)
	    return(-1);
	states->states = tmp;
	states->maxstates = newSize;
    }
    states->states[states->nbstates].step = step;
    states->states[states->nbstates++].node = node;
    return(0);
}

/** The namespace href of an element or attribute (libxml2 node->ns->href), NULL if none. */
static const char *xml_pattern_node_ns_href(const XMQNode *node)
{
    XmlNs *ns = xml_node_ns(node);
    return ns ? xml_ns_href(ns) : NULL;
}

/**
 * Port of xmlPatMatch.
 * Test whether the node matches the pattern
 *
 * @param comp  the precompiled pattern
 * @param node  a node
 * @returns 1 if it matches, 0 if it doesn't and -1 in case of failure
 */
static int
xml_pattern_pat_match(XmlPattern *comp, XMQNode *node) {
    int i;
    XmlPatStepOp *step;
    XmlPatStepStates states = {0, 0, NULL}; /* // may require backtrack */

    if ((comp == NULL) || (node == NULL)) return(-1);
    i = 0;
restart:
    for (;i < comp->nbStep;i++) {
	step = &comp->steps[i];
	switch (step->op) {
            case XML_PAT_OP_END:
		goto found;
            case XML_PAT_OP_ROOT:
		if (xml_node_kind(node) == XML_DOM_NAMESPACE)
		    goto rollback;
		node = xml_node_parent(node);
		/* libxml2 dereferences the parent, only a detached node has none. */
		if (node == NULL)
		    goto rollback;
		if (xml_node_kind(node) == XML_DOM_DOCUMENT)
		    continue;
		goto rollback;
            case XML_PAT_OP_ELEM:
		if (xml_node_kind(node) != XML_DOM_ELEMENT)
		    goto rollback;
		if (step->value == NULL)
		    continue;
		if (step->value[0] != xml_node_name(node)[0])
		    goto rollback;
		if (!xml_pattern_str_equal(step->value, xml_node_name(node)))
		    goto rollback;

		/* Namespace test */
		if (xml_node_ns(node) == NULL) {
		    if (step->value2 != NULL)
			goto rollback;
		} else if (xml_pattern_node_ns_href(node) != NULL) {
		    if (step->value2 == NULL)
			goto rollback;
		    if (!xml_pattern_str_equal(step->value2, xml_pattern_node_ns_href(node)))
			goto rollback;
		}
		continue;
            case XML_PAT_OP_CHILD: {
		XMQNode *lst;

		if ((xml_node_kind(node) != XML_DOM_ELEMENT) &&
		    (xml_node_kind(node) != XML_DOM_DOCUMENT))
		    goto rollback;

		lst = xml_node_first_child(node);

		if (step->value != NULL) {
		    while (lst != NULL) {
			if ((xml_node_kind(lst) == XML_DOM_ELEMENT) &&
			    (step->value[0] == xml_node_name(lst)[0]) &&
			    (xml_pattern_str_equal(step->value, xml_node_name(lst))))
			    break;
			lst = xml_node_next(lst);
		    }
		    if (lst != NULL)
			continue;
		}
		goto rollback;
	    }
            case XML_PAT_OP_ATTR:
		if (xml_node_kind(node) != XML_DOM_ATTRIBUTE)
		    goto rollback;
		if (step->value != NULL) {
		    if (step->value[0] != xml_node_name(node)[0])
			goto rollback;
		    if (!xml_pattern_str_equal(step->value, xml_node_name(node)))
			goto rollback;
		}
		/* Namespace test */
		if (xml_node_ns(node) == NULL) {
		    if (step->value2 != NULL)
			goto rollback;
		} else if (step->value2 != NULL) {
		    if (!xml_pattern_str_equal(step->value2, xml_pattern_node_ns_href(node)))
			goto rollback;
		}
		continue;
            case XML_PAT_OP_PARENT:
		if ((xml_node_kind(node) == XML_DOM_DOCUMENT) ||
		    (xml_node_kind(node) == XML_DOM_NAMESPACE))
		    goto rollback;
		node = xml_node_parent(node);
		if (node == NULL)
		    goto rollback;
		if (step->value == NULL)
		    continue;
		if (step->value[0] != xml_node_name(node)[0])
		    goto rollback;
		if (!xml_pattern_str_equal(step->value, xml_node_name(node)))
		    goto rollback;
		/* Namespace test */
		if (xml_node_ns(node) == NULL) {
		    if (step->value2 != NULL)
			goto rollback;
		} else if (xml_pattern_node_ns_href(node) != NULL) {
		    if (step->value2 == NULL)
			goto rollback;
		    if (!xml_pattern_str_equal(step->value2, xml_pattern_node_ns_href(node)))
			goto rollback;
		}
		continue;
            case XML_PAT_OP_ANCESTOR:
		/* TODO: implement coalescing of ANCESTOR/NODE ops */
		if (step->value == NULL) {
		    i++;
		    step = &comp->steps[i];
		    if (step->op == XML_PAT_OP_ROOT)
			goto found;
		    if (step->op != XML_PAT_OP_ELEM)
			goto rollback;
		    if (step->value == NULL) {
			/* libxml2 leaks the rollback states here. */
			free(states.states);
			return(-1);
		    }
		}
		if (node == NULL)
		    goto rollback;
		if ((xml_node_kind(node) == XML_DOM_DOCUMENT) ||
		    (xml_node_kind(node) == XML_DOM_NAMESPACE))
		    goto rollback;
		node = xml_node_parent(node);
		while (node != NULL) {
		    if ((xml_node_kind(node) == XML_DOM_ELEMENT) &&
			(step->value[0] == xml_node_name(node)[0]) &&
			(xml_pattern_str_equal(step->value, xml_node_name(node)))) {
			/* Namespace test */
			if (xml_node_ns(node) == NULL) {
			    if (step->value2 == NULL)
				break;
			} else if (xml_pattern_node_ns_href(node) != NULL) {
			    if ((step->value2 != NULL) &&
			        (xml_pattern_str_equal(step->value2, xml_pattern_node_ns_href(node))))
				break;
			}
		    }
		    node = xml_node_parent(node);
		}
		if (node == NULL)
		    goto rollback;
		/*
		 * prepare a potential rollback from here
		 * for ancestors of that node.
		 */
		if (step->op == XML_PAT_OP_ANCESTOR)
		    xml_pattern_push_state(&states, i, node);
		else
		    xml_pattern_push_state(&states, i - 1, node);
		continue;
            case XML_PAT_OP_NS:
		if (xml_node_kind(node) != XML_DOM_ELEMENT)
		    goto rollback;
		if (xml_node_ns(node) == NULL) {
		    if (step->value != NULL)
			goto rollback;
		} else if (xml_pattern_node_ns_href(node) != NULL) {
		    if (step->value == NULL)
			goto rollback;
		    if (!xml_pattern_str_equal(step->value, xml_pattern_node_ns_href(node)))
			goto rollback;
		}
		break;
            case XML_PAT_OP_ALL:
		if (xml_node_kind(node) != XML_DOM_ELEMENT)
		    goto rollback;
		break;
	}
    }
found:
    if (states.states != NULL) {
        /* Free the rollback states */
	free(states.states);
    }
    return(1);
rollback:
    /* got an error try to rollback */
    if (states.states == NULL)
	return(0);
    if (states.nbstates <= 0) {
	free(states.states);
	return(0);
    }
    states.nbstates--;
    i = states.states[states.nbstates].step;
    node = states.states[states.nbstates].node;
    goto restart;
}

/************************************************************************
 *									*
 *			Dedicated parser for templates			*
 *									*
 ************************************************************************/

#define CUR (*(const unsigned char*)ctxt->cur)
#define SKIP(val) ctxt->cur += (val)
#define NXT(val) ctxt->cur[(val)]
#define PEEKPREV(val) ((const unsigned char*)ctxt->cur)[-(val)]
#define CUR_PTR ctxt->cur

#define SKIP_BLANKS							\
    while (XML_PAT_IS_BLANK_CH(CUR)) NEXT

#define CURRENT (*ctxt->cur)
#define NEXT ((*ctxt->cur) ?  ctxt->cur++: ctxt->cur)


#define PUSH(op, val, val2)						\
    if (xml_pattern_add(ctxt, ctxt->comp, (op), (val), (val2))) goto error;

/**
 * Port of xmlPatScanName.
 *
 * [4] NameChar ::= Letter | Digit | '.' | '-' | '_' |
 *                  CombiningChar | Extender
 *
 * [5] Name ::= (Letter | '_' | ':') (NameChar)*
 *
 * [6] Names ::= Name (S Name)*
 *
 * @param ctxt  the XPath Parser context
 * @returns the Name parsed or NULL
 */

static char *
xml_pattern_scan_name(XmlPatParserContext *ctxt) {
    const char *q, *cur;
    char *ret = NULL;

    SKIP_BLANKS;

    q = CUR_PTR;
    cur = xml_pattern_scan_name_end(q, XML_PAT_MAX_NAME_LENGTH, 0);
    if ((cur == NULL) || (cur == q))
        return(NULL);
    ret = xml_pattern_strndup(q, cur - q);
    CUR_PTR = cur;
    return(ret);
}

/**
 * Port of xmlPatScanNCName.
 * Parses a non qualified name
 *
 * @param ctxt  the XPath Parser context
 * @returns the Name parsed or NULL
 */

static char *
xml_pattern_scan_ncname(XmlPatParserContext *ctxt) {
    const char *q, *cur;
    char *ret = NULL;

    SKIP_BLANKS;

    q = CUR_PTR;
    cur = xml_pattern_scan_name_end(q, XML_PAT_MAX_NAME_LENGTH, 1);
    if ((cur == NULL) || (cur == q))
        return(NULL);
    ret = xml_pattern_strndup(q, cur - q);
    if (ret == NULL)
        ctxt->error = -1;
    CUR_PTR = cur;
    return(ret);
}

/**
 * Port of xmlCompileAttributeTest.
 * Compile an attribute test.
 *
 * @param ctxt  the compilation context
 */
static void
xml_pattern_compile_attribute_test(XmlPatParserContext *ctxt) {
    char *token = NULL;
    char *name = NULL;
    char *URL = NULL;

    SKIP_BLANKS;
    name = xml_pattern_scan_ncname(ctxt);
    if (ctxt->error < 0)
        return;
    if (name == NULL) {
	if (CUR == '*') {
	    PUSH(XML_PAT_OP_ATTR, NULL, NULL);
	    NEXT;
	} else {
	    ERROR(NULL, NULL, NULL,
		"xmlCompileAttributeTest : Name expected\n");
	    ctxt->error = 1;
	}
	return;
    }
    if (CUR == ':') {
	int i;
	char *prefix = name;

	NEXT;

	if (XML_PAT_IS_BLANK_CH(CUR)) {
	    ERROR5(NULL, NULL, NULL, "Invalid QName.\n", NULL);
	    ctxt->error = 1;
	    goto error;
	}
	/*
	* This is a namespace match
	*/
	token = xml_pattern_scan_name(ctxt);
	if ((prefix[0] == 'x') &&
	    (prefix[1] == 'm') &&
	    (prefix[2] == 'l') &&
	    (prefix[3] == 0))
	{
	    XML_PAT_COPY_NSNAME(ctxt, URL, XML_PAT_XML_NAMESPACE);
	} else {
	    for (i = 0;i < ctxt->nb_namespaces;i++) {
		if (xml_pattern_str_equal(ctxt->namespaces[2 * i + 1], prefix)) {
		    XML_PAT_COPY_NSNAME(ctxt, URL, ctxt->namespaces[2 * i])
		    break;
		}
	    }
	    if (i >= ctxt->nb_namespaces) {
		ERROR5(NULL, NULL, NULL,
		    "xmlCompileAttributeTest : no namespace bound to prefix %s\n",
		    prefix);
		ctxt->error = 1;
		goto error;
	    }
	}
        XML_PAT_FREE_STRING(ctxt, name);
        name = NULL;
	if (token == NULL) {
	    if (CUR == '*') {
		NEXT;
		PUSH(XML_PAT_OP_ATTR, NULL, URL);
	    } else {
		ERROR(NULL, NULL, NULL,
		    "xmlCompileAttributeTest : Name expected\n");
		ctxt->error = 1;
		goto error;
	    }
	} else {
	    PUSH(XML_PAT_OP_ATTR, token, URL);
	}
    } else {
	PUSH(XML_PAT_OP_ATTR, name, NULL);
    }
    return;
error:
    if (name != NULL)
	XML_PAT_FREE_STRING(ctxt, name);
    if (URL != NULL)
	XML_PAT_FREE_STRING(ctxt, URL)
    if (token != NULL)
	XML_PAT_FREE_STRING(ctxt, token);
}

/**
 * Port of xmlCompileStepPattern.
 * Compile the Step Pattern and generates a precompiled
 * form suitable for fast matching.
 *
 * [3]    Step    ::=    '.' | NameTest
 * [4]    NameTest    ::=    QName | '*' | NCName ':' '*'
 *
 * @param ctxt  the compilation context
 */

static void
xml_pattern_compile_step_pattern(XmlPatParserContext *ctxt) {
    char *token = NULL;
    char *name = NULL;
    char *URL = NULL;
    int hasBlanks = 0;

    SKIP_BLANKS;
    if (CUR == '.') {
	/*
	* Context node.
	*/
	NEXT;
	PUSH(XML_PAT_OP_ELEM, NULL, NULL);
	return;
    }
    if (CUR == '@') {
	/*
	* Attribute test.
	*/
	if (XML_STREAM_XS_IDC_SEL(ctxt->comp)) {
	    ERROR5(NULL, NULL, NULL,
		"Unexpected attribute axis in '%s'.\n", ctxt->base);
	    ctxt->error = 1;
	    return;
	}
	NEXT;
	xml_pattern_compile_attribute_test(ctxt);
	if (ctxt->error != 0)
	    goto error;
	return;
    }
    name = xml_pattern_scan_ncname(ctxt);
    if (ctxt->error < 0)
        return;
    if (name == NULL) {
	if (CUR == '*') {
	    NEXT;
	    PUSH(XML_PAT_OP_ALL, NULL, NULL);
	    return;
	} else {
	    ERROR(NULL, NULL, NULL,
		    "xmlCompileStepPattern : Name expected\n");
	    ctxt->error = 1;
	    return;
	}
    }
    if (XML_PAT_IS_BLANK_CH(CUR)) {
	hasBlanks = 1;
	SKIP_BLANKS;
    }
    if (CUR == ':') {
	NEXT;
	if (CUR != ':') {
	    char *prefix = name;
	    int i;

	    if (hasBlanks || XML_PAT_IS_BLANK_CH(CUR)) {
		ERROR5(NULL, NULL, NULL, "Invalid QName.\n", NULL);
		ctxt->error = 1;
		goto error;
	    }
	    /*
	     * This is a namespace match
	     */
	    token = xml_pattern_scan_name(ctxt);
	    if ((prefix[0] == 'x') &&
		(prefix[1] == 'm') &&
		(prefix[2] == 'l') &&
		(prefix[3] == 0))
	    {
		XML_PAT_COPY_NSNAME(ctxt, URL, XML_PAT_XML_NAMESPACE)
	    } else {
		for (i = 0;i < ctxt->nb_namespaces;i++) {
		    if (xml_pattern_str_equal(ctxt->namespaces[2 * i + 1], prefix)) {
			XML_PAT_COPY_NSNAME(ctxt, URL, ctxt->namespaces[2 * i])
			break;
		    }
		}
		if (i >= ctxt->nb_namespaces) {
		    ERROR5(NULL, NULL, NULL,
			"xmlCompileStepPattern : no namespace bound to prefix %s\n",
			prefix);
		    ctxt->error = 1;
		    goto error;
		}
	    }
	    XML_PAT_FREE_STRING(ctxt, prefix);
	    name = NULL;
	    if (token == NULL) {
		if (CUR == '*') {
		    NEXT;
		    PUSH(XML_PAT_OP_NS, URL, NULL);
		} else {
		    ERROR(NULL, NULL, NULL,
			    "xmlCompileStepPattern : Name expected\n");
		    ctxt->error = 1;
		    goto error;
		}
	    } else {
		PUSH(XML_PAT_OP_ELEM, token, URL);
	    }
	} else {
	    NEXT;
	    if (xml_pattern_str_equal(name, "child")) {
		XML_PAT_FREE_STRING(ctxt, name);
		name = xml_pattern_scan_name(ctxt);
		if (name == NULL) {
		    if (CUR == '*') {
			NEXT;
			PUSH(XML_PAT_OP_ALL, NULL, NULL);
			return;
		    } else {
			ERROR(NULL, NULL, NULL,
			    "xmlCompileStepPattern : QName expected\n");
			ctxt->error = 1;
			goto error;
		    }
		}
		if (CUR == ':') {
		    char *prefix = name;
		    int i;

		    NEXT;
		    if (XML_PAT_IS_BLANK_CH(CUR)) {
			ERROR5(NULL, NULL, NULL, "Invalid QName.\n", NULL);
			ctxt->error = 1;
			goto error;
		    }
		    /*
		    * This is a namespace match
		    */
		    token = xml_pattern_scan_name(ctxt);
		    if ((prefix[0] == 'x') &&
			(prefix[1] == 'm') &&
			(prefix[2] == 'l') &&
			(prefix[3] == 0))
		    {
			XML_PAT_COPY_NSNAME(ctxt, URL, XML_PAT_XML_NAMESPACE)
		    } else {
			for (i = 0;i < ctxt->nb_namespaces;i++) {
			    if (xml_pattern_str_equal(ctxt->namespaces[2 * i + 1], prefix)) {
				XML_PAT_COPY_NSNAME(ctxt, URL, ctxt->namespaces[2 * i])
				break;
			    }
			}
			if (i >= ctxt->nb_namespaces) {
			    ERROR5(NULL, NULL, NULL,
				"xmlCompileStepPattern : no namespace bound "
				"to prefix %s\n", prefix);
			    ctxt->error = 1;
			    goto error;
			}
		    }
		    XML_PAT_FREE_STRING(ctxt, prefix);
		    name = NULL;
		    if (token == NULL) {
			if (CUR == '*') {
			    NEXT;
			    PUSH(XML_PAT_OP_NS, URL, NULL);
			} else {
			    ERROR(NULL, NULL, NULL,
				"xmlCompileStepPattern : Name expected\n");
			    ctxt->error = 1;
			    goto error;
			}
		    } else {
			PUSH(XML_PAT_OP_ELEM, token, URL);
		    }
		} else
		    PUSH(XML_PAT_OP_ELEM, name, NULL);
		return;
	    } else if (xml_pattern_str_equal(name, "attribute")) {
		XML_PAT_FREE_STRING(ctxt, name)
		name = NULL;
		if (XML_STREAM_XS_IDC_SEL(ctxt->comp)) {
		    ERROR5(NULL, NULL, NULL,
			"Unexpected attribute axis in '%s'.\n", ctxt->base);
		    ctxt->error = 1;
		    goto error;
		}
		xml_pattern_compile_attribute_test(ctxt);
		if (ctxt->error != 0)
		    goto error;
		return;
	    } else {
		ERROR5(NULL, NULL, NULL,
		    "The 'element' or 'attribute' axis is expected.\n", NULL);
		ctxt->error = 1;
		goto error;
	    }
	}
    } else if (CUR == '*') {
        if (name != NULL) {
	    ctxt->error = 1;
	    goto error;
	}
	NEXT;
	PUSH(XML_PAT_OP_ALL, token, NULL);
    } else {
	PUSH(XML_PAT_OP_ELEM, name, NULL);
    }
    return;
error:
    if (URL != NULL)
	XML_PAT_FREE_STRING(ctxt, URL)
    if (token != NULL)
	XML_PAT_FREE_STRING(ctxt, token)
    if (name != NULL)
	XML_PAT_FREE_STRING(ctxt, name)
}

/**
 * Port of xmlCompilePathPattern.
 * Compile the Path Pattern and generates a precompiled
 * form suitable for fast matching.
 *
 * [5]    Path    ::=    ('.//')? ( Step '/' )* ( Step | '@' NameTest )
 *
 * @param ctxt  the compilation context
 */
static void
xml_pattern_compile_path_pattern(XmlPatParserContext *ctxt) {
    SKIP_BLANKS;
    if (CUR == '/') {
        ctxt->comp->flags |= PAT_FROM_ROOT;
    } else if ((CUR == '.') || (ctxt->comp->flags & XML_PATTERN_NOTPATTERN)) {
        ctxt->comp->flags |= PAT_FROM_CUR;
    }

    if ((CUR == '/') && (NXT(1) == '/')) {
	PUSH(XML_PAT_OP_ANCESTOR, NULL, NULL);
	NEXT;
	NEXT;
    } else if ((CUR == '.') && (NXT(1) == '/') && (NXT(2) == '/')) {
	PUSH(XML_PAT_OP_ANCESTOR, NULL, NULL);
	NEXT;
	NEXT;
	NEXT;
	/* Check for incompleteness. */
	SKIP_BLANKS;
	if (CUR == 0) {
	    ERROR5(NULL, NULL, NULL,
	       "Incomplete expression '%s'.\n", ctxt->base);
	    ctxt->error = 1;
	    goto error;
	}
    }
    if (CUR == '@') {
	NEXT;
	xml_pattern_compile_attribute_test(ctxt);
        if (ctxt->error != 0)
            goto error;
	SKIP_BLANKS;
	/* TODO: check for incompleteness */
	if (CUR != 0) {
	    xml_pattern_compile_step_pattern(ctxt);
	    if (ctxt->error != 0)
		goto error;
	}
    } else {
        if (CUR == '/') {
	    PUSH(XML_PAT_OP_ROOT, NULL, NULL);
	    NEXT;
	    /* Check for incompleteness. */
	    SKIP_BLANKS;
	    if (CUR == 0) {
		ERROR5(NULL, NULL, NULL,
		    "Incomplete expression '%s'.\n", ctxt->base);
		ctxt->error = 1;
		goto error;
	    }
	}
	xml_pattern_compile_step_pattern(ctxt);
	if (ctxt->error != 0)
	    goto error;
	SKIP_BLANKS;
	while (CUR == '/') {
	    if (NXT(1) == '/') {
	        PUSH(XML_PAT_OP_ANCESTOR, NULL, NULL);
		NEXT;
		NEXT;
		SKIP_BLANKS;
		xml_pattern_compile_step_pattern(ctxt);
		if (ctxt->error != 0)
		    goto error;
	    } else {
	        PUSH(XML_PAT_OP_PARENT, NULL, NULL);
		NEXT;
		SKIP_BLANKS;
		if (CUR == 0) {
		    ERROR5(NULL, NULL, NULL,
		    "Incomplete expression '%s'.\n", ctxt->base);
		    ctxt->error = 1;
		    goto error;
		}
		xml_pattern_compile_step_pattern(ctxt);
		if (ctxt->error != 0)
		    goto error;
	    }
	}
    }
    if (CUR != 0) {
	ERROR5(NULL, NULL, NULL,
	       "Failed to compile pattern %s\n", ctxt->base);
	ctxt->error = 1;
    }
error:
    return;
}

/**
 * Port of xmlCompileIDCXPathPath.
 * Compile the Path Pattern and generates a precompiled
 * form suitable for fast matching.
 *
 * [5]    Path    ::=    ('.//')? ( Step '/' )* ( Step | '@' NameTest )
 *
 * @param ctxt  the compilation context
 */
static void
xml_pattern_compile_idc_xpath_path(XmlPatParserContext *ctxt) {
    SKIP_BLANKS;
    if (CUR == '/') {
	ERROR5(NULL, NULL, NULL,
	    "Unexpected selection of the document root in '%s'.\n",
	    ctxt->base);
	goto error;
    }
    ctxt->comp->flags |= PAT_FROM_CUR;

    if (CUR == '.') {
	/* "." - "self::node()" */
	NEXT;
	SKIP_BLANKS;
	if (CUR == 0) {
	    /*
	    * Selection of the context node.
	    */
	    PUSH(XML_PAT_OP_ELEM, NULL, NULL);
	    return;
	}
	if (CUR != '/') {
	    /* TODO: A more meaningful error message. */
	    ERROR5(NULL, NULL, NULL,
	    "Unexpected token after '.' in '%s'.\n", ctxt->base);
	    goto error;
	}
	/* "./" - "self::node()/" */
	NEXT;
	SKIP_BLANKS;
	if (CUR == '/') {
	    if (XML_PAT_IS_BLANK_CH(PEEKPREV(1))) {
		/*
		* Disallow "./ /"
		*/
		ERROR5(NULL, NULL, NULL,
		    "Unexpected '/' token in '%s'.\n", ctxt->base);
		goto error;
	    }
	    /* ".//" - "self:node()/descendant-or-self::node()/" */
	    PUSH(XML_PAT_OP_ANCESTOR, NULL, NULL);
	    NEXT;
	    SKIP_BLANKS;
	}
	if (CUR == 0)
	    goto error_unfinished;
    }
    /*
    * Process steps.
    */
    do {
	xml_pattern_compile_step_pattern(ctxt);
	if (ctxt->error != 0)
	    goto error;
	SKIP_BLANKS;
	if (CUR != '/')
	    break;
	PUSH(XML_PAT_OP_PARENT, NULL, NULL);
	NEXT;
	SKIP_BLANKS;
	if (CUR == '/') {
	    /*
	    * Disallow subsequent '//'.
	    */
	    ERROR5(NULL, NULL, NULL,
		"Unexpected subsequent '//' in '%s'.\n",
		ctxt->base);
	    goto error;
	}
	if (CUR == 0)
	    goto error_unfinished;

    } while (CUR != 0);

    if (CUR != 0) {
	ERROR5(NULL, NULL, NULL,
	    "Failed to compile expression '%s'.\n", ctxt->base);
	ctxt->error = 1;
    }
    return;
error:
    ctxt->error = 1;
    return;

error_unfinished:
    ctxt->error = 1;
    ERROR5(NULL, NULL, NULL,
	"Unfinished expression '%s'.\n", ctxt->base);
}

/************************************************************************
 *									*
 *			The streaming code				*
 *									*
 ************************************************************************/

/**
 * Port of xmlNewStreamComp.
 * build a new compiled pattern for streaming
 *
 * @param size  the number of expected steps
 * @returns the new structure or NULL in case of error.
 */
static XmlPatStreamComp *
xml_stream_comp_new(int size) {
    XmlPatStreamComp *cur;

    if (size < 4)
        size  = 4;

    cur = (XmlPatStreamComp *) malloc(sizeof(XmlPatStreamComp));
    if (cur == NULL) {
	ERROR(NULL, NULL, NULL,
		"xmlNewStreamComp: malloc failed\n");
	return(NULL);
    }
    memset(cur, 0, sizeof(XmlPatStreamComp));
    cur->steps = (XmlPatStreamStep *) malloc(size * sizeof(XmlPatStreamStep));
    if (cur->steps == NULL) {
	free(cur);
	ERROR(NULL, NULL, NULL,
	      "xmlNewStreamComp: malloc failed\n");
	return(NULL);
    }
    cur->nbStep = 0;
    cur->maxStep = size;
    return(cur);
}

/**
 * Port of xmlFreeStreamComp.
 * Free the compiled pattern for streaming
 *
 * @param comp  the compiled pattern for streaming
 */
static void
xml_stream_comp_free(XmlPatStreamComp *comp) {
    if (comp != NULL) {
        if (comp->steps != NULL)
	    free(comp->steps);
        free(comp);
    }
}

/**
 * Port of xmlStreamCompAddStep.
 * Add a new step to the compiled pattern
 *
 * @param comp  the compiled pattern for streaming
 * @param name  the first string, the name, or NULL for *
 * @param ns  the second step, the namespace name
 * @param nodeType  the node type
 * @param flags  the flags for that step
 * @returns -1 in case of error or the step index if successful
 */
static int
xml_stream_comp_add_step(XmlPatStreamComp *comp, const char *name,
                     const char *ns, int nodeType, int flags) {
    XmlPatStreamStep *cur;

    if (comp->nbStep >= comp->maxStep) {
        XmlPatStreamStep *tmp;
        int newSize;

        newSize = xml_pattern_grow_capacity(comp->maxStep, sizeof(tmp[0]),
                                  4, XML_PAT_MAX_ITEMS);
        if (newSize < 0) {
	    ERROR(NULL, NULL, NULL,
		  "xmlNewStreamComp: growCapacity failed\n");
	    return(-1);
        }
	cur = (XmlPatStreamStep *) realloc(comp->steps, newSize * sizeof(tmp[0]));
	if (cur == NULL) {
	    ERROR(NULL, NULL, NULL,
		  "xmlNewStreamComp: malloc failed\n");
	    return(-1);
	}
	comp->steps = cur;
        comp->maxStep = newSize;
    }
    cur = &comp->steps[comp->nbStep++];
    cur->flags = flags;
    cur->name = name;
    cur->ns = ns;
    cur->nodeType = nodeType;
    return(comp->nbStep - 1);
}

/**
 * Port of xmlStreamCompile.
 * Tries to stream compile a pattern
 *
 * @param comp  the precompiled pattern
 * @returns -1 in case of failure and 0 in case of success.
 */
static int
xml_stream_compile(XmlPattern *comp) {
    XmlPatStreamComp *stream;
    int i, s = 0, root = 0, flags = 0, prevs = -1;
    XmlPatStepOp step;

    if ((comp == NULL) || (comp->steps == NULL))
        return(-1);
    /*
     * special case for .
     */
    if ((comp->nbStep == 1) &&
        (comp->steps[0].op == XML_PAT_OP_ELEM) &&
	(comp->steps[0].value == NULL) &&
	(comp->steps[0].value2 == NULL)) {
	stream = xml_stream_comp_new(0);
	if (stream == NULL)
	    return(-1);
	/* Note that the stream will have no steps in this case. */
	stream->flags |= XML_STREAM_FINAL_IS_ANY_NODE;
	comp->stream = stream;
	return(0);
    }

    stream = xml_stream_comp_new((comp->nbStep / 2) + 1);
    if (stream == NULL)
        return(-1);

    i = 0;
    if (comp->flags & PAT_FROM_ROOT)
	stream->flags |= XML_STREAM_FROM_ROOT;

    for (;i < comp->nbStep;i++) {
	step = comp->steps[i];
        switch (step.op) {
	    case XML_PAT_OP_END:
	        break;
	    case XML_PAT_OP_ROOT:
	        if (i != 0)
		    goto error;
		root = 1;
		break;
	    case XML_PAT_OP_NS:
		s = xml_stream_comp_add_step(stream, NULL, step.value,
		    XML_PAT_ELEMENT_NODE, flags);
		if (s < 0)
		    goto error;
		prevs = s;
		flags = 0;
		break;
	    case XML_PAT_OP_ATTR:
		flags |= XML_STREAM_STEP_ATTR;
		prevs = -1;
		s = xml_stream_comp_add_step(stream,
		    step.value, step.value2, XML_PAT_ATTRIBUTE_NODE, flags);
		flags = 0;
		if (s < 0)
		    goto error;
		break;
	    case XML_PAT_OP_ELEM:
	        if ((step.value == NULL) && (step.value2 == NULL)) {
		    /*
		    * We have a "." or "self::node()" here.
		    * Eliminate redundant self::node() tests like in "/./."
		    * or "//./"
		    * The only case we won't eliminate is "//.", i.e. if
		    * self::node() is the last node test and we had
		    * continuation somewhere beforehand.
		    */
		    if ((comp->nbStep == i + 1) &&
			(flags & XML_STREAM_STEP_DESC)) {
			/*
			* Mark the special case where the expression resolves
			* to any type of node.
			*/
			if (comp->nbStep == i + 1) {
			    stream->flags |= XML_STREAM_FINAL_IS_ANY_NODE;
			}
			flags |= XML_STREAM_STEP_NODE;
			s = xml_stream_comp_add_step(stream, NULL, NULL,
			    XML_STREAM_ANY_NODE, flags);
			if (s < 0)
			    goto error;
			flags = 0;
			/*
			* If there was a previous step, mark it to be added to
			* the result node-set; this is needed since only
			* the last step will be marked as "final" and only
			* "final" nodes are added to the resulting set.
			*/
			if (prevs != -1) {
			    stream->steps[prevs].flags |= XML_STREAM_STEP_IN_SET;
			    prevs = -1;
			}
			break;

		    } else {
			/* Just skip this one. */
			continue;
		    }
		}
		/* An element node. */
	        s = xml_stream_comp_add_step(stream, step.value, step.value2,
		    XML_PAT_ELEMENT_NODE, flags);
		if (s < 0)
		    goto error;
		prevs = s;
		flags = 0;
		break;
	    case XML_PAT_OP_CHILD:
		/* An element node child. */
	        s = xml_stream_comp_add_step(stream, step.value, step.value2,
		    XML_PAT_ELEMENT_NODE, flags);
		if (s < 0)
		    goto error;
		prevs = s;
		flags = 0;
		break;
	    case XML_PAT_OP_ALL:
	        s = xml_stream_comp_add_step(stream, NULL, NULL,
		    XML_PAT_ELEMENT_NODE, flags);
		if (s < 0)
		    goto error;
		prevs = s;
		flags = 0;
		break;
	    case XML_PAT_OP_PARENT:
	        break;
	    case XML_PAT_OP_ANCESTOR:
		/* Skip redundant continuations. */
		if (flags & XML_STREAM_STEP_DESC)
		    break;
	        flags |= XML_STREAM_STEP_DESC;
		/*
		* Mark the expression as having "//".
		*/
		if ((stream->flags & XML_STREAM_DESC) == 0)
		    stream->flags |= XML_STREAM_DESC;
		break;
	}
    }
    if ((! root) && (comp->flags & XML_PATTERN_NOTPATTERN) == 0) {
	/*
	* If this should behave like a real pattern, we will mark
	* the first step as having "//", to be reentrant on every
	* tree level.
	*/
	if ((stream->flags & XML_STREAM_DESC) == 0)
	    stream->flags |= XML_STREAM_DESC;

	if (stream->nbStep > 0) {
	    if ((stream->steps[0].flags & XML_STREAM_STEP_DESC) == 0)
		stream->steps[0].flags |= XML_STREAM_STEP_DESC;
	}
    }
    if (stream->nbStep <= s)
	goto error;
    stream->steps[s].flags |= XML_STREAM_STEP_FINAL;
    if (root)
	stream->steps[0].flags |= XML_STREAM_STEP_ROOT;
    comp->stream = stream;
    return(0);
error:
    xml_stream_comp_free(stream);
    return(0);
}

/**
 * Port of xmlNewStreamCtxt.
 * build a new stream context
 *
 * @param stream  the copmiled stream
 * @returns the new structure or NULL in case of error.
 */
static XmlStreamCtxt *
xml_stream_ctxt_new(XmlPatStreamComp *stream) {
    XmlStreamCtxt *cur;

    cur = (XmlStreamCtxt *) malloc(sizeof(XmlStreamCtxt));
    if (cur == NULL) {
	ERROR(NULL, NULL, NULL,
		"xmlNewStreamCtxt: malloc failed\n");
	return(NULL);
    }
    memset(cur, 0, sizeof(XmlStreamCtxt));
    cur->states = NULL;
    cur->nbState = 0;
    cur->maxState = 0;
    cur->level = 0;
    cur->comp = stream;
    cur->blockLevel = -1;
    return(cur);
}

/**
 * Port of xmlFreeStreamCtxt.
 * Free the stream context
 *
 * @param stream  the stream context
 */
void
xml_stream_ctxt_free(XmlStreamCtxt *stream) {
    XmlStreamCtxt *next;

    while (stream != NULL) {
        next = stream->next;
        if (stream->states != NULL)
	    free(stream->states);
        free(stream);
	stream = next;
    }
}

/**
 * Port of xmlStreamCtxtAddState.
 * Add a new state to the stream context
 *
 * @param comp  the stream context
 * @param idx  the step index for that streaming state
 * @param level  the level
 * @returns -1 in case of error or the state index if successful
 */
static int
xml_stream_ctxt_add_state(XmlStreamCtxt *comp, int idx, int level) {
    int i;
    for (i = 0;i < comp->nbState;i++) {
        if (comp->states[2 * i] < 0) {
	    comp->states[2 * i] = idx;
	    comp->states[2 * i + 1] = level;
	    return(i);
	}
    }
    if (comp->nbState >= comp->maxState) {
        int *tmp;
        int newSize;

        newSize = xml_pattern_grow_capacity(comp->maxState, sizeof(tmp[0]) * 2,
                                  4, XML_PAT_MAX_ITEMS);
        if (newSize < 0) {
	    ERROR(NULL, NULL, NULL,
		  "xmlNewStreamCtxt: growCapacity failed\n");
	    return(-1);
        }
	tmp = (int *) realloc(comp->states, newSize * sizeof(tmp[0]) * 2);
	if (tmp == NULL) {
	    ERROR(NULL, NULL, NULL,
		  "xmlNewStreamCtxt: malloc failed\n");
	    return(-1);
	}
	comp->states = tmp;
        comp->maxState = newSize;
    }
    comp->states[2 * comp->nbState] = idx;
    comp->states[2 * comp->nbState++ + 1] = level;
    return(comp->nbState - 1);
}

/**
 * Port of xmlStreamPushInternal.
 * Push new data onto the stream.
 * Both `name` and `ns` being NULL means the / i.e. the root of the document.
 * This can also act as a reset.
 *
 * @param stream  the stream context
 * @param name  the current name
 * @param ns  the namespace name
 * @param nodeType  the type of the node
 * @returns -1 in case of error, 1 if the current state in the stream is a
 *    match and 0 otherwise.
 */
static int
xml_stream_push_internal(XmlStreamCtxt *stream,
		      const char *name, const char *ns,
		      int nodeType) {
    int ret = 0, final = 0, tmp, i, m, match, stepNr, desc;
    XmlPatStreamComp *comp;
    XmlPatStreamStep step;

    if ((stream == NULL) || (stream->nbState < 0))
        return(-1);

    while (stream != NULL) {
	comp = stream->comp;

	if ((nodeType == XML_PAT_ELEMENT_NODE) &&
	    (name == NULL) && (ns == NULL)) {
	    /* We have a document node here (or a reset). */
	    stream->nbState = 0;
	    stream->level = 0;
	    stream->blockLevel = -1;
	    if (comp->flags & XML_STREAM_FROM_ROOT) {
		if (comp->nbStep == 0) {
		    /* TODO: We have a "/." here? */
		    ret = 1;
		} else {
		    if ((comp->nbStep == 1) &&
			(comp->steps[0].nodeType == XML_STREAM_ANY_NODE) &&
			(comp->steps[0].flags & XML_STREAM_STEP_DESC))
		    {
			/*
			* In the case of "//." the document node will match
			* as well.
			*/
			ret = 1;
		    } else if (comp->steps[0].flags & XML_STREAM_STEP_ROOT) {
			if (xml_stream_ctxt_add_state(stream, 0, 0) < 0)
                            return(-1);
		    }
		}
	    }
	    stream = stream->next;
	    continue; /* while */
	}

	/*
	* Fast check for ".".
	*/
	if (comp->nbStep == 0) {
	    /*
	     * / and . are handled at the XPath node set creation
	     * level by checking min depth
	     */
	    if (stream->flags & XML_PATTERN_FLAG_XPATH) {
		stream = stream->next;
		continue; /* while */
	    }
	    /*
	    * For non-pattern like evaluation like XML Schema IDCs
	    * or traditional XPath expressions, this will match if
	    * we are at the first level only, otherwise on every level.
	    */
	    if ((nodeType != XML_PAT_ATTRIBUTE_NODE) &&
		(((stream->flags & XML_PATTERN_NOTPATTERN) == 0) ||
		(stream->level == 0))) {
		    ret = 1;
	    }
	    stream->level++;
	    goto stream_next;
	}
	if (stream->blockLevel != -1) {
	    /*
	    * Skip blocked expressions.
	    */
	    stream->level++;
	    goto stream_next;
	}

	if ((nodeType != XML_PAT_ELEMENT_NODE) &&
	    (nodeType != XML_PAT_ATTRIBUTE_NODE) &&
	    ((comp->flags & XML_STREAM_FINAL_IS_ANY_NODE) == 0)) {
	    /*
	    * No need to process nodes of other types if we don't
	    * resolve to those types.
	    * TODO: Do we need to block the context here?
	    */
	    stream->level++;
	    goto stream_next;
	}

	/*
	 * Check evolution of existing states
	 */
	i = 0;
	m = stream->nbState;
	while (i < m) {
	    if ((comp->flags & XML_STREAM_DESC) == 0) {
		/*
		* If there is no "//", then only the last
		* added state is of interest.
		*/
		stepNr = stream->states[2 * (stream->nbState -1)];
		/*
		* TODO: Security check, should not happen, remove it.
		*/
		if (stream->states[(2 * (stream->nbState -1)) + 1] <
		    stream->level) {
		    return (-1);
		}
		desc = 0;
		/* loop-stopper */
		i = m;
	    } else {
		/*
		* If there are "//", then we need to process every "//"
		* occurring in the states, plus any other state for this
		* level.
		*/
		stepNr = stream->states[2 * i];

		/* TODO: should not happen anymore: dead states */
		if (stepNr < 0)
		    goto next_state;

		tmp = stream->states[(2 * i) + 1];

		/* skip new states just added */
		if (tmp > stream->level)
		    goto next_state;

		/* skip states at ancestor levels, except if "//" */
		desc = comp->steps[stepNr].flags & XML_STREAM_STEP_DESC;
		if ((tmp < stream->level) && (!desc))
		    goto next_state;
	    }
	    /*
	    * Check for correct node-type.
	    */
	    step = comp->steps[stepNr];
	    if (step.nodeType != nodeType) {
		if (step.nodeType == XML_PAT_ATTRIBUTE_NODE) {
		    /*
		    * Block this expression for deeper evaluation.
		    */
		    if ((comp->flags & XML_STREAM_DESC) == 0)
			stream->blockLevel = stream->level +1;
		    goto next_state;
		} else if (step.nodeType != XML_STREAM_ANY_NODE)
		    goto next_state;
	    }
	    /*
	    * Compare local/namespace-name.
	    */
	    match = 0;
	    if (step.nodeType == XML_STREAM_ANY_NODE) {
		match = 1;
	    } else if (step.name == NULL) {
		if (step.ns == NULL) {
		    /*
		    * This lets through all elements/attributes.
		    */
		    match = 1;
		} else if (ns != NULL)
		    match = xml_pattern_str_equal(step.ns, ns);
	    } else if (((step.ns != NULL) == (ns != NULL)) &&
		(name != NULL) &&
		(step.name[0] == name[0]) &&
		xml_pattern_str_equal(step.name, name) &&
		((step.ns == ns) || xml_pattern_str_equal(step.ns, ns)))
	    {
		match = 1;
	    }
	    if (match) {
		final = step.flags & XML_STREAM_STEP_FINAL;
                if (final) {
                    ret = 1;
                } else if (xml_stream_ctxt_add_state(stream, stepNr + 1,
                                                 stream->level + 1) < 0) {
                    return(-1);
                }
		if ((ret != 1) && (step.flags & XML_STREAM_STEP_IN_SET)) {
		    /*
		    * Check if we have a special case like "foo/bar//.", where
		    * "foo" is selected as well.
		    */
		    ret = 1;
		}
	    }
	    if (((comp->flags & XML_STREAM_DESC) == 0) &&
		((! match) || final))  {
		/*
		* Mark this expression as blocked for any evaluation at
		* deeper levels. Note that this includes "/foo"
		* expressions if the *pattern* behaviour is used.
		*/
		stream->blockLevel = stream->level +1;
	    }
next_state:
	    i++;
	}

	stream->level++;

	/*
	* Re/enter the expression.
	* Don't reenter if it's an absolute expression like "/foo",
	*   except "//foo".
	*/
	step = comp->steps[0];
	if (step.flags & XML_STREAM_STEP_ROOT)
	    goto stream_next;

	desc = step.flags & XML_STREAM_STEP_DESC;
	if (stream->flags & XML_PATTERN_NOTPATTERN) {
	    /*
	    * Re/enter the expression if it is a "descendant" one,
	    * or if we are at the 1st level of evaluation.
	    */

	    if (stream->level == 1) {
		if (XML_STREAM_XS_IDC(stream)) {
		    /*
		    * XS-IDC: The missing "self::node()" will always
		    * match the first given node.
		    */
		    goto stream_next;
		} else
		    goto compare;
	    }
	    /*
	    * A "//" is always reentrant.
	    */
	    if (desc)
		goto compare;

	    /*
	    * XS-IDC: Process the 2nd level, since the missing
	    * "self::node()" is responsible for the 2nd level being
	    * the real start level.
	    */
	    if ((stream->level == 2) && XML_STREAM_XS_IDC(stream))
		goto compare;

	    goto stream_next;
	}

compare:
	/*
	* Check expected node-type.
	*/
	if (step.nodeType != nodeType) {
	    if (nodeType == XML_PAT_ATTRIBUTE_NODE)
		goto stream_next;
	    else if (step.nodeType != XML_STREAM_ANY_NODE)
		goto stream_next;
	}
	/*
	* Compare local/namespace-name.
	*/
	match = 0;
	if (step.nodeType == XML_STREAM_ANY_NODE) {
	    match = 1;
	} else if (step.name == NULL) {
	    if (step.ns == NULL) {
		/*
		* This lets through all elements/attributes.
		*/
		match = 1;
	    } else if (ns != NULL)
		match = xml_pattern_str_equal(step.ns, ns);
	} else if (((step.ns != NULL) == (ns != NULL)) &&
	    (name != NULL) &&
	    (step.name[0] == name[0]) &&
	    xml_pattern_str_equal(step.name, name) &&
	    ((step.ns == ns) || xml_pattern_str_equal(step.ns, ns)))
	{
	    match = 1;
	}
	final = step.flags & XML_STREAM_STEP_FINAL;
	if (match) {
	    if (final) {
		ret = 1;
            } else if (xml_stream_ctxt_add_state(stream, 1, stream->level) < 0) {
                return(-1);
            }
	    if ((ret != 1) && (step.flags & XML_STREAM_STEP_IN_SET)) {
		/*
		* Check if we have a special case like "foo//.", where
		* "foo" is selected as well.
		*/
		ret = 1;
	    }
	}
	if (((comp->flags & XML_STREAM_DESC) == 0) &&
	    ((! match) || final))  {
	    /*
	    * Mark this expression as blocked for any evaluation at
	    * deeper levels.
	    */
	    stream->blockLevel = stream->level;
	}

stream_next:
        stream = stream->next;
    } /* while stream != NULL */

    return(ret);
}

/**
 * Port of xmlStreamPush.
 * Push new data onto the stream.
 * Both `name` and `ns` being NULL means the / i.e. the root of the document.
 * This can also act as a reset.
 * Otherwise the function will act as if it has been given an element-node.
 *
 * @param stream  the stream context
 * @param name  the current name
 * @param ns  the namespace name
 * @returns -1 in case of error, 1 if the current state in the stream is a
 *    match and 0 otherwise.
 */
int
xml_stream_push(XmlStreamCtxt *stream,
              const char *name, const char *ns) {
    return (xml_stream_push_internal(stream, name, ns, XML_PAT_ELEMENT_NODE));
}

/**
 * Port of xmlStreamPushNode.
 * Push new data onto the stream.
 * Both `name` and `ns` being NULL means the / i.e. the root of the document.
 * This can also act as a reset.
 * Different from #xml_stream_push this function can be fed with nodes of type:
 * element-, attribute-, text-, cdata-section-, comment- and
 * processing-instruction-node.
 *
 * @param stream  the stream context
 * @param name  the current name
 * @param ns  the namespace name
 * @param nodeType  the type of the node being pushed
 * @returns -1 in case of error, 1 if the current state in the stream is a
 *    match and 0 otherwise.
 */
int
xml_stream_push_node(XmlStreamCtxt *stream,
		  const char *name, const char *ns,
		  int nodeType)
{
    return (xml_stream_push_internal(stream, name, ns,
	nodeType));
}

/**
 * Port of xmlStreamPushAttr.
 * Push new attribute data onto the stream.
 *
 * Both `name` and `ns` being NULL means the root of the document.
 * This can also act as a reset. Otherwise the function will act as
 * if it has been given an attribute-node.
 *
 * @param stream  the stream context
 * @param name  the current name
 * @param ns  the namespace name
 * @returns -1 in case of error, 1 if the current state in the stream
 * is a match and 0 otherwise.
 */
int
xml_stream_push_attr(XmlStreamCtxt *stream,
		  const char *name, const char *ns) {
    return (xml_stream_push_internal(stream, name, ns, XML_PAT_ATTRIBUTE_NODE));
}

/**
 * Port of xmlStreamPop.
 * push one level from the stream.
 *
 * @param stream  the stream context
 * @returns -1 in case of error, 0 otherwise.
 */
int
xml_stream_pop(XmlStreamCtxt *stream) {
    int i, lev;

    if (stream == NULL)
        return(-1);
    while (stream != NULL) {
	/*
	* Reset block-level.
	*/
	if (stream->blockLevel == stream->level)
	    stream->blockLevel = -1;

	/*
	 *  stream->level can be zero when XML_FINAL_IS_ANY_NODE is set
	 *  (see the thread at
	 *  http://mail.gnome.org/archives/xslt/2008-July/msg00027.html)
	 */
	if (stream->level)
	    stream->level--;
	/*
	 * Check evolution of existing states
	 */
	for (i = stream->nbState -1; i >= 0; i--) {
	    /* discard obsoleted states */
	    lev = stream->states[(2 * i) + 1];
	    if (lev > stream->level)
		stream->nbState--;
	    if (lev <= stream->level)
		break;
	}
	stream = stream->next;
    }
    return(0);
}

/**
 * Port of xmlStreamWantsAnyNode.
 * Query if the streaming pattern additionally needs to be fed with
 * text-, cdata-section-, comment- and processing-instruction-nodes.
 * If the result is 0 then only element-nodes and attribute-nodes
 * need to be pushed.
 *
 * @param streamCtxt  the stream context
 * @returns 1 in case of need of nodes of the above described types,
 *          0 otherwise. -1 on API errors.
 */
int
xml_stream_wants_any_node(XmlStreamCtxt *streamCtxt)
{
    if (streamCtxt == NULL)
	return(-1);
    while (streamCtxt != NULL) {
	if (streamCtxt->comp->flags & XML_STREAM_FINAL_IS_ANY_NODE)
	    return(1);
	streamCtxt = streamCtxt->next;
    }
    return(0);
}

/************************************************************************
 *									*
 *			The public interfaces				*
 *									*
 ************************************************************************/

/**
 * Port of xmlPatternCompileSafe.
 * Compile a pattern.
 *
 * @param pattern  the pattern to compile
 * @param flags  compilation flags, see XML_PATTERN_*
 * @param namespaces  the prefix definitions, array of [URI, prefix] or NULL
 * @param patternOut  output pattern
 * @returns 0 on success, 1 on error, -1 if a memory allocation failed.
 */
int
xml_pattern_compile_safe(const char *pattern, int flags,
                      const char **namespaces, XmlPattern **patternOut) {
    XmlPattern *ret = NULL, *cur;
    XmlPatParserContext *ctxt = NULL;
    const char *orp, *start;
    char *tmp = NULL;
    int type = 0;
    int streamable = 1;
    int error;

    if (patternOut == NULL)
        return(1);

    if (pattern == NULL) {
        error = 1;
        goto error;
    }

    start = pattern;
    orp = start;
    while (*orp != 0) {
	tmp = NULL;
	while ((*orp != 0) && (*orp != '|')) orp++;
        if (*orp == 0)
	    ctxt = xml_pattern_new_parser_context(start, namespaces);
	else {
	    tmp = xml_pattern_strndup(start, orp - start);
	    if (tmp != NULL) {
		ctxt = xml_pattern_new_parser_context(tmp, namespaces);
	    }
	    orp++;
	}
	if (ctxt == NULL) {
            error = -1;
            goto error;
        }
	cur = xml_pattern_new();
	if (cur == NULL) {
            error = -1;
            goto error;
        }
	if (ret == NULL)
	    ret = cur;
	else {
	    cur->next = ret->next;
	    ret->next = cur;
	}
	cur->flags = flags;
	ctxt->comp = cur;

	if (XML_STREAM_XS_IDC(cur))
	    xml_pattern_compile_idc_xpath_path(ctxt);
	else
	    xml_pattern_compile_path_pattern(ctxt);
	if (ctxt->error != 0) {
            error = ctxt->error;
	    goto error;
        }
	xml_pattern_free_parser_context(ctxt);
	ctxt = NULL;


        if (streamable) {
	    if (type == 0) {
	        type = cur->flags & (PAT_FROM_ROOT | PAT_FROM_CUR);
	    } else if (type == PAT_FROM_ROOT) {
	        if (cur->flags & PAT_FROM_CUR)
		    streamable = 0;
	    } else if (type == PAT_FROM_CUR) {
	        if (cur->flags & PAT_FROM_ROOT)
		    streamable = 0;
	    }
	}
	if (streamable) {
	    error = xml_stream_compile(cur);
            if (error != 0)
                goto error;
        }
	error = xml_pattern_reverse(cur);
        if (error != 0)
	    goto error;
	if (tmp != NULL) {
	    free(tmp);
	    tmp = NULL;
	}
	start = orp;
    }
    if (streamable == 0) {
        cur = ret;
	while (cur != NULL) {
	    if (cur->stream != NULL) {
		xml_stream_comp_free(cur->stream);
		cur->stream = NULL;
	    }
	    cur = cur->next;
	}
    }

    *patternOut = ret;
    return(0);
error:
    if (ctxt != NULL) xml_pattern_free_parser_context(ctxt);
    if (ret != NULL) xml_pattern_free(ret);
    if (tmp != NULL) free(tmp);
    *patternOut = NULL;
    return(error);
}

/**
 * Port of xmlPatterncompile.
 * Compile a pattern.
 *
 * @param pattern  the pattern to compile
 * @param flags  compilation flags, see XML_PATTERN_*
 * @param namespaces  the prefix definitions, array of [URI, prefix] or NULL
 * @returns the compiled form of the pattern or NULL in case of error
 */
XmlPattern *
xml_pattern_compile(const char *pattern, int flags,
                  const char **namespaces) {
    XmlPattern *ret;
    xml_pattern_compile_safe(pattern, flags, namespaces, &ret);
    return(ret);
}

/**
 * Port of xmlPatternMatch.
 * Test whether the node matches the pattern
 *
 * @param comp  the precompiled pattern
 * @param node  a node
 * @returns 1 if it matches, 0 if it doesn't and -1 in case of failure
 */
int
xml_pattern_match(XmlPattern *comp, XMQNode *node)
{
    int ret = 0;

    if ((comp == NULL) || (node == NULL))
        return(-1);

    while (comp != NULL) {
        ret = xml_pattern_pat_match(comp, node);
	if (ret != 0)
	    return(ret);
	comp = comp->next;
    }
    return(ret);
}

/**
 * Port of xmlPatternGetStreamCtxt.
 * Get a streaming context for that pattern
 * Use #xml_stream_ctxt_free to free the context.
 *
 * @param comp  the precompiled pattern
 * @returns a pointer to the context or NULL in case of failure
 */
XmlStreamCtxt *
xml_pattern_get_stream_ctxt(XmlPattern *comp)
{
    XmlStreamCtxt *ret = NULL, *cur;

    if ((comp == NULL) || (comp->stream == NULL))
        return(NULL);

    while (comp != NULL) {
        if (comp->stream == NULL)
	    goto failed;
	cur = xml_stream_ctxt_new(comp->stream);
	if (cur == NULL)
	    goto failed;
	if (ret == NULL)
	    ret = cur;
	else {
	    cur->next = ret->next;
	    ret->next = cur;
	}
	cur->flags = comp->flags;
	comp = comp->next;
    }
    return(ret);
failed:
    xml_stream_ctxt_free(ret);
    return(NULL);
}

/**
 * Port of xmlPatternStreamable.
 * Check if the pattern is streamable i.e. #xml_pattern_get_stream_ctxt
 * should work.
 *
 * @param comp  the precompiled pattern
 * @returns 1 if streamable, 0 if not and -1 in case of error.
 */
int
xml_pattern_streamable(XmlPattern *comp) {
    if (comp == NULL)
        return(-1);
    while (comp != NULL) {
        if (comp->stream == NULL)
	    return(0);
	comp = comp->next;
    }
    return(1);
}

/**
 * Port of xmlPatternMaxDepth.
 * Check the maximum depth reachable by a pattern
 *
 * @param comp  the precompiled pattern
 * @returns -2 if no limit (using //), otherwise the depth,
 *         and -1 in case of error
 */
int
xml_pattern_max_depth(XmlPattern *comp) {
    int ret = 0, i;
    if (comp == NULL)
        return(-1);
    while (comp != NULL) {
        if (comp->stream == NULL)
	    return(-1);
	for (i = 0;i < comp->stream->nbStep;i++)
	    if (comp->stream->steps[i].flags & XML_STREAM_STEP_DESC)
	        return(-2);
	if (comp->stream->nbStep > ret)
	    ret = comp->stream->nbStep;
	comp = comp->next;
    }
    return(ret);
}

/**
 * Port of xmlPatternMinDepth.
 * Check the minimum depth reachable by a pattern, 0 mean the / or . are
 * part of the set.
 *
 * @param comp  the precompiled pattern
 * @returns -1 in case of error otherwise the depth,
 */
int
xml_pattern_min_depth(XmlPattern *comp) {
    int ret = 12345678;
    if (comp == NULL)
        return(-1);
    while (comp != NULL) {
        if (comp->stream == NULL)
	    return(-1);
	if (comp->stream->nbStep < ret)
	    ret = comp->stream->nbStep;
	if (ret == 0)
	    return(0);
	comp = comp->next;
    }
    return(ret);
}

/**
 * Port of xmlPatternFromRoot.
 * Check if the pattern must be looked at from the root.
 *
 * @param comp  the precompiled pattern
 * @returns 1 if true, 0 if false and -1 in case of error
 */
int
xml_pattern_from_root(XmlPattern *comp) {
    if (comp == NULL)
        return(-1);
    while (comp != NULL) {
        if (comp->stream == NULL)
	    return(-1);
	if (comp->flags & PAT_FROM_ROOT)
	    return(1);
	comp = comp->next;
    }
    return(0);

}

// The macros of libxml2 pattern.c, not shared with the other parts.
#undef ERROR
#undef ERROR5
#undef XML_STREAM_STEP_DESC
#undef XML_STREAM_STEP_FINAL
#undef XML_STREAM_STEP_ROOT
#undef XML_STREAM_STEP_ATTR
#undef XML_STREAM_STEP_NODE
#undef XML_STREAM_STEP_IN_SET
#undef XML_STREAM_FINAL_IS_ANY_NODE
#undef XML_STREAM_FROM_ROOT
#undef XML_STREAM_DESC
#undef XML_STREAM_ANY_NODE
#undef XML_PATTERN_NOTPATTERN
#undef XML_STREAM_XS_IDC
#undef XML_STREAM_XS_IDC_SEL
#undef XML_STREAM_XS_IDC_FIELD
#undef XML_PAT_COPY_NSNAME
#undef XML_PAT_FREE_STRING
#undef XML_PAT_MAX_ITEMS
#undef XML_PAT_MAX_NAME_LENGTH
#undef XML_PAT_XML_NAMESPACE
#undef XML_PAT_ELEMENT_NODE
#undef XML_PAT_ATTRIBUTE_NODE
#undef PAT_FROM_ROOT
#undef PAT_FROM_CUR
#undef CUR
#undef SKIP
#undef NXT
#undef PEEKPREV
#undef CUR_PTR
#undef SKIP_BLANKS
#undef CURRENT
#undef NEXT
#undef PUSH
#undef XML_PAT_IS_BLANK_CH

#endif // XML_PATTERN_MODULE
