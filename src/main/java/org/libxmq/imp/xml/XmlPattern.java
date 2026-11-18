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

package org.libxmq.imp.xml;

import static org.libxmq.imp.xml.XmlDomApi.*;

import java.nio.charset.StandardCharsets;

/**
 * Streamable patterns, a port of libxml2 pattern.c (xml_pattern.c): the XPath subset used for
 * xs:selector and xs:field of XML Schema identity constraints (and plain patterns like "a/b",
 * "//c", "/a/@b", alternatives with |).
 *
 * A compiled pattern can be matched against a node of the native dom (patternMatch), or, when
 * streamable, a stream context can be created that is fed with streamPush/streamPushAttr/streamPop
 * while walking a tree in document order.
 *
 * Namespaces are given like libxml2 does: an array of href, prefix pairs terminated by a null href
 * (or null for no namespaces). The prefix xml is always bound to the xml namespace.
 *
 * Compilation errors are not printed: libxml2 2.16 compiles its error messages away in pattern.c,
 * it only returns null. patternCompileSafe returns the error code.
 */
public final class XmlPattern
{
    /** simple pattern match */
    public static final int XML_PATTERN_FLAG_DEFAULT = 0;
    /** standard XPath pattern */
    public static final int XML_PATTERN_FLAG_XPATH = 1 << 0;
    /** XPath subset for schema selector */
    public static final int XML_PATTERN_FLAG_XSSEL = 1 << 1;
    /** XPath subset for schema field */
    public static final int XML_PATTERN_FLAG_XSFIELD = 1 << 2;

    private static final int XML_STREAM_STEP_DESC = 1;
    private static final int XML_STREAM_STEP_FINAL = 2;
    private static final int XML_STREAM_STEP_ROOT = 4;
    private static final int XML_STREAM_STEP_ATTR = 8;
    private static final int XML_STREAM_STEP_NODE = 16;
    private static final int XML_STREAM_STEP_IN_SET = 32;

    /*
     * NOTE: Those private flags (XML_STREAM_xxx) are used
     *   in XmlStreamCtxt->flag. They extend the public
     *   pattern flags, so be careful not to interfere with the
     *   reserved values for the pattern flags.
     */
    private static final int XML_STREAM_FINAL_IS_ANY_NODE = 1 << 14;
    private static final int XML_STREAM_FROM_ROOT = 1 << 15;
    private static final int XML_STREAM_DESC = 1 << 16;

    /*
     * XML_STREAM_ANY_NODE is used for comparison against
     * node type values, to indicate a node of any type.
     */
    private static final int XML_STREAM_ANY_NODE = 100;

    private static final int XML_PATTERN_NOTPATTERN = XML_PATTERN_FLAG_XPATH | XML_PATTERN_FLAG_XSSEL | XML_PATTERN_FLAG_XSFIELD;

    /* XML_MAX_ITEMS (private/memory.h) and XML_MAX_NAME_LENGTH (parserInternals.h). */
    private static final int XML_PAT_MAX_ITEMS = 1000000000;
    private static final int XML_PAT_MAX_NAME_LENGTH = 50000;
    private static final String XML_PAT_XML_NAMESPACE = "http://www.w3.org/XML/1998/namespace";

    /* The libxml2 xmlElementType values, the same as the XmlNodeKind values. */
    private static final int XML_PAT_ELEMENT_NODE = 1;
    private static final int XML_PAT_ATTRIBUTE_NODE = 2;

    private static final int PAT_FROM_ROOT = 1 << 8;
    private static final int PAT_FROM_CUR = 1 << 9;

    /* XmlPatOp */
    private static final int XML_PAT_OP_END = 0;
    private static final int XML_PAT_OP_ROOT = 1;
    private static final int XML_PAT_OP_ELEM = 2;
    private static final int XML_PAT_OP_CHILD = 3;
    private static final int XML_PAT_OP_ATTR = 4;
    private static final int XML_PAT_OP_PARENT = 5;
    private static final int XML_PAT_OP_ANCESTOR = 6;
    private static final int XML_PAT_OP_NS = 7;
    private static final int XML_PAT_OP_ALL = 8;

    // XmlPatStreamStep
    static final class StreamStep
    {
        int flags;          /* properties of that step */
        String name;        /* first string value if NULL accept all */
        String ns;          /* second string value */
        int node_type;       /* type of node */
    }

    // XmlPatStreamComp
    static final class StreamComp
    {
        int nb_step;         /* number of steps in the automata */
        int max_step;        /* allocated number of steps */
        StreamStep[] steps; /* the array of steps */
        int flags;
    }

    // XmlPatStepState
    private static final class StepState
    {
        int step;
        XmlNode node;
    }

    // XmlPatStepStates
    private static final class StepStates
    {
        int nbstates;
        int maxstates;
        StepState[] states;
    }

    // XmlPatStepOp
    private static final class StepOp
    {
        int op;
        String value;
        String value2; /* The namespace name */
    }

    // XmlPatParserContext
    private static final class PatParserContext
    {
        byte[] buf;         /* the expression, 0 terminated */
        int cur;            /* the current char being parsed */
        String base;        /* the full expression */
        int error;          /* error code */
        XmlPattern comp;    /* the result */
        XmlNode elem;       /* the current node if any */
        String[] namespaces;/* the namespaces definitions */
        int nb_namespaces;  /* the number of namespaces */
    }

    // struct XmlPattern
    Object data;            /* the associated template */
    XmlPattern next;        /* next pattern if | is used */
    String pattern;         /* the pattern */
    int flags;              /* flags */
    int nb_step;
    int max_step;
    StepOp[] steps;         /* ops for computation */
    StreamComp stream;      /* the streaming data if any */

    private XmlPattern()
    {
    }

    /* IS_BLANK_CH of libxml2. */
    private static boolean isBlankCh(int c)
    {
        return (c == 0x20) || ((0x9 <= c) && (c <= 0xa)) || (c == 0xd);
    }

    private static int xsIdc(int flags)
    {
        return flags & (XML_PATTERN_FLAG_XSSEL | XML_PATTERN_FLAG_XSFIELD);
    }

    private static int xsIdcSel(int flags)
    {
        return flags & XML_PATTERN_FLAG_XSSEL;
    }

    /** The first char of a C string, 0 for "" (and for NULL, where C would crash). */
    private static int c0(String s)
    {
        if (s == null || s.isEmpty()) return 0;
        return s.charAt(0);
    }

    /* ***********************************************************************
     *                                                                      *
     *          Helpers (libxml2 xmlstring.c, memory.h, parser.c)           *
     *                                                                      *
     ************************************************************************/

    // xml_pattern_strndup
    /** Port of xmlStrndup. */
    private static String strndup(byte[] s, int off, int len)
    {
        if (s == null) return null;
        return new String(s, off, len, StandardCharsets.UTF_8);
    }

    // xml_pattern_str_equal
    /** Port of xmlStrEqual. */
    private static int strEqual(String a, String b)
    {
        if (a == b) return 1;
        if (a == null || b == null) return 0;
        return a.equals(b) ? 1 : 0;
    }

    // xml_pattern_grow_capacity
    /** Port of xmlGrowCapacity (private/memory.h). */
    private static int growCapacity(int capacity, long elem_size, int min, int max)
    {
        if (capacity <= 0) return min;
        if ((capacity >= max) || ((long) capacity > Long.MAX_VALUE / 2 / elem_size)) return -1;
        int extra = (capacity + 1) / 2;
        if (capacity > max - extra) return max;
        return capacity + extra;
    }

    // xml_pattern_get_utf8_char
    /** Port of xmlGetUTF8Char (xmlstring.c). utf must be 0 terminated. */
    private static int getUtf8Char(byte[] utf, int p, int[] len)
    {
        int c;

        c = utf[p] & 0xff;
        if (c < 0x80)
        {
            if (len[0] < 1)
            {
                len[0] = 0;
                return -1;
            }
            /* 1-byte code */
            len[0] = 1;
        }
        else
        {
            if ((len[0] < 2) || ((utf[p + 1] & 0xc0) != 0x80))
            {
                len[0] = 0;
                return -1;
            }
            if (c < 0xe0)
            {
                if (c < 0xc2)
                {
                    len[0] = 0;
                    return -1;
                }
                /* 2-byte code */
                len[0] = 2;
                c = (c & 0x1f) << 6;
                c |= utf[p + 1] & 0x3f;
            }
            else
            {
                if ((len[0] < 3) || ((utf[p + 2] & 0xc0) != 0x80))
                {
                    len[0] = 0;
                    return -1;
                }
                if (c < 0xf0)
                {
                    /* 3-byte code */
                    len[0] = 3;
                    c = (c & 0xf) << 12;
                    c |= (utf[p + 1] & 0x3f) << 6;
                    c |= utf[p + 2] & 0x3f;
                    if ((c < 0x800) || ((c >= 0xd800) && (c < 0xe000)))
                    {
                        len[0] = 0;
                        return -1;
                    }
                }
                else
                {
                    if ((len[0] < 4) || ((utf[p + 3] & 0xc0) != 0x80))
                    {
                        len[0] = 0;
                        return -1;
                    }
                    len[0] = 4;
                    /* 4-byte code */
                    c = (c & 0x7) << 18;
                    c |= (utf[p + 1] & 0x3f) << 12;
                    c |= (utf[p + 2] & 0x3f) << 6;
                    c |= utf[p + 3] & 0x3f;
                    if ((c < 0x10000) || (c >= 0x110000))
                    {
                        len[0] = 0;
                        return -1;
                    }
                }
            }
        }
        return c;
    }

    // xml_pattern_is_name_start_char
    /** Port of xmlIsNameStartCharNew (parser.c). */
    private static int isNameStartChar(int c)
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
            return 1;
        return 0;
    }

    // xml_pattern_is_name_char
    /** Port of xmlIsNameCharNew (parser.c). */
    private static int isNameChar(int c)
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
            return 1;
        return 0;
    }

    // xml_pattern_scan_name_end
    /**
     * Port of xmlScanName (parser.c), for XML 1.0 5th edition names.
     * Scan an XML Name (nc 0) or NCName (nc 1).
     *
     * Returns the index of the end of the name on success. If the
     * name is invalid, returns `ptr`. If the name is longer than
     * `max_size` bytes, returns -1 (NULL).
     */
    private static int scanNameEnd(byte[] buf, int ptr, long max_size, int nc)
    {
        int stop = nc != 0 ? ':' : 0;
        int first = 1;
        int[] len = new int[1];

        while (true)
        {
            int c;

            c = buf[ptr] & 0xff;
            if (c < 0x80)
            {
                if (c == stop)
                    break;
                len[0] = 1;
            }
            else
            {
                len[0] = 4;
                c = getUtf8Char(buf, ptr, len);
                if (c < 0)
                    break;
            }

            if (first != 0 ? isNameStartChar(c) == 0 : isNameChar(c) == 0)
                break;

            if (len[0] > max_size)
                return -1;
            ptr += len[0];
            max_size -= len[0];
            first = 0;
        }

        return ptr;
    }

    /* ***********************************************************************
     *                                                                      *
     *                      Type functions                                  *
     *                                                                      *
     ************************************************************************/

    // xml_pattern_new
    /**
     * Port of xmlNewPattern.
     * Create a new XSLT Pattern
     */
    private static XmlPattern patternNew()
    {
        XmlPattern cur = new XmlPattern();
        cur.steps = null;
        cur.max_step = 0;
        return cur;
    }

    // xml_pattern_free
    /**
     * Port of xmlFreePattern.
     * Free up the memory allocated by `comp`
     * @param comp an XSLT comp
     */
    public static void patternFree(XmlPattern comp)
    {
        patternFreeList(comp);
    }

    // xml_pattern_free_internal
    /** Port of xmlFreePatternInternal. */
    private static void patternFreeInternal(XmlPattern comp)
    {
        if (comp == null)
            return;
        if (comp.stream != null)
            streamCompFree(comp.stream);
        comp.stream = null;
        comp.steps = null;
        comp.nb_step = 0;
        comp.max_step = 0;
    }

    // xml_pattern_free_list
    /**
     * Port of xmlFreePatternList.
     * Free up the memory allocated by all the elements of `comp`
     * @param comp an XSLT comp list
     */
    public static void patternFreeList(XmlPattern comp)
    {
        XmlPattern cur;

        while (comp != null)
        {
            cur = comp;
            comp = comp.next;
            cur.next = null;
            patternFreeInternal(cur);
        }
    }

    // xml_pattern_new_parser_context
    /**
     * Port of xmlNewPatParserContext.
     * Create a new XML pattern parser context
     *
     * @param pattern  the pattern, 0 terminated bytes
     * @param namespaces  the prefix definitions, array of [URI, prefix] terminated
     *              with [NULL, NULL] or NULL if no namespace is used
     */
    private static PatParserContext newParserContext(byte[] pattern, String base, String[] namespaces)
    {
        PatParserContext cur;

        if (pattern == null)
            return null;

        cur = new PatParserContext();
        cur.buf = pattern;
        cur.cur = 0;
        cur.base = base;
        if (namespaces != null)
        {
            int i;
            for (i = 0; 2 * i < namespaces.length && namespaces[2 * i] != null; i++)
                ;
            cur.nb_namespaces = i;
        }
        else
        {
            cur.nb_namespaces = 0;
        }
        cur.namespaces = namespaces;
        return cur;
    }

    // xml_pattern_free_parser_context
    /** Port of xmlFreePatParserContext. */
    private static void freeParserContext(PatParserContext ctxt)
    {
    }

    // xml_pattern_grow
    /** Port of xmlPatternGrow. */
    private static int patternGrow(XmlPattern comp)
    {
        int new_size;

        new_size = growCapacity(comp.max_step, 24, 10, XML_PAT_MAX_ITEMS);
        if (new_size < 0)
            return -1;
        StepOp[] temp = new StepOp[new_size];
        int i = 0;
        if (comp.steps != null)
        {
            for (; i < comp.steps.length && i < new_size; i++) temp[i] = comp.steps[i];
        }
        for (; i < new_size; i++) temp[i] = new StepOp();
        comp.steps = temp;
        comp.max_step = new_size;

        return 0;
    }

    // xml_pattern_add
    /**
     * Port of xmlPatternAdd.
     * Add a step to an XSLT Compiled Match
     * @returns -1 in case of failure, 0 otherwise.
     */
    private static int patternAdd(PatParserContext ctxt, XmlPattern comp, int op, String value, String value2)
    {
        if (comp.nb_step >= comp.max_step)
        {
            if (patternGrow(comp) < 0)
            {
                ctxt.error = -1;
                return -1;
            }
        }
        comp.steps[comp.nb_step].op = op;
        comp.steps[comp.nb_step].value = value;
        comp.steps[comp.nb_step].value2 = value2;
        comp.nb_step++;
        return 0;
    }

    // xml_pattern_reverse
    /**
     * Port of xmlReversePattern.
     * reverse all the stack of expressions
     * @returns 0 in case of success and -1 in case of error.
     */
    private static int patternReverse(XmlPattern comp)
    {
        int i, j;

        /*
         * remove the leading // for //a or .//a
         */
        if ((comp.nb_step > 0) && (comp.steps[0].op == XML_PAT_OP_ANCESTOR))
        {
            for (i = 0, j = 1; j < comp.nb_step; i++, j++)
            {
                comp.steps[i].value = comp.steps[j].value;
                comp.steps[i].value2 = comp.steps[j].value2;
                comp.steps[i].op = comp.steps[j].op;
            }
            comp.nb_step--;
        }

        /*
         * Grow to add OP_END later
         */
        if (comp.nb_step >= comp.max_step)
        {
            if (patternGrow(comp) < 0)
                return -1;
        }

        i = 0;
        j = comp.nb_step - 1;
        while (j > i)
        {
            String tmp;
            int op;
            tmp = comp.steps[i].value;
            comp.steps[i].value = comp.steps[j].value;
            comp.steps[j].value = tmp;
            tmp = comp.steps[i].value2;
            comp.steps[i].value2 = comp.steps[j].value2;
            comp.steps[j].value2 = tmp;
            op = comp.steps[i].op;
            comp.steps[i].op = comp.steps[j].op;
            comp.steps[j].op = op;
            j--;
            i++;
        }

        comp.steps[comp.nb_step].value = null;
        comp.steps[comp.nb_step].value2 = null;
        comp.steps[comp.nb_step++].op = XML_PAT_OP_END;
        return 0;
    }

    /* ***********************************************************************
     *                                                                      *
     *              The interpreter for the precompiled patterns            *
     *                                                                      *
     ************************************************************************/

    // xml_pattern_push_state
    /** Port of xmlPatPushState. */
    private static int pushState(StepStates states, int step, XmlNode node)
    {
        if (states.maxstates <= states.nbstates)
        {
            int new_size;

            new_size = growCapacity(states.maxstates, 16, 4, XML_PAT_MAX_ITEMS);
            if (new_size < 0)
                return -1;
            StepState[] tmp = new StepState[new_size];
            int i = 0;
            if (states.states != null)
            {
                for (; i < states.states.length && i < new_size; i++) tmp[i] = states.states[i];
            }
            for (; i < new_size; i++) tmp[i] = new StepState();
            states.states = tmp;
            states.maxstates = new_size;
        }
        states.states[states.nbstates].step = step;
        states.states[states.nbstates++].node = node;
        return 0;
    }

    // xml_pattern_node_ns_href
    /** The namespace href of an element or attribute (libxml2 node->ns->href), null if none. */
    private static String nodeNsHref(XmlNode node)
    {
        XmlNs ns = nodeNs(node);
        return ns != null ? nsHref(ns) : null;
    }

    /** Namespace test of ELEM and PARENT, true means rollback. */
    private static boolean elemNsMismatch(StepOp step, XmlNode node)
    {
        if (nodeNs(node) == null)
        {
            if (step.value2 != null)
                return true;
        }
        else if (nodeNsHref(node) != null)
        {
            if (step.value2 == null)
                return true;
            if (strEqual(step.value2, nodeNsHref(node)) == 0)
                return true;
        }
        return false;
    }

    // xml_pattern_pat_match
    /**
     * Port of xmlPatMatch.
     * Test whether the node matches the pattern
     * @returns 1 if it matches, 0 if it doesn't and -1 in case of failure
     */
    private static int patMatch(XmlPattern comp, XmlNode node)
    {
        int i;
        StepOp step;
        StepStates states = new StepStates(); /* // may require backtrack */

        if ((comp == null) || (node == null)) return -1;
        i = 0;
        restart:
        while (true)
        {
            boolean rollback = false;
            steps:
            for (; i < comp.nb_step; i++)
            {
                step = comp.steps[i];
                switch (step.op)
                {
                case XML_PAT_OP_END:
                    break steps; /* found */
                case XML_PAT_OP_ROOT:
                    if (nodeKind(node) == NAMESPACE)
                    {
                        rollback = true;
                        break steps;
                    }
                    node = nodeParent(node);
                    /* libxml2 dereferences the parent, only a detached node has none. */
                    if (node == null)
                    {
                        rollback = true;
                        break steps;
                    }
                    if (nodeKind(node) == DOCUMENT)
                        continue;
                    rollback = true;
                    break steps;
                case XML_PAT_OP_ELEM:
                    if (nodeKind(node) != ELEMENT)
                    {
                        rollback = true;
                        break steps;
                    }
                    if (step.value == null)
                        continue;
                    if (c0(step.value) != c0(nodeName(node))
                        || strEqual(step.value, nodeName(node)) == 0
                        || elemNsMismatch(step, node))
                    {
                        rollback = true;
                        break steps;
                    }
                    continue;
                case XML_PAT_OP_CHILD:
                {
                    XmlNode lst;

                    if ((nodeKind(node) != ELEMENT) &&
                        (nodeKind(node) != DOCUMENT))
                    {
                        rollback = true;
                        break steps;
                    }

                    lst = nodeFirstChild(node);

                    if (step.value != null)
                    {
                        while (lst != null)
                        {
                            if ((nodeKind(lst) == ELEMENT) &&
                                (c0(step.value) == c0(nodeName(lst))) &&
                                (strEqual(step.value, nodeName(lst)) != 0))
                                break;
                            lst = nodeNext(lst);
                        }
                        if (lst != null)
                            continue;
                    }
                    rollback = true;
                    break steps;
                }
                case XML_PAT_OP_ATTR:
                    if (nodeKind(node) != ATTRIBUTE)
                    {
                        rollback = true;
                        break steps;
                    }
                    if (step.value != null)
                    {
                        if (c0(step.value) != c0(nodeName(node))
                            || strEqual(step.value, nodeName(node)) == 0)
                        {
                            rollback = true;
                            break steps;
                        }
                    }
                    /* Namespace test */
                    if (nodeNs(node) == null)
                    {
                        if (step.value2 != null)
                        {
                            rollback = true;
                            break steps;
                        }
                    }
                    else if (step.value2 != null)
                    {
                        if (strEqual(step.value2, nodeNsHref(node)) == 0)
                        {
                            rollback = true;
                            break steps;
                        }
                    }
                    continue;
                case XML_PAT_OP_PARENT:
                    if ((nodeKind(node) == DOCUMENT) ||
                        (nodeKind(node) == NAMESPACE))
                    {
                        rollback = true;
                        break steps;
                    }
                    node = nodeParent(node);
                    if (node == null)
                    {
                        rollback = true;
                        break steps;
                    }
                    if (step.value == null)
                        continue;
                    if (c0(step.value) != c0(nodeName(node))
                        || strEqual(step.value, nodeName(node)) == 0
                        || elemNsMismatch(step, node))
                    {
                        rollback = true;
                        break steps;
                    }
                    continue;
                case XML_PAT_OP_ANCESTOR:
                    /* TODO: implement coalescing of ANCESTOR/NODE ops */
                    if (step.value == null)
                    {
                        i++;
                        if (i >= comp.steps.length)
                        {
                            rollback = true;
                            break steps;
                        }
                        step = comp.steps[i];
                        if (step.op == XML_PAT_OP_ROOT)
                            break steps; /* found */
                        if (step.op != XML_PAT_OP_ELEM)
                        {
                            rollback = true;
                            break steps;
                        }
                        if (step.value == null)
                        {
                            /* libxml2 leaks the rollback states here. */
                            return -1;
                        }
                    }
                    if (node == null)
                    {
                        rollback = true;
                        break steps;
                    }
                    if ((nodeKind(node) == DOCUMENT) ||
                        (nodeKind(node) == NAMESPACE))
                    {
                        rollback = true;
                        break steps;
                    }
                    node = nodeParent(node);
                    while (node != null)
                    {
                        if ((nodeKind(node) == ELEMENT) &&
                            (c0(step.value) == c0(nodeName(node))) &&
                            (strEqual(step.value, nodeName(node)) != 0))
                        {
                            /* Namespace test */
                            if (nodeNs(node) == null)
                            {
                                if (step.value2 == null)
                                    break;
                            }
                            else if (nodeNsHref(node) != null)
                            {
                                if ((step.value2 != null) &&
                                    (strEqual(step.value2, nodeNsHref(node)) != 0))
                                    break;
                            }
                        }
                        node = nodeParent(node);
                    }
                    if (node == null)
                    {
                        rollback = true;
                        break steps;
                    }
                    /*
                     * prepare a potential rollback from here
                     * for ancestors of that node.
                     */
                    if (step.op == XML_PAT_OP_ANCESTOR)
                        pushState(states, i, node);
                    else
                        pushState(states, i - 1, node);
                    continue;
                case XML_PAT_OP_NS:
                    if (nodeKind(node) != ELEMENT)
                    {
                        rollback = true;
                        break steps;
                    }
                    if (nodeNs(node) == null)
                    {
                        if (step.value != null)
                        {
                            rollback = true;
                            break steps;
                        }
                    }
                    else if (nodeNsHref(node) != null)
                    {
                        if (step.value == null
                            || strEqual(step.value, nodeNsHref(node)) == 0)
                        {
                            rollback = true;
                            break steps;
                        }
                    }
                    /* The C break only leaves the switch. */
                    break;
                case XML_PAT_OP_ALL:
                    if (nodeKind(node) != ELEMENT)
                    {
                        rollback = true;
                        break steps;
                    }
                    break;
                default:
                    break;
                }
            }
            if (!rollback)
            {
                /* found */
                return 1;
            }
            /* rollback: got an error try to rollback */
            if (states.states == null)
                return 0;
            if (states.nbstates <= 0)
                return 0;
            states.nbstates--;
            i = states.states[states.nbstates].step;
            node = states.states[states.nbstates].node;
            continue restart;
        }
    }

    /* ***********************************************************************
     *                                                                      *
     *                      Dedicated parser for templates                  *
     *                                                                      *
     ************************************************************************/

    // CUR
    private static int CUR(PatParserContext ctxt)
    {
        return ctxt.buf[ctxt.cur] & 0xff;
    }

    // NXT
    private static int NXT(PatParserContext ctxt, int val)
    {
        return ctxt.buf[ctxt.cur + val] & 0xff;
    }

    // PEEKPREV
    private static int PEEKPREV(PatParserContext ctxt, int val)
    {
        return ctxt.buf[ctxt.cur - val] & 0xff;
    }

    // NEXT
    private static void NEXT(PatParserContext ctxt)
    {
        if (ctxt.buf[ctxt.cur] != 0) ctxt.cur++;
    }

    // SKIP_BLANKS
    private static void SKIP_BLANKS(PatParserContext ctxt)
    {
        while (isBlankCh(CUR(ctxt))) NEXT(ctxt);
    }

    // PUSH (returns true when C does goto error)
    private static boolean PUSH(PatParserContext ctxt, int op, String val, String val2)
    {
        return patternAdd(ctxt, ctxt.comp, op, val, val2) != 0;
    }

    // xml_pattern_scan_name
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
     * @returns the Name parsed or NULL
     */
    private static String scanName(PatParserContext ctxt)
    {
        int q, cur;
        String ret;

        SKIP_BLANKS(ctxt);

        q = ctxt.cur;
        cur = scanNameEnd(ctxt.buf, q, XML_PAT_MAX_NAME_LENGTH, 0);
        if ((cur < 0) || (cur == q))
            return null;
        ret = strndup(ctxt.buf, q, cur - q);
        ctxt.cur = cur;
        return ret;
    }

    // xml_pattern_scan_ncname
    /**
     * Port of xmlPatScanNCName.
     * Parses a non qualified name
     * @returns the Name parsed or NULL
     */
    private static String scanNcname(PatParserContext ctxt)
    {
        int q, cur;
        String ret;

        SKIP_BLANKS(ctxt);

        q = ctxt.cur;
        cur = scanNameEnd(ctxt.buf, q, XML_PAT_MAX_NAME_LENGTH, 1);
        if ((cur < 0) || (cur == q))
            return null;
        ret = strndup(ctxt.buf, q, cur - q);
        if (ret == null)
            ctxt.error = -1;
        ctxt.cur = cur;
        return ret;
    }

    /** The namespace lookup of a prefix, the xml prefix is always bound. null if not bound. */
    private static String lookupPrefix(PatParserContext ctxt, String prefix)
    {
        if (prefix.equals("xml"))
        {
            return XML_PAT_XML_NAMESPACE;
        }
        int i;
        for (i = 0; i < ctxt.nb_namespaces; i++)
        {
            if (strEqual(ctxt.namespaces[2 * i + 1], prefix) != 0)
            {
                return ctxt.namespaces[2 * i];
            }
        }
        return null;
    }

    /** True when the prefix has no binding (i >= nb_namespaces in C). */
    private static boolean prefixUnbound(PatParserContext ctxt, String prefix)
    {
        if (prefix.equals("xml")) return false;
        for (int i = 0; i < ctxt.nb_namespaces; i++)
        {
            if (strEqual(ctxt.namespaces[2 * i + 1], prefix) != 0) return false;
        }
        return true;
    }

    // xml_pattern_compile_attribute_test
    /**
     * Port of xmlCompileAttributeTest.
     * Compile an attribute test.
     */
    private static void compileAttributeTest(PatParserContext ctxt)
    {
        String token = null;
        String name = null;
        String URL = null;

        SKIP_BLANKS(ctxt);
        name = scanNcname(ctxt);
        if (ctxt.error < 0)
            return;
        if (name == null)
        {
            if (CUR(ctxt) == '*')
            {
                if (PUSH(ctxt, XML_PAT_OP_ATTR, null, null)) return;
                NEXT(ctxt);
            }
            else
            {
                ctxt.error = 1;
            }
            return;
        }
        if (CUR(ctxt) == ':')
        {
            String prefix = name;

            NEXT(ctxt);

            if (isBlankCh(CUR(ctxt)))
            {
                /* Invalid QName. */
                ctxt.error = 1;
                return;
            }
            /*
             * This is a namespace match
             */
            token = scanName(ctxt);
            if (prefixUnbound(ctxt, prefix))
            {
                /* xmlCompileAttributeTest : no namespace bound to prefix */
                ctxt.error = 1;
                return;
            }
            URL = lookupPrefix(ctxt, prefix);
            name = null;
            if (token == null)
            {
                if (CUR(ctxt) == '*')
                {
                    NEXT(ctxt);
                    if (PUSH(ctxt, XML_PAT_OP_ATTR, null, URL)) return;
                }
                else
                {
                    ctxt.error = 1;
                    return;
                }
            }
            else
            {
                if (PUSH(ctxt, XML_PAT_OP_ATTR, token, URL)) return;
            }
        }
        else
        {
            if (PUSH(ctxt, XML_PAT_OP_ATTR, name, null)) return;
        }
    }

    // xml_pattern_compile_step_pattern
    /**
     * Port of xmlCompileStepPattern.
     * Compile the Step Pattern and generates a precompiled
     * form suitable for fast matching.
     *
     * [3]    Step    ::=    '.' | NameTest
     * [4]    NameTest    ::=    QName | '*' | NCName ':' '*'
     */
    private static void compileStepPattern(PatParserContext ctxt)
    {
        String token = null;
        String name = null;
        String URL = null;
        int has_blanks = 0;

        SKIP_BLANKS(ctxt);
        if (CUR(ctxt) == '.')
        {
            /*
             * Context node.
             */
            NEXT(ctxt);
            if (PUSH(ctxt, XML_PAT_OP_ELEM, null, null)) return;
            return;
        }
        if (CUR(ctxt) == '@')
        {
            /*
             * Attribute test.
             */
            if (xsIdcSel(ctxt.comp.flags) != 0)
            {
                /* Unexpected attribute axis */
                ctxt.error = 1;
                return;
            }
            NEXT(ctxt);
            compileAttributeTest(ctxt);
            return;
        }
        name = scanNcname(ctxt);
        if (ctxt.error < 0)
            return;
        if (name == null)
        {
            if (CUR(ctxt) == '*')
            {
                NEXT(ctxt);
                if (PUSH(ctxt, XML_PAT_OP_ALL, null, null)) return;
                return;
            }
            else
            {
                /* xmlCompileStepPattern : Name expected */
                ctxt.error = 1;
                return;
            }
        }
        if (isBlankCh(CUR(ctxt)))
        {
            has_blanks = 1;
            SKIP_BLANKS(ctxt);
        }
        if (CUR(ctxt) == ':')
        {
            NEXT(ctxt);
            if (CUR(ctxt) != ':')
            {
                String prefix = name;

                if (has_blanks != 0 || isBlankCh(CUR(ctxt)))
                {
                    /* Invalid QName. */
                    ctxt.error = 1;
                    return;
                }
                /*
                 * This is a namespace match
                 */
                token = scanName(ctxt);
                if (prefixUnbound(ctxt, prefix))
                {
                    /* xmlCompileStepPattern : no namespace bound to prefix */
                    ctxt.error = 1;
                    return;
                }
                URL = lookupPrefix(ctxt, prefix);
                name = null;
                if (token == null)
                {
                    if (CUR(ctxt) == '*')
                    {
                        NEXT(ctxt);
                        if (PUSH(ctxt, XML_PAT_OP_NS, URL, null)) return;
                    }
                    else
                    {
                        /* xmlCompileStepPattern : Name expected */
                        ctxt.error = 1;
                        return;
                    }
                }
                else
                {
                    if (PUSH(ctxt, XML_PAT_OP_ELEM, token, URL)) return;
                }
            }
            else
            {
                NEXT(ctxt);
                if (strEqual(name, "child") != 0)
                {
                    name = scanName(ctxt);
                    if (name == null)
                    {
                        if (CUR(ctxt) == '*')
                        {
                            NEXT(ctxt);
                            if (PUSH(ctxt, XML_PAT_OP_ALL, null, null)) return;
                            return;
                        }
                        else
                        {
                            /* xmlCompileStepPattern : QName expected */
                            ctxt.error = 1;
                            return;
                        }
                    }
                    if (CUR(ctxt) == ':')
                    {
                        String prefix = name;

                        NEXT(ctxt);
                        if (isBlankCh(CUR(ctxt)))
                        {
                            /* Invalid QName. */
                            ctxt.error = 1;
                            return;
                        }
                        /*
                         * This is a namespace match
                         */
                        token = scanName(ctxt);
                        if (prefixUnbound(ctxt, prefix))
                        {
                            /* xmlCompileStepPattern : no namespace bound to prefix */
                            ctxt.error = 1;
                            return;
                        }
                        URL = lookupPrefix(ctxt, prefix);
                        name = null;
                        if (token == null)
                        {
                            if (CUR(ctxt) == '*')
                            {
                                NEXT(ctxt);
                                if (PUSH(ctxt, XML_PAT_OP_NS, URL, null)) return;
                            }
                            else
                            {
                                /* xmlCompileStepPattern : Name expected */
                                ctxt.error = 1;
                                return;
                            }
                        }
                        else
                        {
                            if (PUSH(ctxt, XML_PAT_OP_ELEM, token, URL)) return;
                        }
                    }
                    else
                    {
                        if (PUSH(ctxt, XML_PAT_OP_ELEM, name, null)) return;
                    }
                    return;
                }
                else if (strEqual(name, "attribute") != 0)
                {
                    name = null;
                    if (xsIdcSel(ctxt.comp.flags) != 0)
                    {
                        /* Unexpected attribute axis */
                        ctxt.error = 1;
                        return;
                    }
                    compileAttributeTest(ctxt);
                    return;
                }
                else
                {
                    /* The 'element' or 'attribute' axis is expected. */
                    ctxt.error = 1;
                    return;
                }
            }
        }
        else if (CUR(ctxt) == '*')
        {
            if (name != null)
            {
                ctxt.error = 1;
                return;
            }
            NEXT(ctxt);
            if (PUSH(ctxt, XML_PAT_OP_ALL, token, null)) return;
        }
        else
        {
            if (PUSH(ctxt, XML_PAT_OP_ELEM, name, null)) return;
        }
    }

    // xml_pattern_compile_path_pattern
    /**
     * Port of xmlCompilePathPattern.
     * Compile the Path Pattern and generates a precompiled
     * form suitable for fast matching.
     *
     * [5]    Path    ::=    ('.//')? ( Step '/' )* ( Step | '@' NameTest )
     */
    private static void compilePathPattern(PatParserContext ctxt)
    {
        SKIP_BLANKS(ctxt);
        if (CUR(ctxt) == '/')
        {
            ctxt.comp.flags |= PAT_FROM_ROOT;
        }
        else if ((CUR(ctxt) == '.') || (ctxt.comp.flags & XML_PATTERN_NOTPATTERN) != 0)
        {
            ctxt.comp.flags |= PAT_FROM_CUR;
        }

        if ((CUR(ctxt) == '/') && (NXT(ctxt, 1) == '/'))
        {
            if (PUSH(ctxt, XML_PAT_OP_ANCESTOR, null, null)) return;
            NEXT(ctxt);
            NEXT(ctxt);
        }
        else if ((CUR(ctxt) == '.') && (NXT(ctxt, 1) == '/') && (NXT(ctxt, 2) == '/'))
        {
            if (PUSH(ctxt, XML_PAT_OP_ANCESTOR, null, null)) return;
            NEXT(ctxt);
            NEXT(ctxt);
            NEXT(ctxt);
            /* Check for incompleteness. */
            SKIP_BLANKS(ctxt);
            if (CUR(ctxt) == 0)
            {
                /* Incomplete expression */
                ctxt.error = 1;
                return;
            }
        }
        if (CUR(ctxt) == '@')
        {
            NEXT(ctxt);
            compileAttributeTest(ctxt);
            if (ctxt.error != 0)
                return;
            SKIP_BLANKS(ctxt);
            /* TODO: check for incompleteness */
            if (CUR(ctxt) != 0)
            {
                compileStepPattern(ctxt);
                if (ctxt.error != 0)
                    return;
            }
        }
        else
        {
            if (CUR(ctxt) == '/')
            {
                if (PUSH(ctxt, XML_PAT_OP_ROOT, null, null)) return;
                NEXT(ctxt);
                /* Check for incompleteness. */
                SKIP_BLANKS(ctxt);
                if (CUR(ctxt) == 0)
                {
                    /* Incomplete expression */
                    ctxt.error = 1;
                    return;
                }
            }
            compileStepPattern(ctxt);
            if (ctxt.error != 0)
                return;
            SKIP_BLANKS(ctxt);
            while (CUR(ctxt) == '/')
            {
                if (NXT(ctxt, 1) == '/')
                {
                    if (PUSH(ctxt, XML_PAT_OP_ANCESTOR, null, null)) return;
                    NEXT(ctxt);
                    NEXT(ctxt);
                    SKIP_BLANKS(ctxt);
                    compileStepPattern(ctxt);
                    if (ctxt.error != 0)
                        return;
                }
                else
                {
                    if (PUSH(ctxt, XML_PAT_OP_PARENT, null, null)) return;
                    NEXT(ctxt);
                    SKIP_BLANKS(ctxt);
                    if (CUR(ctxt) == 0)
                    {
                        /* Incomplete expression */
                        ctxt.error = 1;
                        return;
                    }
                    compileStepPattern(ctxt);
                    if (ctxt.error != 0)
                        return;
                }
            }
        }
        if (CUR(ctxt) != 0)
        {
            /* Failed to compile pattern */
            ctxt.error = 1;
        }
    }

    // xml_pattern_compile_idc_xpath_path
    /**
     * Port of xmlCompileIDCXPathPath.
     * Compile the Path Pattern and generates a precompiled
     * form suitable for fast matching.
     *
     * [5]    Path    ::=    ('.//')? ( Step '/' )* ( Step | '@' NameTest )
     */
    private static void compileIdcXpathPath(PatParserContext ctxt)
    {
        SKIP_BLANKS(ctxt);
        if (CUR(ctxt) == '/')
        {
            /* Unexpected selection of the document root */
            ctxt.error = 1;
            return;
        }
        ctxt.comp.flags |= PAT_FROM_CUR;

        if (CUR(ctxt) == '.')
        {
            /* "." - "self::node()" */
            NEXT(ctxt);
            SKIP_BLANKS(ctxt);
            if (CUR(ctxt) == 0)
            {
                /*
                 * Selection of the context node.
                 */
                if (PUSH(ctxt, XML_PAT_OP_ELEM, null, null))
                {
                    ctxt.error = 1;
                    return;
                }
                return;
            }
            if (CUR(ctxt) != '/')
            {
                /* Unexpected token after '.' */
                ctxt.error = 1;
                return;
            }
            /* "./" - "self::node()/" */
            NEXT(ctxt);
            SKIP_BLANKS(ctxt);
            if (CUR(ctxt) == '/')
            {
                if (isBlankCh(PEEKPREV(ctxt, 1)))
                {
                    /*
                     * Disallow "./ /"
                     */
                    ctxt.error = 1;
                    return;
                }
                /* ".//" - "self:node()/descendant-or-self::node()/" */
                if (PUSH(ctxt, XML_PAT_OP_ANCESTOR, null, null))
                {
                    ctxt.error = 1;
                    return;
                }
                NEXT(ctxt);
                SKIP_BLANKS(ctxt);
            }
            if (CUR(ctxt) == 0)
            {
                /* error_unfinished */
                ctxt.error = 1;
                return;
            }
        }
        /*
         * Process steps.
         */
        do
        {
            compileStepPattern(ctxt);
            if (ctxt.error != 0)
            {
                ctxt.error = 1;
                return;
            }
            SKIP_BLANKS(ctxt);
            if (CUR(ctxt) != '/')
                break;
            if (PUSH(ctxt, XML_PAT_OP_PARENT, null, null))
            {
                ctxt.error = 1;
                return;
            }
            NEXT(ctxt);
            SKIP_BLANKS(ctxt);
            if (CUR(ctxt) == '/')
            {
                /*
                 * Disallow subsequent '//'.
                 */
                ctxt.error = 1;
                return;
            }
            if (CUR(ctxt) == 0)
            {
                /* error_unfinished */
                ctxt.error = 1;
                return;
            }

        } while (CUR(ctxt) != 0);

        if (CUR(ctxt) != 0)
        {
            /* Failed to compile expression */
            ctxt.error = 1;
        }
    }

    /* ***********************************************************************
     *                                                                      *
     *                      The streaming code                              *
     *                                                                      *
     ************************************************************************/

    // xml_stream_comp_new
    /**
     * Port of xmlNewStreamComp.
     * build a new compiled pattern for streaming
     */
    private static StreamComp streamCompNew(int size)
    {
        StreamComp cur;

        if (size < 4)
            size = 4;

        cur = new StreamComp();
        cur.steps = new StreamStep[size];
        for (int i = 0; i < size; i++) cur.steps[i] = new StreamStep();
        cur.nb_step = 0;
        cur.max_step = size;
        return cur;
    }

    // xml_stream_comp_free
    /**
     * Port of xmlFreeStreamComp.
     * Free the compiled pattern for streaming
     */
    private static void streamCompFree(StreamComp comp)
    {
    }

    // xml_stream_comp_add_step
    /**
     * Port of xmlStreamCompAddStep.
     * Add a new step to the compiled pattern
     * @returns -1 in case of error or the step index if successful
     */
    private static int streamCompAddStep(StreamComp comp, String name, String ns, int node_type, int flags)
    {
        StreamStep cur;

        if (comp.nb_step >= comp.max_step)
        {
            int new_size;

            new_size = growCapacity(comp.max_step, 24, 4, XML_PAT_MAX_ITEMS);
            if (new_size < 0)
            {
                return -1;
            }
            StreamStep[] tmp = new StreamStep[new_size];
            int i = 0;
            for (; i < comp.steps.length && i < new_size; i++) tmp[i] = comp.steps[i];
            for (; i < new_size; i++) tmp[i] = new StreamStep();
            comp.steps = tmp;
            comp.max_step = new_size;
        }
        cur = comp.steps[comp.nb_step++];
        cur.flags = flags;
        cur.name = name;
        cur.ns = ns;
        cur.node_type = node_type;
        return comp.nb_step - 1;
    }

    // xml_stream_compile
    /**
     * Port of xmlStreamCompile.
     * Tries to stream compile a pattern
     * @returns -1 in case of failure and 0 in case of success.
     */
    private static int streamCompile(XmlPattern comp)
    {
        StreamComp stream;
        int i, s = 0, root = 0, flags = 0, prevs = -1;
        StepOp step;

        if ((comp == null) || (comp.steps == null))
            return -1;
        /*
         * special case for .
         */
        if ((comp.nb_step == 1) &&
            (comp.steps[0].op == XML_PAT_OP_ELEM) &&
            (comp.steps[0].value == null) &&
            (comp.steps[0].value2 == null))
        {
            stream = streamCompNew(0);
            /* Note that the stream will have no steps in this case. */
            stream.flags |= XML_STREAM_FINAL_IS_ANY_NODE;
            comp.stream = stream;
            return 0;
        }

        stream = streamCompNew((comp.nb_step / 2) + 1);

        i = 0;
        if ((comp.flags & PAT_FROM_ROOT) != 0)
            stream.flags |= XML_STREAM_FROM_ROOT;

        for (; i < comp.nb_step; i++)
        {
            step = comp.steps[i];
            switch (step.op)
            {
            case XML_PAT_OP_END:
                break;
            case XML_PAT_OP_ROOT:
                if (i != 0)
                {
                    streamCompFree(stream);
                    return 0;
                }
                root = 1;
                break;
            case XML_PAT_OP_NS:
                s = streamCompAddStep(stream, null, step.value, XML_PAT_ELEMENT_NODE, flags);
                if (s < 0)
                {
                    streamCompFree(stream);
                    return 0;
                }
                prevs = s;
                flags = 0;
                break;
            case XML_PAT_OP_ATTR:
                flags |= XML_STREAM_STEP_ATTR;
                prevs = -1;
                s = streamCompAddStep(stream, step.value, step.value2, XML_PAT_ATTRIBUTE_NODE, flags);
                flags = 0;
                if (s < 0)
                {
                    streamCompFree(stream);
                    return 0;
                }
                break;
            case XML_PAT_OP_ELEM:
                if ((step.value == null) && (step.value2 == null))
                {
                    /*
                     * We have a "." or "self::node()" here.
                     * Eliminate redundant self::node() tests like in "/./."
                     * or "//./"
                     * The only case we won't eliminate is "//.", i.e. if
                     * self::node() is the last node test and we had
                     * continuation somewhere beforehand.
                     */
                    if ((comp.nb_step == i + 1) &&
                        (flags & XML_STREAM_STEP_DESC) != 0)
                    {
                        /*
                         * Mark the special case where the expression resolves
                         * to any type of node.
                         */
                        if (comp.nb_step == i + 1)
                        {
                            stream.flags |= XML_STREAM_FINAL_IS_ANY_NODE;
                        }
                        flags |= XML_STREAM_STEP_NODE;
                        s = streamCompAddStep(stream, null, null, XML_STREAM_ANY_NODE, flags);
                        if (s < 0)
                        {
                            streamCompFree(stream);
                            return 0;
                        }
                        flags = 0;
                        /*
                         * If there was a previous step, mark it to be added to
                         * the result node-set; this is needed since only
                         * the last step will be marked as "final" and only
                         * "final" nodes are added to the resulting set.
                         */
                        if (prevs != -1)
                        {
                            stream.steps[prevs].flags |= XML_STREAM_STEP_IN_SET;
                            prevs = -1;
                        }
                        break;
                    }
                    else
                    {
                        /* Just skip this one. */
                        continue;
                    }
                }
                /* An element node. */
                s = streamCompAddStep(stream, step.value, step.value2, XML_PAT_ELEMENT_NODE, flags);
                if (s < 0)
                {
                    streamCompFree(stream);
                    return 0;
                }
                prevs = s;
                flags = 0;
                break;
            case XML_PAT_OP_CHILD:
                /* An element node child. */
                s = streamCompAddStep(stream, step.value, step.value2, XML_PAT_ELEMENT_NODE, flags);
                if (s < 0)
                {
                    streamCompFree(stream);
                    return 0;
                }
                prevs = s;
                flags = 0;
                break;
            case XML_PAT_OP_ALL:
                s = streamCompAddStep(stream, null, null, XML_PAT_ELEMENT_NODE, flags);
                if (s < 0)
                {
                    streamCompFree(stream);
                    return 0;
                }
                prevs = s;
                flags = 0;
                break;
            case XML_PAT_OP_PARENT:
                break;
            case XML_PAT_OP_ANCESTOR:
                /* Skip redundant continuations. */
                if ((flags & XML_STREAM_STEP_DESC) != 0)
                    break;
                flags |= XML_STREAM_STEP_DESC;
                /*
                 * Mark the expression as having "//".
                 */
                if ((stream.flags & XML_STREAM_DESC) == 0)
                    stream.flags |= XML_STREAM_DESC;
                break;
            default:
                break;
            }
        }
        if ((root == 0) && (comp.flags & XML_PATTERN_NOTPATTERN) == 0)
        {
            /*
             * If this should behave like a real pattern, we will mark
             * the first step as having "//", to be reentrant on every
             * tree level.
             */
            if ((stream.flags & XML_STREAM_DESC) == 0)
                stream.flags |= XML_STREAM_DESC;

            if (stream.nb_step > 0)
            {
                if ((stream.steps[0].flags & XML_STREAM_STEP_DESC) == 0)
                    stream.steps[0].flags |= XML_STREAM_STEP_DESC;
            }
        }
        if (stream.nb_step <= s)
        {
            streamCompFree(stream);
            return 0;
        }
        stream.steps[s].flags |= XML_STREAM_STEP_FINAL;
        if (root != 0)
            stream.steps[0].flags |= XML_STREAM_STEP_ROOT;
        comp.stream = stream;
        return 0;
    }

    // xml_stream_ctxt_new
    /**
     * Port of xmlNewStreamCtxt.
     * build a new stream context
     */
    private static XmlStreamCtxt streamCtxtNew(StreamComp stream)
    {
        XmlStreamCtxt cur = new XmlStreamCtxt();
        cur.states = null;
        cur.nb_state = 0;
        cur.max_state = 0;
        cur.level = 0;
        cur.comp = stream;
        cur.block_level = -1;
        return cur;
    }

    // xml_stream_ctxt_free
    /**
     * Port of xmlFreeStreamCtxt.
     * Free the stream context
     * @param stream the stream context
     */
    public static void streamCtxtFree(XmlStreamCtxt stream)
    {
        XmlStreamCtxt next;

        while (stream != null)
        {
            next = stream.next;
            stream.states = null;
            stream = next;
        }
    }

    // xml_stream_ctxt_add_state
    /**
     * Port of xmlStreamCtxtAddState.
     * Add a new state to the stream context
     * @returns -1 in case of error or the state index if successful
     */
    private static int streamCtxtAddState(XmlStreamCtxt comp, int idx, int level)
    {
        int i;
        for (i = 0; i < comp.nb_state; i++)
        {
            if (comp.states[2 * i] < 0)
            {
                comp.states[2 * i] = idx;
                comp.states[2 * i + 1] = level;
                return i;
            }
        }
        if (comp.nb_state >= comp.max_state)
        {
            int new_size;

            new_size = growCapacity(comp.max_state, 8, 4, XML_PAT_MAX_ITEMS);
            if (new_size < 0)
            {
                return -1;
            }
            int[] tmp = new int[new_size * 2];
            if (comp.states != null)
                System.arraycopy(comp.states, 0, tmp, 0, Math.min(comp.states.length, tmp.length));
            comp.states = tmp;
            comp.max_state = new_size;
        }
        comp.states[2 * comp.nb_state] = idx;
        comp.states[2 * comp.nb_state++ + 1] = level;
        return comp.nb_state - 1;
    }

    /** The local/namespace-name compare of xmlStreamPushInternal. */
    private static int stepMatch(StreamStep step, String name, String ns)
    {
        int match = 0;
        if (step.node_type == XML_STREAM_ANY_NODE)
        {
            match = 1;
        }
        else if (step.name == null)
        {
            if (step.ns == null)
            {
                /*
                 * This lets through all elements/attributes.
                 */
                match = 1;
            }
            else if (ns != null)
                match = strEqual(step.ns, ns);
        }
        else if (((step.ns != null) == (ns != null)) &&
                 (name != null) &&
                 (c0(step.name) == c0(name)) &&
                 strEqual(step.name, name) != 0 &&
                 ((step.ns == ns) || strEqual(step.ns, ns) != 0))
        {
            match = 1;
        }
        return match;
    }

    // xml_stream_push_internal
    /**
     * Port of xmlStreamPushInternal.
     * Push new data onto the stream.
     * Both `name` and `ns` being NULL means the / i.e. the root of the document.
     * This can also act as a reset.
     * @returns -1 in case of error, 1 if the current state in the stream is a
     *    match and 0 otherwise.
     */
    private static int streamPushInternal(XmlStreamCtxt stream, String name, String ns, int node_type)
    {
        int ret = 0, fin = 0, tmp, i, m, match, step_nr, desc;
        StreamComp comp;
        StreamStep step;

        if ((stream == null) || (stream.nb_state < 0))
            return -1;

        while (stream != null)
        {
            comp = stream.comp;

            if ((node_type == XML_PAT_ELEMENT_NODE) &&
                (name == null) && (ns == null))
            {
                /* We have a document node here (or a reset). */
                stream.nb_state = 0;
                stream.level = 0;
                stream.block_level = -1;
                if ((comp.flags & XML_STREAM_FROM_ROOT) != 0)
                {
                    if (comp.nb_step == 0)
                    {
                        /* TODO: We have a "/." here? */
                        ret = 1;
                    }
                    else
                    {
                        if ((comp.nb_step == 1) &&
                            (comp.steps[0].node_type == XML_STREAM_ANY_NODE) &&
                            (comp.steps[0].flags & XML_STREAM_STEP_DESC) != 0)
                        {
                            /*
                             * In the case of "//." the document node will match
                             * as well.
                             */
                            ret = 1;
                        }
                        else if ((comp.steps[0].flags & XML_STREAM_STEP_ROOT) != 0)
                        {
                            if (streamCtxtAddState(stream, 0, 0) < 0)
                                return -1;
                        }
                    }
                }
                stream = stream.next;
                continue; /* while */
            }

            /*
             * Fast check for ".".
             */
            if (comp.nb_step == 0)
            {
                /*
                 * / and . are handled at the XPath node set creation
                 * level by checking min depth
                 */
                if ((stream.flags & XML_PATTERN_FLAG_XPATH) != 0)
                {
                    stream = stream.next;
                    continue; /* while */
                }
                /*
                 * For non-pattern like evaluation like XML Schema IDCs
                 * or traditional XPath expressions, this will match if
                 * we are at the first level only, otherwise on every level.
                 */
                if ((node_type != XML_PAT_ATTRIBUTE_NODE) &&
                    (((stream.flags & XML_PATTERN_NOTPATTERN) == 0) ||
                     (stream.level == 0)))
                {
                    ret = 1;
                }
                stream.level++;
                stream = stream.next; /* stream_next */
                continue;
            }
            if (stream.block_level != -1)
            {
                /*
                 * Skip blocked expressions.
                 */
                stream.level++;
                stream = stream.next; /* stream_next */
                continue;
            }

            if ((node_type != XML_PAT_ELEMENT_NODE) &&
                (node_type != XML_PAT_ATTRIBUTE_NODE) &&
                ((comp.flags & XML_STREAM_FINAL_IS_ANY_NODE) == 0))
            {
                /*
                 * No need to process nodes of other types if we don't
                 * resolve to those types.
                 * TODO: Do we need to block the context here?
                 */
                stream.level++;
                stream = stream.next; /* stream_next */
                continue;
            }

            /*
             * Check evolution of existing states
             */
            i = 0;
            m = stream.nb_state;
            while (i < m)
            {
                next_state:
                {
                    if ((comp.flags & XML_STREAM_DESC) == 0)
                    {
                        /*
                         * If there is no "//", then only the last
                         * added state is of interest.
                         */
                        step_nr = stream.states[2 * (stream.nb_state - 1)];
                        /*
                         * TODO: Security check, should not happen, remove it.
                         */
                        if (stream.states[(2 * (stream.nb_state - 1)) + 1] <
                            stream.level)
                        {
                            return -1;
                        }
                        desc = 0;
                        /* loop-stopper */
                        i = m;
                    }
                    else
                    {
                        /*
                         * If there are "//", then we need to process every "//"
                         * occurring in the states, plus any other state for this
                         * level.
                         */
                        step_nr = stream.states[2 * i];

                        /* TODO: should not happen anymore: dead states */
                        if (step_nr < 0)
                            break next_state;

                        tmp = stream.states[(2 * i) + 1];

                        /* skip new states just added */
                        if (tmp > stream.level)
                            break next_state;

                        /* skip states at ancestor levels, except if "//" */
                        desc = comp.steps[step_nr].flags & XML_STREAM_STEP_DESC;
                        if ((tmp < stream.level) && (desc == 0))
                            break next_state;
                    }
                    /*
                     * Check for correct node-type.
                     */
                    step = comp.steps[step_nr];
                    if (step.node_type != node_type)
                    {
                        if (step.node_type == XML_PAT_ATTRIBUTE_NODE)
                        {
                            /*
                             * Block this expression for deeper evaluation.
                             */
                            if ((comp.flags & XML_STREAM_DESC) == 0)
                                stream.block_level = stream.level + 1;
                            break next_state;
                        }
                        else if (step.node_type != XML_STREAM_ANY_NODE)
                            break next_state;
                    }
                    /*
                     * Compare local/namespace-name.
                     */
                    match = stepMatch(step, name, ns);
                    if (match != 0)
                    {
                        fin = step.flags & XML_STREAM_STEP_FINAL;
                        if (fin != 0)
                        {
                            ret = 1;
                        }
                        else if (streamCtxtAddState(stream, step_nr + 1, stream.level + 1) < 0)
                        {
                            return -1;
                        }
                        if ((ret != 1) && (step.flags & XML_STREAM_STEP_IN_SET) != 0)
                        {
                            /*
                             * Check if we have a special case like "foo/bar//.", where
                             * "foo" is selected as well.
                             */
                            ret = 1;
                        }
                    }
                    if (((comp.flags & XML_STREAM_DESC) == 0) &&
                        ((match == 0) || fin != 0))
                    {
                        /*
                         * Mark this expression as blocked for any evaluation at
                         * deeper levels. Note that this includes "/foo"
                         * expressions if the *pattern* behaviour is used.
                         */
                        stream.block_level = stream.level + 1;
                    }
                }
                /* next_state: */
                i++;
            }

            stream.level++;

            /*
             * Re/enter the expression.
             * Don't reenter if it's an absolute expression like "/foo",
             *   except "//foo".
             */
            step = comp.steps[0];
            if ((step.flags & XML_STREAM_STEP_ROOT) != 0)
            {
                stream = stream.next; /* stream_next */
                continue;
            }

            desc = step.flags & XML_STREAM_STEP_DESC;
            if ((stream.flags & XML_PATTERN_NOTPATTERN) != 0)
            {
                /*
                 * Re/enter the expression if it is a "descendant" one,
                 * or if we are at the 1st level of evaluation.
                 */
                boolean compare;
                if (stream.level == 1)
                {
                    /*
                     * XS-IDC: The missing "self::node()" will always
                     * match the first given node.
                     */
                    compare = xsIdc(stream.flags) == 0;
                }
                else if (desc != 0)
                {
                    /*
                     * A "//" is always reentrant.
                     */
                    compare = true;
                }
                else
                {
                    /*
                     * XS-IDC: Process the 2nd level, since the missing
                     * "self::node()" is responsible for the 2nd level being
                     * the real start level.
                     */
                    compare = (stream.level == 2) && xsIdc(stream.flags) != 0;
                }
                if (!compare)
                {
                    stream = stream.next; /* stream_next */
                    continue;
                }
            }

            /* compare: */
            /*
             * Check expected node-type.
             */
            if (step.node_type != node_type)
            {
                if (node_type == XML_PAT_ATTRIBUTE_NODE || step.node_type != XML_STREAM_ANY_NODE)
                {
                    stream = stream.next; /* stream_next */
                    continue;
                }
            }
            /*
             * Compare local/namespace-name.
             */
            match = stepMatch(step, name, ns);
            fin = step.flags & XML_STREAM_STEP_FINAL;
            if (match != 0)
            {
                if (fin != 0)
                {
                    ret = 1;
                }
                else if (streamCtxtAddState(stream, 1, stream.level) < 0)
                {
                    return -1;
                }
                if ((ret != 1) && (step.flags & XML_STREAM_STEP_IN_SET) != 0)
                {
                    /*
                     * Check if we have a special case like "foo//.", where
                     * "foo" is selected as well.
                     */
                    ret = 1;
                }
            }
            if (((comp.flags & XML_STREAM_DESC) == 0) &&
                ((match == 0) || fin != 0))
            {
                /*
                 * Mark this expression as blocked for any evaluation at
                 * deeper levels.
                 */
                stream.block_level = stream.level;
            }

            /* stream_next: */
            stream = stream.next;
        } /* while stream != NULL */

        return ret;
    }

    // xml_stream_push
    /**
     * Port of xmlStreamPush.
     * Push new data onto the stream.
     * Both `name` and `ns` being null means the / i.e. the root of the document.
     * This can also act as a reset.
     * Otherwise the function will act as if it has been given an element-node.
     * @param stream the stream context
     * @param name the current name
     * @param ns the namespace name
     * @return -1 in case of error, 1 if the current state in the stream is a
     *    match and 0 otherwise.
     */
    public static int streamPush(XmlStreamCtxt stream, String name, String ns)
    {
        return streamPushInternal(stream, name, ns, XML_PAT_ELEMENT_NODE);
    }

    // xml_stream_push_node
    /**
     * Port of xmlStreamPushNode.
     * Push new data onto the stream.
     * Both `name` and `ns` being null means the / i.e. the root of the document.
     * This can also act as a reset.
     * Different from streamPush this function can be fed with nodes of type:
     * element-, attribute-, text-, cdata-section-, comment- and
     * processing-instruction-node.
     * @param stream the stream context
     * @param name the current name
     * @param ns the namespace name
     * @param node_type the type of the node being pushed
     * @return -1 in case of error, 1 if the current state in the stream is a
     *    match and 0 otherwise.
     */
    public static int streamPushNode(XmlStreamCtxt stream, String name, String ns, int node_type)
    {
        return streamPushInternal(stream, name, ns, node_type);
    }

    // xml_stream_push_attr
    /**
     * Port of xmlStreamPushAttr.
     * Push new attribute data onto the stream.
     *
     * Both `name` and `ns` being null means the root of the document.
     * This can also act as a reset. Otherwise the function will act as
     * if it has been given an attribute-node.
     * @param stream the stream context
     * @param name the current name
     * @param ns the namespace name
     * @return -1 in case of error, 1 if the current state in the stream
     * is a match and 0 otherwise.
     */
    public static int streamPushAttr(XmlStreamCtxt stream, String name, String ns)
    {
        return streamPushInternal(stream, name, ns, XML_PAT_ATTRIBUTE_NODE);
    }

    // xml_stream_pop
    /**
     * Port of xmlStreamPop.
     * push one level from the stream.
     * @param stream the stream context
     * @return -1 in case of error, 0 otherwise.
     */
    public static int streamPop(XmlStreamCtxt stream)
    {
        int i, lev;

        if (stream == null)
            return -1;
        while (stream != null)
        {
            /*
             * Reset block-level.
             */
            if (stream.block_level == stream.level)
                stream.block_level = -1;

            /*
             *  stream->level can be zero when XML_FINAL_IS_ANY_NODE is set
             *  (see the thread at
             *  http://mail.gnome.org/archives/xslt/2008-July/msg00027.html)
             */
            if (stream.level != 0)
                stream.level--;
            /*
             * Check evolution of existing states
             */
            for (i = stream.nb_state - 1; i >= 0; i--)
            {
                /* discard obsoleted states */
                lev = stream.states[(2 * i) + 1];
                if (lev > stream.level)
                    stream.nb_state--;
                if (lev <= stream.level)
                    break;
            }
            stream = stream.next;
        }
        return 0;
    }

    // xml_stream_wants_any_node
    /**
     * Port of xmlStreamWantsAnyNode.
     * Query if the streaming pattern additionally needs to be fed with
     * text-, cdata-section-, comment- and processing-instruction-nodes.
     * If the result is 0 then only element-nodes and attribute-nodes
     * need to be pushed.
     * @param stream_ctxt the stream context
     * @return 1 in case of need of nodes of the above described types,
     *          0 otherwise. -1 on API errors.
     */
    public static int streamWantsAnyNode(XmlStreamCtxt stream_ctxt)
    {
        if (stream_ctxt == null)
            return -1;
        while (stream_ctxt != null)
        {
            if ((stream_ctxt.comp.flags & XML_STREAM_FINAL_IS_ANY_NODE) != 0)
                return 1;
            stream_ctxt = stream_ctxt.next;
        }
        return 0;
    }

    /* ***********************************************************************
     *                                                                      *
     *                      The public interfaces                           *
     *                                                                      *
     ************************************************************************/

    // xml_pattern_compile_safe
    /**
     * Port of xmlPatternCompileSafe.
     * Compile a pattern.
     * @param pattern the pattern to compile
     * @param flags compilation flags, see XML_PATTERN_*
     * @param namespaces the prefix definitions, array of [URI, prefix] or null
     * @param pattern_out output pattern (length 1)
     * @return 0 on success, 1 on error, -1 if a memory allocation failed.
     */
    public static int patternCompileSafe(String pattern, int flags, String[] namespaces, XmlPattern[] pattern_out)
    {
        XmlPattern ret = null, cur;
        PatParserContext ctxt = null;
        int orp, start;
        int type = 0;
        int streamable = 1;
        int error;

        if (pattern_out == null)
            return 1;

        if (pattern == null)
        {
            error = 1;
            pattern_out[0] = null;
            return error;
        }

        /* The C string ends at the first NUL. */
        byte[] all = pattern.getBytes(StandardCharsets.UTF_8);
        int n = 0;
        while (n < all.length && all[n] != 0) n++;
        byte[] pat = new byte[n + 1];
        System.arraycopy(all, 0, pat, 0, n);

        start = 0;
        orp = start;
        while (pat[orp] != 0)
        {
            while ((pat[orp] != 0) && (pat[orp] != '|')) orp++;
            byte[] part = new byte[orp - start + 1];
            System.arraycopy(pat, start, part, 0, orp - start);
            ctxt = newParserContext(part, pattern, namespaces);
            if (pat[orp] != 0)
                orp++;
            cur = patternNew();
            if (ret == null)
                ret = cur;
            else
            {
                cur.next = ret.next;
                ret.next = cur;
            }
            cur.flags = flags;
            ctxt.comp = cur;

            if (xsIdc(cur.flags) != 0)
                compileIdcXpathPath(ctxt);
            else
                compilePathPattern(ctxt);
            if (ctxt.error != 0)
            {
                error = ctxt.error;
                patternFree(ret);
                pattern_out[0] = null;
                return error;
            }
            freeParserContext(ctxt);
            ctxt = null;

            if (streamable != 0)
            {
                if (type == 0)
                {
                    type = cur.flags & (PAT_FROM_ROOT | PAT_FROM_CUR);
                }
                else if (type == PAT_FROM_ROOT)
                {
                    if ((cur.flags & PAT_FROM_CUR) != 0)
                        streamable = 0;
                }
                else if (type == PAT_FROM_CUR)
                {
                    if ((cur.flags & PAT_FROM_ROOT) != 0)
                        streamable = 0;
                }
            }
            if (streamable != 0)
            {
                error = streamCompile(cur);
                if (error != 0)
                {
                    patternFree(ret);
                    pattern_out[0] = null;
                    return error;
                }
            }
            error = patternReverse(cur);
            if (error != 0)
            {
                patternFree(ret);
                pattern_out[0] = null;
                return error;
            }
            start = orp;
        }
        if (streamable == 0)
        {
            cur = ret;
            while (cur != null)
            {
                if (cur.stream != null)
                {
                    streamCompFree(cur.stream);
                    cur.stream = null;
                }
                cur = cur.next;
            }
        }

        pattern_out[0] = ret;
        return 0;
    }

    // xml_pattern_compile
    /**
     * Port of xmlPatterncompile.
     * Compile a pattern.
     * @param pattern the pattern to compile
     * @param flags compilation flags, see XML_PATTERN_*
     * @param namespaces the prefix definitions, array of [URI, prefix] or null
     * @return the compiled form of the pattern or null in case of error
     */
    public static XmlPattern patternCompile(String pattern, int flags, String[] namespaces)
    {
        XmlPattern[] ret = new XmlPattern[1];
        patternCompileSafe(pattern, flags, namespaces, ret);
        return ret[0];
    }

    // xml_pattern_match
    /**
     * Port of xmlPatternMatch.
     * Test whether the node matches the pattern
     * @param comp the precompiled pattern
     * @param node a node
     * @return 1 if it matches, 0 if it doesn't and -1 in case of failure
     */
    public static int patternMatch(XmlPattern comp, XmlNode node)
    {
        int ret = 0;

        if ((comp == null) || (node == null))
            return -1;

        while (comp != null)
        {
            ret = patMatch(comp, node);
            if (ret != 0)
                return ret;
            comp = comp.next;
        }
        return ret;
    }

    // xml_pattern_get_stream_ctxt
    /**
     * Port of xmlPatternGetStreamCtxt.
     * Get a streaming context for that pattern
     * Use streamCtxtFree to free the context.
     * @param comp the precompiled pattern
     * @return the context or null in case of failure
     */
    public static XmlStreamCtxt patternGetStreamCtxt(XmlPattern comp)
    {
        XmlStreamCtxt ret = null, cur;

        if ((comp == null) || (comp.stream == null))
            return null;

        while (comp != null)
        {
            if (comp.stream == null)
            {
                streamCtxtFree(ret);
                return null;
            }
            cur = streamCtxtNew(comp.stream);
            if (ret == null)
                ret = cur;
            else
            {
                cur.next = ret.next;
                ret.next = cur;
            }
            cur.flags = comp.flags;
            comp = comp.next;
        }
        return ret;
    }

    // xml_pattern_streamable
    /**
     * Port of xmlPatternStreamable.
     * Check if the pattern is streamable i.e. patternGetStreamCtxt should work.
     * @param comp the precompiled pattern
     * @return 1 if streamable, 0 if not and -1 in case of error.
     */
    public static int patternStreamable(XmlPattern comp)
    {
        if (comp == null)
            return -1;
        while (comp != null)
        {
            if (comp.stream == null)
                return 0;
            comp = comp.next;
        }
        return 1;
    }

    // xml_pattern_max_depth
    /**
     * Port of xmlPatternMaxDepth.
     * Check the maximum depth reachable by a pattern
     * @param comp the precompiled pattern
     * @return -2 if no limit (using //), otherwise the depth,
     *         and -1 in case of error
     */
    public static int patternMaxDepth(XmlPattern comp)
    {
        int ret = 0, i;
        if (comp == null)
            return -1;
        while (comp != null)
        {
            if (comp.stream == null)
                return -1;
            for (i = 0; i < comp.stream.nb_step; i++)
                if ((comp.stream.steps[i].flags & XML_STREAM_STEP_DESC) != 0)
                    return -2;
            if (comp.stream.nb_step > ret)
                ret = comp.stream.nb_step;
            comp = comp.next;
        }
        return ret;
    }

    // xml_pattern_min_depth
    /**
     * Port of xmlPatternMinDepth.
     * Check the minimum depth reachable by a pattern, 0 mean the / or . are
     * part of the set.
     * @param comp the precompiled pattern
     * @return -1 in case of error otherwise the depth,
     */
    public static int patternMinDepth(XmlPattern comp)
    {
        int ret = 12345678;
        if (comp == null)
            return -1;
        while (comp != null)
        {
            if (comp.stream == null)
                return -1;
            if (comp.stream.nb_step < ret)
                ret = comp.stream.nb_step;
            if (ret == 0)
                return 0;
            comp = comp.next;
        }
        return ret;
    }

    // xml_pattern_from_root
    /**
     * Port of xmlPatternFromRoot.
     * Check if the pattern must be looked at from the root.
     * @param comp the precompiled pattern
     * @return 1 if true, 0 if false and -1 in case of error
     */
    public static int patternFromRoot(XmlPattern comp)
    {
        if (comp == null)
            return -1;
        while (comp != null)
        {
            if (comp.stream == null)
                return -1;
            if ((comp.flags & PAT_FROM_ROOT) != 0)
                return 1;
            comp = comp.next;
        }
        return 0;
    }
}
