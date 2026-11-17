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

/* Portions of this file are ported from libxml2 (xmlregexp.c, version 2.16.0 as found
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

import java.io.PrintStream;
import java.nio.charset.StandardCharsets;
import java.util.Arrays;

/**
 * XML Schemas (Part 2, Appendix F) regular expressions, the automata used to build
 * content models and their progressive execution, a port of xml_regexp.c (libxml2
 * xmlregexp.c: xmlRegexp*, xmlAutomata* and xmlRegExec*). An instance is a compiled
 * regular expression or automata (libxml2 xmlRegexp), all functions are static.
 */
@SuppressWarnings("fallthrough")
public final class XmlRegexp
{
    /*
     * regexp.c: generic and extensible Regular Expression engine
     *
     * Basically designed with the purpose of compiling regexps for
     * the variety of validation/schemas mechanisms now available in
     * XML related specifications these include:
     *    - XML-1.0 DTD validation
     *    - XML Schemas structure part 1
     *    - XML Schemas Datatypes part 2 especially Appendix F
     *    - RELAX-NG/TREX i.e. the counter proposal
     *
     * See Copyright for the status of this software.
     *
     * Author: Daniel Veillard
     */

    /**
     * Receives each compile error (XmlRegexpErrorFunc), message is the libxml2 error message
     * ("failed to compile: ...\n"), regexp the expression (null for automata) and
     * index the byte offset in it where the error was found.
     */
    public interface ErrorFunc
    {
        /**
         * Called for each compile error.
         * @param data the error data
         * @param message the message
         * @param regexp the regexp or null
         * @param index the byte offset of the error
         */
        void error(Object data, String message, String regexp, int index);
    }

    /**
     * Called when a transition is taken by regExecPushString(2) (XmlRegExecCallbacks).
     * The C callback receives the data given to xml_reg_exec_new as first argument,
     * here the exec context is passed, its data is in exec.data.
     */
    public interface ExecCallback
    {
        /**
         * Called when a transition is taken.
         * @param exec the exec context (exec.data is the C first argument)
         * @param token the transition token
         * @param transdata the data of the transition
         * @param inputdata the data pushed with the token
         */
        void callback(XmlRegExecCtxt exec, String token, Object transdata, Object inputdata);
    }

    /* The exec status and error return values (libxml2 private/regexp.h). */
    /** No error. */
    public static final int XML_REGEXP_OK = 0;
    /** Not found. */
    public static final int XML_REGEXP_NOT_FOUND = -1;
    /** Internal error. */
    public static final int XML_REGEXP_INTERNAL_ERROR = -4;
    /** Out of memory. */
    public static final int XML_REGEXP_OUT_OF_MEMORY = -5;
    /** Internal limit reached. */
    public static final int XML_REGEXP_INTERNAL_LIMIT = -6;
    /** Invalid UTF-8. */
    public static final int XML_REGEXP_INVALID_UTF8 = -7;

    /** The automata flag used by RELAX NG (libxml2 AM_AUTOMATA_RNG): string atoms are
        compared by pointer when computing determinism. */
    public static final int XML_AUTOMATA_RNG = 1;

    /* The libxml2 XML_MAX_ITEMS (private/memory.h). */
    private static final int XML_MAX_ITEMS = 1000000000; /* 1 billion */

    /* The ctxt->error values, the libxml2 XML_ERR_NO_MEMORY and XML_REGEXP_COMPILE_ERROR. */
    private static final int XML_REGEXP_ERR_NO_MEMORY = 2;
    private static final int XML_REGEXP_ERR_COMPILE = 1450;

    private static final int MAX_PUSH = 10000000;

    private static final int XML_REG_STRING_SEPARATOR = '|';

    private static final int AM_AUTOMATA_RNG = XML_AUTOMATA_RNG;

    private static final int REGEXP_ALL_COUNTER = 0x123456;
    private static final int REGEXP_ALL_LAX_COUNTER = 0x123457;

    private static final int INT_MAX = Integer.MAX_VALUE;

    /*
     * Note: the order of the enums below is significant, do not shuffle
     */
    // XmlRegAtomType
    static final int XML_REGEXP_EPSILON = 1;
    static final int XML_REGEXP_CHARVAL = 2;
    static final int XML_REGEXP_RANGES = 3;
    static final int XML_REGEXP_SUBREG = 4;  /* used for () sub regexps */
    static final int XML_REGEXP_STRING = 5;
    static final int XML_REGEXP_ANYCHAR = 6; /* . */
    static final int XML_REGEXP_ANYSPACE = 7; /* \s */
    static final int XML_REGEXP_NOTSPACE = 8; /* \S */
    static final int XML_REGEXP_INITNAME = 9; /* \l */
    static final int XML_REGEXP_NOTINITNAME = 10; /* \L */
    static final int XML_REGEXP_NAMECHAR = 11; /* \c */
    static final int XML_REGEXP_NOTNAMECHAR = 12; /* \C */
    static final int XML_REGEXP_DECIMAL = 13; /* \d */
    static final int XML_REGEXP_NOTDECIMAL = 14; /* \D */
    static final int XML_REGEXP_REALCHAR = 15; /* \w */
    static final int XML_REGEXP_NOTREALCHAR = 16; /* \W */
    static final int XML_REGEXP_LETTER = 100;
    static final int XML_REGEXP_LETTER_UPPERCASE = 101;
    static final int XML_REGEXP_LETTER_LOWERCASE = 102;
    static final int XML_REGEXP_LETTER_TITLECASE = 103;
    static final int XML_REGEXP_LETTER_MODIFIER = 104;
    static final int XML_REGEXP_LETTER_OTHERS = 105;
    static final int XML_REGEXP_MARK = 106;
    static final int XML_REGEXP_MARK_NONSPACING = 107;
    static final int XML_REGEXP_MARK_SPACECOMBINING = 108;
    static final int XML_REGEXP_MARK_ENCLOSING = 109;
    static final int XML_REGEXP_NUMBER = 110;
    static final int XML_REGEXP_NUMBER_DECIMAL = 111;
    static final int XML_REGEXP_NUMBER_LETTER = 112;
    static final int XML_REGEXP_NUMBER_OTHERS = 113;
    static final int XML_REGEXP_PUNCT = 114;
    static final int XML_REGEXP_PUNCT_CONNECTOR = 115;
    static final int XML_REGEXP_PUNCT_DASH = 116;
    static final int XML_REGEXP_PUNCT_OPEN = 117;
    static final int XML_REGEXP_PUNCT_CLOSE = 118;
    static final int XML_REGEXP_PUNCT_INITQUOTE = 119;
    static final int XML_REGEXP_PUNCT_FINQUOTE = 120;
    static final int XML_REGEXP_PUNCT_OTHERS = 121;
    static final int XML_REGEXP_SEPAR = 122;
    static final int XML_REGEXP_SEPAR_SPACE = 123;
    static final int XML_REGEXP_SEPAR_LINE = 124;
    static final int XML_REGEXP_SEPAR_PARA = 125;
    static final int XML_REGEXP_SYMBOL = 126;
    static final int XML_REGEXP_SYMBOL_MATH = 127;
    static final int XML_REGEXP_SYMBOL_CURRENCY = 128;
    static final int XML_REGEXP_SYMBOL_MODIFIER = 129;
    static final int XML_REGEXP_SYMBOL_OTHERS = 130;
    static final int XML_REGEXP_OTHER = 131;
    static final int XML_REGEXP_OTHER_CONTROL = 132;
    static final int XML_REGEXP_OTHER_FORMAT = 133;
    static final int XML_REGEXP_OTHER_PRIVATE = 134;
    static final int XML_REGEXP_OTHER_NA = 135;
    static final int XML_REGEXP_BLOCK_NAME = 136;

    // XmlRegQuantType
    static final int XML_REGEXP_QUANT_EPSILON = 1;
    static final int XML_REGEXP_QUANT_ONCE = 2;
    static final int XML_REGEXP_QUANT_OPT = 3;
    static final int XML_REGEXP_QUANT_MULT = 4;
    static final int XML_REGEXP_QUANT_PLUS = 5;
    static final int XML_REGEXP_QUANT_ONCEONLY = 6;
    static final int XML_REGEXP_QUANT_ALL = 7;
    static final int XML_REGEXP_QUANT_RANGE = 8;

    // XmlRegStateType
    static final int XML_REGEXP_START_STATE = 1;
    static final int XML_REGEXP_FINAL_STATE = 2;
    static final int XML_REGEXP_TRANS_STATE = 3;
    static final int XML_REGEXP_SINK_STATE = 4;
    static final int XML_REGEXP_UNREACH_STATE = 5;

    // XmlRegMarkedType
    static final int XML_REGEXP_MARK_NORMAL = 0;
    static final int XML_REGEXP_MARK_START = 1;
    static final int XML_REGEXP_MARK_VISITED = 2;

    // struct XmlRegRange
    static final class RegRange
    {
        int neg;		/* 0 normal, 1 not, 2 exclude */
        int type;
        int start;
        int end;
        String block_name;
    }

    // struct XmlRegAtom
    static final class RegAtom
    {
        int no;
        int type;
        int quant;
        int min;
        int max;

        Object valuep;
        Object valuep2;
        int neg;
        int codepoint;
        XmlAutomataState start;
        XmlAutomataState start0;
        XmlAutomataState stop;
        int max_ranges;
        int nb_ranges;
        RegRange[] ranges;
        Object data;
    }

    // struct XmlRegCounter
    static final class RegCounter
    {
        int min;
        int max;
    }

    // struct XmlRegTrans
    static final class RegTrans
    {
        RegAtom atom;
        int to;
        int counter;
        int count;
        int nd;
    }

    // struct XmlRegExecRollback
    static final class RegExecRollback
    {
        XmlAutomataState state; /* the current state */
        int index;		/* the index in the input stack */
        int nextbranch;	/* the next transition to explore in that state */
        int[] counts;	/* save the automata state if it has some */
    }

    // struct XmlRegInputToken
    static final class RegInputToken
    {
        String value;
        Object data;
    }

    // struct XmlRegexp
    byte[] string;
    int nb_states;
    XmlAutomataState[] states;
    int nb_atoms;
    RegAtom[] atoms;
    int nb_counters;
    RegCounter[] counters;
    int determinist;
    int flags;
    /*
     * That's the compact form for determinists automatas
     */
    int nbstates;
    int[] compact;
    Object[] transdata;
    int nbstrings;
    String[] string_map;

    XmlRegexp()
    {
    }

    /* ***********************************************************************
     *									*
     *	String, memory and character helpers (libxml2 xmlstring.c,	*
     *	private/memory.h and chvalid.h)					*
     *									*
     ************************************************************************/

    /* The byte at i of a C string, 0 at and past the end (the terminating 0). */
    private static int at(byte[] s, int i)
    {
        if (s == null || i < 0 || i >= s.length) return 0;
        return s[i] & 0xff;
    }

    /* The char at i of a C string, 0 at and past the end (the terminating 0). */
    private static int at(String s, int i)
    {
        if (i < 0 || i >= s.length()) return 0;
        return s.charAt(i);
    }

    /* The UTF-8 bytes of a C string: the Java string up to its first 0. */
    private static byte[] toUtf8(String s)
    {
        int z = s.indexOf('\0');
        if (z >= 0) s = s.substring(0, z);
        return s.getBytes(StandardCharsets.UTF_8);
    }

    // xml_reg_strdup
    /* Port of xmlStrdup. A new String object, its identity is compared like the C pointer. */
    private static String regStrdup(String cur)
    {
        if (cur == null)
            return null;
        return new String(cur);
    }

    // xml_reg_strndup
    /* Port of xmlStrndup. */
    private static String regStrndup(byte[] cur, int off, int len)
    {
        if ((cur == null) || (len < 0))
            return null;
        return new String(cur, off, len, StandardCharsets.UTF_8);
    }

    // xml_reg_str_equal
    /* Port of xmlStrEqual. */
    private static int regStrEqual(String str1, String str2)
    {
        if (str1 == str2) return 1;
        if (str1 == null) return 0;
        if (str2 == null) return 0;
        return str1.equals(str2) ? 1 : 0;
    }

    // xml_reg_strchr
    /* Port of xmlStrchr, returns 1 if found. */
    private static boolean regStrchr(String str, char val)
    {
        if (str == null) return false;
        int z = str.indexOf('\0');
        int i = str.indexOf(val);
        return i >= 0 && (z < 0 || i < z);
    }

    // xml_reg_grow_capacity
    /* Port of xmlGrowCapacity (private/memory.h). */
    private static int regGrowCapacity(int capacity, int min, int max)
    {
        int extra;

        if (capacity <= 0)
        {
            return min;
        }

        if (capacity >= max)
            return -1;

        /* Grow by 50% */
        extra = (capacity + 1) / 2;

        if (capacity > max - extra)
            return max;

        return capacity + extra;
    }

    // xml_reg_get_utf8_char
    /* Port of xmlGetUTF8Char, str[off..] is read as a C string. */
    private static int regGetUtf8Char(byte[] str, int off, int[] len)
    {
        int c;

        if (str == null)
            return error8(len);
        if (len == null)
            return error8(len);

        c = at(str, off);
        if (c < 0x80)
        {
            if (len[0] < 1)
                return error8(len);
            /* 1-byte code */
            len[0] = 1;
        }
        else
        {
            if ((len[0] < 2) || ((at(str, off + 1) & 0xc0) != 0x80))
                return error8(len);
            if (c < 0xe0)
            {
                if (c < 0xc2)
                    return error8(len);
                /* 2-byte code */
                len[0] = 2;
                c = (c & 0x1f) << 6;
                c |= at(str, off + 1) & 0x3f;
            }
            else
            {
                if ((len[0] < 3) || ((at(str, off + 2) & 0xc0) != 0x80))
                    return error8(len);
                if (c < 0xf0)
                {
                    /* 3-byte code */
                    len[0] = 3;
                    c = (c & 0xf) << 12;
                    c |= (at(str, off + 1) & 0x3f) << 6;
                    c |= at(str, off + 2) & 0x3f;
                    if ((c < 0x800) || ((c >= 0xd800) && (c < 0xe000)))
                        return error8(len);
                }
                else
                {
                    if ((len[0] < 4) || ((at(str, off + 3) & 0xc0) != 0x80))
                        return error8(len);
                    len[0] = 4;
                    /* 4-byte code */
                    c = (c & 0x7) << 18;
                    c |= (at(str, off + 1) & 0x3f) << 12;
                    c |= (at(str, off + 2) & 0x3f) << 6;
                    c |= at(str, off + 3) & 0x3f;
                    if ((c < 0x10000) || (c >= 0x110000))
                        return error8(len);
                }
            }
        }
        return c;
    }

    private static int error8(int[] len)
    {
        if (len != null)
            len[0] = 0;
        return -1;
    }

    // xml_reg_is_char
    /* Port of xmlIsCharQ (IS_CHAR), the XML 1.0 Char production. */
    private static boolean regIsChar(int c)
    {
        if (c < 0x100)
            return ((0x9 <= c) && (c <= 0xa)) || (c == 0xd) || (0x20 <= c);
        return ((0x100 <= c) && (c <= 0xd7ff)) ||
               ((0xe000 <= c) && (c <= 0xfffd)) ||
               ((0x10000 <= c) && (c <= 0x10ffff));
    }

    /* The parser macros of xmlregexp.c. */

    // CUR
    private static int CUR(XmlAutomata ctxt)
    {
        return at(ctxt.string, ctxt.cur);
    }

    // NXT
    private static int NXT(XmlAutomata ctxt, int index)
    {
        if (ctxt.cur + index < ctxt.len)
            return at(ctxt.string, ctxt.cur + index);
        return 0;
    }

    // NEXT
    private static void NEXT(XmlAutomata ctxt)
    {
        ctxt.cur++;
    }

    // NEXTL
    private static void NEXTL(XmlAutomata ctxt, int l)
    {
        ctxt.cur += l;
    }

    // PREV
    private static int PREV(XmlAutomata ctxt)
    {
        return at(ctxt.string, ctxt.cur - 1);
    }

    // ERROR
    private static void ERROR(XmlAutomata ctxt, String str)
    {
        ctxt.error = XML_REGEXP_ERR_COMPILE;
        regexpErrCompile(ctxt, str);
    }

    /* ***********************************************************************
     *									*
     *		Regexp memory error handler				*
     *									*
     ************************************************************************/

    // xml_regexp_err_memory
    /*
     * Port of xmlRegexpErrMemory.
     * Handle an out of memory condition
     */
    private static void regexpErrMemory(XmlAutomata ctxt)
    {
        if (ctxt != null)
            ctxt.error = XML_REGEXP_ERR_NO_MEMORY;
    }

    // xml_regexp_err_compile
    /*
     * Port of xmlRegexpErrCompile.
     *
     * Handle a compilation failure, libxml2 raises the error (domain
     * XML_FROM_REGEXP, code XML_REGEXP_COMPILE_ERROR, str1 extra, str2 the
     * regexp, int1 the index) with the message "failed to compile: %s\n",
     * here it goes to the error function of the context.
     */
    private static void regexpErrCompile(XmlAutomata ctxt, String extra)
    {
        String regexp = null;
        int idx = 0;

        if (ctxt != null)
        {
            if (ctxt.string != null)
                regexp = new String(ctxt.string, StandardCharsets.UTF_8);
            if (ctxt.string != null)
                idx = ctxt.cur;
            ctxt.error = XML_REGEXP_ERR_COMPILE;
        }

        if ((ctxt != null) && (ctxt.error_func != null))
        {
            String msg = "failed to compile: " + extra + "\n";
            ctxt.error_func.error(ctxt.error_data, msg, regexp, idx);
        }
    }

    /* ***********************************************************************
     *									*
     *			Allocation/Deallocation				*
     *									*
     ************************************************************************/

    // xml_reg_calloc2
    /*
     * Port of xmlRegCalloc2.
     * Allocate a two-dimensional array and set all elements to zero,
     * returns the number of elements or -1 in case of error.
     */
    private static int regCalloc2(long dim1, long dim2)
    {
        long num_elems;

        /* Check for overflow */
        if (dim2 == 0)
            return -1;
        num_elems = dim1 * dim2;
        if (num_elems > XML_MAX_ITEMS)
            return -1;
        return (int) num_elems;
    }

    // xml_reg_epx_from_parse
    /*
     * Port of xmlRegEpxFromParse.
     * Allocate a new regexp and fill it with the result from the parser
     */
    private static XmlRegexp regEpxFromParse(XmlAutomata ctxt)
    {
        XmlRegexp ret;

        ret = new XmlRegexp();
        ret.string = ctxt.string;
        ret.nb_states = ctxt.nb_states;
        ret.states = ctxt.states;
        ret.nb_atoms = ctxt.nb_atoms;
        ret.atoms = ctxt.atoms;
        ret.nb_counters = ctxt.nb_counters;
        ret.counters = ctxt.counters;
        ret.determinist = ctxt.determinist;
        ret.flags = ctxt.flags;
        if (ret.determinist == -1)
        {
            if (regexpIsDeterminist(ret) < 0)
            {
                regexpErrMemory(ctxt);
                return null;
            }
        }

        if ((ret.determinist != 0) &&
            (ret.nb_counters == 0) &&
            (ctxt.negs == 0) &&
            (ret.atoms != null) &&
            (ret.atoms[0] != null) &&
            (ret.atoms[0].type == XML_REGEXP_STRING))
        {
            int i, j, nbstates = 0, nbatoms = 0;
            int[] state_remap;
            int[] string_remap;
            int[] transitions;
            Object[] transdata;
            String[] string_map;
            String value;

            /*
             * Switch to a compact representation
             * 1/ counting the effective number of states left
             * 2/ counting the unique number of atoms, and check that
             *    they are all of the string type
             * 3/ build a table state x atom for the transitions
             */

            state_remap = new int[ret.nb_states];
            for (i = 0; i < ret.nb_states; i++)
            {
                if (ret.states[i] != null)
                {
                    state_remap[i] = nbstates;
                    nbstates++;
                }
                else
                {
                    state_remap[i] = -1;
                }
            }
            string_map = new String[ret.nb_atoms];
            string_remap = new int[ret.nb_atoms];
            for (i = 0; i < ret.nb_atoms; i++)
            {
                if ((ret.atoms[i].type == XML_REGEXP_STRING) &&
                    (ret.atoms[i].quant == XML_REGEXP_QUANT_ONCE))
                {
                    value = (String) ret.atoms[i].valuep;
                    for (j = 0; j < nbatoms; j++)
                    {
                        if (regStrEqual(string_map[j], value) != 0)
                        {
                            string_remap[i] = j;
                            break;
                        }
                    }
                    if (j >= nbatoms)
                    {
                        string_remap[i] = nbatoms;
                        string_map[nbatoms] = regStrdup(value);
                        if (string_map[nbatoms] == null)
                        {
                            /* C: strdup(NULL) fails, the regexp is lost. */
                            return null;
                        }
                        nbatoms++;
                    }
                }
                else
                {
                    return null;
                }
            }
            int nbt = regCalloc2(nbstates + 1, nbatoms + 1);
            if (nbt < 0)
            {
                return null;
            }
            transitions = new int[nbt];

            /*
             * Allocate the transition table. The first entry for each
             * state corresponds to the state type.
             */
            transdata = null;

            for (i = 0; i < ret.nb_states; i++)
            {
                int stateno, atomno, targetno, prev;
                XmlAutomataState state;
                RegTrans trans;

                stateno = state_remap[i];
                if (stateno == -1)
                    continue;
                state = ret.states[i];

                transitions[stateno * (nbatoms + 1)] = state.type;

                for (j = 0; j < state.nb_trans; j++)
                {
                    trans = state.trans[j];
                    if ((trans.to < 0) || (trans.atom == null))
                        continue;
                    atomno = string_remap[trans.atom.no];
                    if ((trans.atom.data != null) && (transdata == null))
                    {
                        int nbd = regCalloc2(nbstates, nbatoms);
                        if (nbd < 0)
                        {
                            regexpErrMemory(ctxt);
                            break;
                        }
                        transdata = new Object[nbd];
                    }
                    targetno = state_remap[trans.to];
                    /*
                     * if the same atom can generate transitions to 2 different
                     * states then it means the automata is not deterministic and
                     * the compact form can't be used !
                     */
                    prev = transitions[stateno * (nbatoms + 1) + atomno + 1];
                    if (prev != 0)
                    {
                        if (prev != targetno + 1)
                        {
                            ret.determinist = 0;
                            return notDeterm(ctxt, ret);
                        }
                    }
                    else
                    {
                        transitions[stateno * (nbatoms + 1) + atomno + 1] =
                            targetno + 1; /* to avoid 0 */
                        if (transdata != null)
                            transdata[stateno * nbatoms + atomno] =
                                trans.atom.data;
                    }
                }
            }
            ret.determinist = 1;
            /*
             * Cleanup of the old data
             */
            ret.states = null;
            ret.nb_states = 0;
            ret.atoms = null;
            ret.nb_atoms = 0;

            ret.compact = transitions;
            ret.transdata = transdata;
            ret.string_map = string_map;
            ret.nbstrings = nbatoms;
            ret.nbstates = nbstates;
        }
        return notDeterm(ctxt, ret);
    }

    /* The not_determ: label of xml_reg_epx_from_parse. */
    private static XmlRegexp notDeterm(XmlAutomata ctxt, XmlRegexp ret)
    {
        ctxt.string = null;
        ctxt.nb_states = 0;
        ctxt.states = null;
        ctxt.nb_atoms = 0;
        ctxt.atoms = null;
        ctxt.nb_counters = 0;
        ctxt.counters = null;
        return ret;
    }

    // xml_reg_new_parser_ctxt
    /*
     * Port of xmlRegNewParserCtxt.
     * Allocate a new regexp parser context
     */
    private static XmlAutomata regNewParserCtxt(String string)
    {
        XmlAutomata ret;

        ret = new XmlAutomata();
        if (string != null)
        {
            ret.string = toUtf8(string);
            ret.len = ret.string.length;
        }
        ret.cur = 0;
        ret.neg = 0;
        ret.negs = 0;
        ret.error = 0;
        ret.determinist = -1;
        return ret;
    }

    // xml_reg_new_range
    /*
     * Port of xmlRegNewRange.
     * Allocate a new regexp range
     */
    private static RegRange regNewRange(XmlAutomata ctxt, int neg, int type, int start, int end)
    {
        RegRange ret;

        ret = new RegRange();
        ret.neg = neg;
        ret.type = type;
        ret.start = start;
        ret.end = end;
        return ret;
    }

    // xml_reg_free_range
    /* Port of xmlRegFreeRange. */
    private static void regFreeRange(RegRange range)
    {
    }

    // xml_reg_copy_range
    /*
     * Port of xmlRegCopyRange.
     * Copy a regexp range
     */
    private static RegRange regCopyRange(XmlAutomata ctxt, RegRange range)
    {
        RegRange ret;

        if (range == null)
            return null;

        ret = regNewRange(ctxt, range.neg, range.type, range.start, range.end);
        if (range.block_name != null)
        {
            ret.block_name = regStrdup(range.block_name);
        }
        return ret;
    }

    // xml_reg_new_atom
    /*
     * Port of xmlRegNewAtom.
     * Allocate a new atom
     */
    private static RegAtom regNewAtom(XmlAutomata ctxt, int type)
    {
        RegAtom ret;

        ret = new RegAtom();
        ret.type = type;
        ret.quant = XML_REGEXP_QUANT_ONCE;
        ret.min = 0;
        ret.max = 0;
        return ret;
    }

    // xml_reg_free_atom
    /* Port of xmlRegFreeAtom. */
    private static void regFreeAtom(RegAtom atom)
    {
    }

    // xml_reg_copy_atom
    /*
     * Port of xmlRegCopyAtom.
     * Allocate a new regexp range
     */
    private static RegAtom regCopyAtom(XmlAutomata ctxt, RegAtom atom)
    {
        RegAtom ret;

        ret = new RegAtom();
        ret.type = atom.type;
        ret.quant = atom.quant;
        ret.min = atom.min;
        ret.max = atom.max;
        if (atom.nb_ranges > 0)
        {
            int i;

            ret.ranges = new RegRange[atom.nb_ranges];
            for (i = 0; i < atom.nb_ranges; i++)
            {
                ret.ranges[i] = regCopyRange(ctxt, atom.ranges[i]);
                if (ret.ranges[i] == null)
                {
                    regFreeAtom(ret);
                    return null;
                }
                ret.nb_ranges = i + 1;
            }
        }
        return ret;
    }

    // xml_reg_new_state
    /* Port of xmlRegNewState. */
    private static XmlAutomataState regNewState(XmlAutomata ctxt)
    {
        XmlAutomataState ret;

        ret = new XmlAutomataState();
        ret.type = XML_REGEXP_TRANS_STATE;
        ret.mark = XML_REGEXP_MARK_NORMAL;
        return ret;
    }

    // xml_reg_free_state
    /* Port of xmlRegFreeState. */
    private static void regFreeState(XmlAutomataState state)
    {
    }

    // xml_reg_free_parser_ctxt
    /* Port of xmlRegFreeParserCtxt. */
    private static void regFreeParserCtxt(XmlAutomata ctxt)
    {
    }

    /* ***********************************************************************
     *									*
     *		 Finite Automata structures manipulations		*
     *									*
     ************************************************************************/

    // xml_reg_atom_add_range
    /* Port of xmlRegAtomAddRange. */
    private static RegRange regAtomAddRange(XmlAutomata ctxt, RegAtom atom,
                                            int neg, int type, int start, int end,
                                            String block_name)
    {
        RegRange range;

        if (atom == null)
        {
            ERROR(ctxt, "add range: atom is NULL");
            return null;
        }
        if (atom.type != XML_REGEXP_RANGES)
        {
            ERROR(ctxt, "add range: atom is not ranges");
            return null;
        }
        if (atom.nb_ranges >= atom.max_ranges)
        {
            int new_size;

            new_size = regGrowCapacity(atom.max_ranges, 4, XML_MAX_ITEMS);
            if (new_size < 0)
            {
                regexpErrMemory(ctxt);
                return null;
            }
            atom.ranges = atom.ranges == null ? new RegRange[new_size] : Arrays.copyOf(atom.ranges, new_size);
            atom.max_ranges = new_size;
        }
        range = regNewRange(ctxt, neg, type, start, end);
        range.block_name = block_name;
        atom.ranges[atom.nb_ranges++] = range;

        return range;
    }

    // xml_reg_get_counter
    /* Port of xmlRegGetCounter. */
    private static int regGetCounter(XmlAutomata ctxt)
    {
        if (ctxt.nb_counters >= ctxt.max_counters)
        {
            int new_size;

            new_size = regGrowCapacity(ctxt.max_counters, 4, XML_MAX_ITEMS);
            if (new_size < 0)
            {
                regexpErrMemory(ctxt);
                return -1;
            }
            ctxt.counters = ctxt.counters == null ? new RegCounter[new_size] : Arrays.copyOf(ctxt.counters, new_size);
            ctxt.max_counters = new_size;
        }
        ctxt.counters[ctxt.nb_counters] = new RegCounter();
        ctxt.counters[ctxt.nb_counters].min = -1;
        ctxt.counters[ctxt.nb_counters].max = -1;
        return ctxt.nb_counters++;
    }

    // xml_reg_atom_push
    /* Port of xmlRegAtomPush. */
    private static int regAtomPush(XmlAutomata ctxt, RegAtom atom)
    {
        if (atom == null)
        {
            ERROR(ctxt, "atom push: atom is NULL");
            return -1;
        }
        if (ctxt.nb_atoms >= ctxt.max_atoms)
        {
            int new_size;

            new_size = regGrowCapacity(ctxt.max_atoms, 4, XML_MAX_ITEMS);
            if (new_size < 0)
            {
                regexpErrMemory(ctxt);
                return -1;
            }
            ctxt.atoms = ctxt.atoms == null ? new RegAtom[new_size] : Arrays.copyOf(ctxt.atoms, new_size);
            ctxt.max_atoms = new_size;
        }
        atom.no = ctxt.nb_atoms;
        ctxt.atoms[ctxt.nb_atoms++] = atom;
        return 0;
    }

    // xml_reg_state_add_trans_to
    /* Port of xmlRegStateAddTransTo. */
    private static void regStateAddTransTo(XmlAutomata ctxt, XmlAutomataState target, int from)
    {
        if (target.nb_trans_to >= target.max_trans_to)
        {
            int new_size;

            new_size = regGrowCapacity(target.max_trans_to, 8, XML_MAX_ITEMS);
            if (new_size < 0)
            {
                regexpErrMemory(ctxt);
                return;
            }
            target.trans_to = target.trans_to == null ? new int[new_size] : Arrays.copyOf(target.trans_to, new_size);
            target.max_trans_to = new_size;
        }
        target.trans_to[target.nb_trans_to] = from;
        target.nb_trans_to++;
    }

    // xml_reg_state_add_trans
    /* Port of xmlRegStateAddTrans. */
    private static void regStateAddTrans(XmlAutomata ctxt, XmlAutomataState state,
                                         RegAtom atom, XmlAutomataState target,
                                         int counter, int count)
    {
        int nrtrans;

        if (state == null)
        {
            ERROR(ctxt, "add state: state is NULL");
            return;
        }
        if (target == null)
        {
            ERROR(ctxt, "add state: target is NULL");
            return;
        }
        /*
         * Other routines follow the philosophy 'When in doubt, add a transition'
         * so we check here whether such a transition is already present and, if
         * so, silently ignore this request.
         */

        for (nrtrans = state.nb_trans - 1; nrtrans >= 0; nrtrans--)
        {
            RegTrans trans = state.trans[nrtrans];
            if ((trans.atom == atom) &&
                (trans.to == target.no) &&
                (trans.counter == counter) &&
                (trans.count == count))
            {
                return;
            }
        }

        if (state.nb_trans >= state.max_trans)
        {
            int new_size;

            new_size = regGrowCapacity(state.max_trans, 8, XML_MAX_ITEMS);
            if (new_size < 0)
            {
                regexpErrMemory(ctxt);
                return;
            }
            state.trans = state.trans == null ? new RegTrans[new_size] : Arrays.copyOf(state.trans, new_size);
            state.max_trans = new_size;
        }

        if (state.trans[state.nb_trans] == null)
            state.trans[state.nb_trans] = new RegTrans();
        state.trans[state.nb_trans].atom = atom;
        state.trans[state.nb_trans].to = target.no;
        state.trans[state.nb_trans].counter = counter;
        state.trans[state.nb_trans].count = count;
        state.trans[state.nb_trans].nd = 0;
        state.nb_trans++;
        regStateAddTransTo(ctxt, target, state.no);
    }

    // xml_reg_state_push
    /* Port of xmlRegStatePush. */
    private static XmlAutomataState regStatePush(XmlAutomata ctxt)
    {
        XmlAutomataState state;

        if (ctxt.nb_states >= ctxt.max_states)
        {
            int new_size;

            new_size = regGrowCapacity(ctxt.max_states, 4, XML_MAX_ITEMS);
            if (new_size < 0)
            {
                regexpErrMemory(ctxt);
                return null;
            }
            ctxt.states = ctxt.states == null ? new XmlAutomataState[new_size] : Arrays.copyOf(ctxt.states, new_size);
            ctxt.max_states = new_size;
        }

        state = regNewState(ctxt);

        state.no = ctxt.nb_states;
        ctxt.states[ctxt.nb_states++] = state;

        return state;
    }

    // xml_fa_generate_all_transition
    /* Port of xmlFAGenerateAllTransition. */
    private static int faGenerateAllTransition(XmlAutomata ctxt,
                                               XmlAutomataState from, XmlAutomataState to,
                                               int lax)
    {
        if (to == null)
        {
            to = regStatePush(ctxt);
            if (to == null)
                return -1;
            ctxt.state = to;
        }
        if (lax != 0)
            regStateAddTrans(ctxt, from, null, to, -1, REGEXP_ALL_LAX_COUNTER);
        else
            regStateAddTrans(ctxt, from, null, to, -1, REGEXP_ALL_COUNTER);
        return 0;
    }

    // xml_fa_generate_epsilon_transition
    /* Port of xmlFAGenerateEpsilonTransition. */
    private static int faGenerateEpsilonTransition(XmlAutomata ctxt,
                                                   XmlAutomataState from, XmlAutomataState to)
    {
        if (to == null)
        {
            to = regStatePush(ctxt);
            if (to == null)
                return -1;
            ctxt.state = to;
        }
        regStateAddTrans(ctxt, from, null, to, -1, -1);
        return 0;
    }

    // xml_fa_generate_counted_epsilon_transition
    /* Port of xmlFAGenerateCountedEpsilonTransition. */
    private static int faGenerateCountedEpsilonTransition(XmlAutomata ctxt,
                                                          XmlAutomataState from, XmlAutomataState to, int counter)
    {
        if (to == null)
        {
            to = regStatePush(ctxt);
            if (to == null)
                return -1;
            ctxt.state = to;
        }
        regStateAddTrans(ctxt, from, null, to, counter, -1);
        return 0;
    }

    // xml_fa_generate_counted_transition
    /* Port of xmlFAGenerateCountedTransition. */
    private static int faGenerateCountedTransition(XmlAutomata ctxt,
                                                   XmlAutomataState from, XmlAutomataState to, int counter)
    {
        if (to == null)
        {
            to = regStatePush(ctxt);
            if (to == null)
                return -1;
            ctxt.state = to;
        }
        regStateAddTrans(ctxt, from, null, to, -1, counter);
        return 0;
    }

    // xml_fa_generate_transitions
    /*
     * Port of xmlFAGenerateTransitions.
     * returns 0 if success and -1 in case of error.
     */
    private static int faGenerateTransitions(XmlAutomata ctxt, XmlAutomataState from,
                                             XmlAutomataState to, RegAtom atom)
    {
        XmlAutomataState end;
        int nullable = 0;

        if (atom == null)
        {
            ERROR(ctxt, "generate transition: atom == NULL");
            return -1;
        }
        if (atom.type == XML_REGEXP_SUBREG)
        {
            /*
             * this is a subexpression handling one should not need to
             * create a new node except for XML_REGEXP_QUANT_RANGE.
             */
            if ((to != null) && (atom.stop != to) &&
                (atom.quant != XML_REGEXP_QUANT_RANGE))
            {
                /*
                 * Generate an epsilon transition to link to the target
                 */
                faGenerateEpsilonTransition(ctxt, atom.stop, to);
            }
            switch (atom.quant)
            {
            case XML_REGEXP_QUANT_OPT:
                atom.quant = XML_REGEXP_QUANT_ONCE;
                /*
                 * transition done to the state after end of atom.
                 *      1. set transition from atom start to new state
                 *      2. set transition from atom end to this state.
                 */
                if (to == null)
                {
                    faGenerateEpsilonTransition(ctxt, atom.start, null);
                    faGenerateEpsilonTransition(ctxt, atom.stop, ctxt.state);
                }
                else
                {
                    faGenerateEpsilonTransition(ctxt, atom.start, to);
                }
                break;
            case XML_REGEXP_QUANT_MULT:
                atom.quant = XML_REGEXP_QUANT_ONCE;
                faGenerateEpsilonTransition(ctxt, atom.start, atom.stop);
                faGenerateEpsilonTransition(ctxt, atom.stop, atom.start);
                break;
            case XML_REGEXP_QUANT_PLUS:
                atom.quant = XML_REGEXP_QUANT_ONCE;
                faGenerateEpsilonTransition(ctxt, atom.stop, atom.start);
                break;
            case XML_REGEXP_QUANT_RANGE:
            {
                int counter;
                XmlAutomataState inter, newstate;

                /*
                 * create the final state now if needed
                 */
                if (to != null)
                {
                    newstate = to;
                }
                else
                {
                    newstate = regStatePush(ctxt);
                    if (newstate == null)
                        return -1;
                }

                /*
                 * The principle here is to use counted transition
                 * to avoid explosion in the number of states in the
                 * graph. This is clearly more complex but should not
                 * be exploitable at runtime.
                 */
                if ((atom.min == 0) && (atom.start0 == null))
                {
                    RegAtom copy;
                    /*
                     * duplicate a transition based on atom to count next
                     * occurrences after 1. We cannot loop to atom->start
                     * directly because we need an epsilon transition to
                     * newstate.
                     */
                    /* ???? For some reason it seems we never reach that
                       case, I suppose this got optimized out before when
                       building the automata */
                    copy = regCopyAtom(ctxt, atom);
                    if (copy == null)
                        return -1;
                    copy.quant = XML_REGEXP_QUANT_ONCE;
                    copy.min = 0;
                    copy.max = 0;

                    if (faGenerateTransitions(ctxt, atom.start, null, copy) < 0)
                    {
                        regFreeAtom(copy);
                        return -1;
                    }
                    inter = ctxt.state;
                    counter = regGetCounter(ctxt);
                    if (counter < 0)
                        return -1;
                    ctxt.counters[counter].min = atom.min - 1;
                    ctxt.counters[counter].max = atom.max - 1;
                    /* count the number of times we see it again */
                    faGenerateCountedEpsilonTransition(ctxt, inter, atom.stop, counter);
                    /* allow a way out based on the count */
                    faGenerateCountedTransition(ctxt, inter, newstate, counter);
                    /* and also allow a direct exit for 0 */
                    faGenerateEpsilonTransition(ctxt, atom.start, newstate);
                }
                else
                {
                    /*
                     * either we need the atom at least once or there
                     * is an atom->start0 allowing to easily plug the
                     * epsilon transition.
                     */
                    counter = regGetCounter(ctxt);
                    if (counter < 0)
                        return -1;
                    ctxt.counters[counter].min = atom.min - 1;
                    ctxt.counters[counter].max = atom.max - 1;
                    /* allow a way out based on the count */
                    faGenerateCountedTransition(ctxt, atom.stop, newstate, counter);
                    /* count the number of times we see it again */
                    faGenerateCountedEpsilonTransition(ctxt, atom.stop, atom.start, counter);
                    /* and if needed allow a direct exit for 0 */
                    if (atom.min == 0)
                        faGenerateEpsilonTransition(ctxt, atom.start0, newstate);

                }
                atom.min = 0;
                atom.max = 0;
                atom.quant = XML_REGEXP_QUANT_ONCE;
                ctxt.state = newstate;
            }
            // falls through
            default:
                break;
            }
            atom.start = null;
            atom.start0 = null;
            atom.stop = null;
            if (regAtomPush(ctxt, atom) < 0)
                return -1;
            return 0;
        }
        if ((atom.min == 0) && (atom.max == 0) &&
            (atom.quant == XML_REGEXP_QUANT_RANGE))
        {
            /*
             * we can discard the atom and generate an epsilon transition instead
             */
            if (to == null)
            {
                to = regStatePush(ctxt);
                if (to == null)
                    return -1;
            }
            faGenerateEpsilonTransition(ctxt, from, to);
            ctxt.state = to;
            regFreeAtom(atom);
            return 0;
        }
        if (to == null)
        {
            to = regStatePush(ctxt);
            if (to == null)
                return -1;
        }
        end = to;
        if ((atom.quant == XML_REGEXP_QUANT_MULT) ||
            (atom.quant == XML_REGEXP_QUANT_PLUS))
        {
            /*
             * Do not pollute the target state by adding transitions from
             * it as it is likely to be the shared target of multiple branches.
             * So isolate with an epsilon transition.
             */
            XmlAutomataState tmp;

            tmp = regStatePush(ctxt);
            if (tmp == null)
                return -1;
            faGenerateEpsilonTransition(ctxt, tmp, to);
            to = tmp;
        }
        if ((atom.quant == XML_REGEXP_QUANT_RANGE) &&
            (atom.min == 0) && (atom.max > 0))
        {
            nullable = 1;
            atom.min = 1;
            if (atom.max == 1)
                atom.quant = XML_REGEXP_QUANT_OPT;
        }
        regStateAddTrans(ctxt, from, atom, to, -1, -1);
        ctxt.state = end;
        switch (atom.quant)
        {
        case XML_REGEXP_QUANT_OPT:
            atom.quant = XML_REGEXP_QUANT_ONCE;
            faGenerateEpsilonTransition(ctxt, from, to);
            break;
        case XML_REGEXP_QUANT_MULT:
            atom.quant = XML_REGEXP_QUANT_ONCE;
            faGenerateEpsilonTransition(ctxt, from, to);
            regStateAddTrans(ctxt, to, atom, to, -1, -1);
            break;
        case XML_REGEXP_QUANT_PLUS:
            atom.quant = XML_REGEXP_QUANT_ONCE;
            regStateAddTrans(ctxt, to, atom, to, -1, -1);
            break;
        case XML_REGEXP_QUANT_RANGE:
            if (nullable != 0)
                faGenerateEpsilonTransition(ctxt, from, to);
            break;
        default:
            break;
        }
        if (regAtomPush(ctxt, atom) < 0)
            return -1;
        return 0;
    }

    // xml_fa_reduce_epsilon_transitions
    /* Port of xmlFAReduceEpsilonTransitions. */
    private static void faReduceEpsilonTransitions(XmlAutomata ctxt, int fromnr,
                                                   int tonr, int counter)
    {
        int transnr;
        XmlAutomataState from;
        XmlAutomataState to;

        from = ctxt.states[fromnr];
        if (from == null)
            return;
        to = ctxt.states[tonr];
        if (to == null)
            return;
        if ((to.mark == XML_REGEXP_MARK_START) ||
            (to.mark == XML_REGEXP_MARK_VISITED))
            return;

        to.mark = XML_REGEXP_MARK_VISITED;
        if (to.type == XML_REGEXP_FINAL_STATE)
        {
            from.type = XML_REGEXP_FINAL_STATE;
        }
        for (transnr = 0; transnr < to.nb_trans; transnr++)
        {
            RegTrans t1 = to.trans[transnr];
            int tcounter;

            if (t1.to < 0)
                continue;
            if (t1.counter >= 0)
            {
                /* assert(counter < 0); */
                tcounter = t1.counter;
            }
            else
            {
                tcounter = counter;
            }
            if (t1.atom == null)
            {
                /*
                 * Don't remove counted transitions
                 * Don't loop either
                 */
                if (t1.to != fromnr)
                {
                    if (t1.count >= 0)
                    {
                        regStateAddTrans(ctxt, from, null, ctxt.states[t1.to], -1, t1.count);
                    }
                    else
                    {
                        faReduceEpsilonTransitions(ctxt, fromnr, t1.to, tcounter);
                    }
                }
            }
            else
            {
                regStateAddTrans(ctxt, from, t1.atom, ctxt.states[t1.to], tcounter, -1);
            }
        }
    }

    // xml_fa_finish_reduce_epsilon_transitions
    /* Port of xmlFAFinishReduceEpsilonTransitions. */
    private static void faFinishReduceEpsilonTransitions(XmlAutomata ctxt, int tonr)
    {
        int transnr;
        XmlAutomataState to;

        to = ctxt.states[tonr];
        if (to == null)
            return;
        if ((to.mark == XML_REGEXP_MARK_START) ||
            (to.mark == XML_REGEXP_MARK_NORMAL))
            return;

        to.mark = XML_REGEXP_MARK_NORMAL;
        for (transnr = 0; transnr < to.nb_trans; transnr++)
        {
            RegTrans t1 = to.trans[transnr];
            if ((t1.to >= 0) && (t1.atom == null))
                faFinishReduceEpsilonTransitions(ctxt, t1.to);
        }
    }

    // xml_fa_eliminate_simple_epsilon_transitions
    /*
     * Port of xmlFAEliminateSimpleEpsilonTransitions.
     *
     * Eliminating general epsilon transitions can get costly in the general
     * algorithm due to the large amount of generated new transitions and
     * associated comparisons. However for simple epsilon transition used just
     * to separate building blocks when generating the automata this can be
     * reduced to state elimination:
     *    - if there exists an epsilon from X to Y
     *    - if there is no other transition from X
     * then X and Y are semantically equivalent and X can be eliminated
     * If X is the start state then make Y the start state, else replace the
     * target of all transitions to X by transitions to Y.
     *
     * If X is a final state, skip it.
     * Otherwise it would be necessary to manipulate counters for this case when
     * eliminating state 2:
     * State 1 has a transition with an atom to state 2.
     * State 2 is final and has an epsilon transition to state 1.
     */
    private static void faEliminateSimpleEpsilonTransitions(XmlAutomata ctxt)
    {
        int statenr, i, j, newto;
        XmlAutomataState state, tmp;

        for (statenr = 0; statenr < ctxt.nb_states; statenr++)
        {
            state = ctxt.states[statenr];
            if (state == null)
                continue;
            if (state.nb_trans != 1)
                continue;
            if (state.type == XML_REGEXP_UNREACH_STATE ||
                state.type == XML_REGEXP_FINAL_STATE)
                continue;
            /* is the only transition out a basic transition */
            if ((state.trans[0].atom == null) &&
                (state.trans[0].to >= 0) &&
                (state.trans[0].to != statenr) &&
                (state.trans[0].counter < 0) &&
                (state.trans[0].count < 0))
            {
                newto = state.trans[0].to;

                if (state.type == XML_REGEXP_START_STATE)
                {
                }
                else
                {
                    for (i = 0; i < state.nb_trans_to; i++)
                    {
                        tmp = ctxt.states[state.trans_to[i]];
                        for (j = 0; j < tmp.nb_trans; j++)
                        {
                            if (tmp.trans[j].to == statenr)
                            {
                                tmp.trans[j].to = -1;
                                regStateAddTrans(ctxt, tmp, tmp.trans[j].atom,
                                                 ctxt.states[newto],
                                                 tmp.trans[j].counter,
                                                 tmp.trans[j].count);
                            }
                        }
                    }
                    if (state.type == XML_REGEXP_FINAL_STATE)
                        ctxt.states[newto].type = XML_REGEXP_FINAL_STATE;
                    /* eliminate the transition completely */
                    state.nb_trans = 0;

                    state.type = XML_REGEXP_UNREACH_STATE;

                }

            }
        }
    }

    // xml_fa_eliminate_epsilon_transitions
    /* Port of xmlFAEliminateEpsilonTransitions. */
    private static void faEliminateEpsilonTransitions(XmlAutomata ctxt)
    {
        int statenr, transnr;
        XmlAutomataState state;
        int has_epsilon;

        if (ctxt.states == null) return;

        /*
         * Eliminate simple epsilon transition and the associated unreachable
         * states.
         */
        faEliminateSimpleEpsilonTransitions(ctxt);
        for (statenr = 0; statenr < ctxt.nb_states; statenr++)
        {
            state = ctxt.states[statenr];
            if ((state != null) && (state.type == XML_REGEXP_UNREACH_STATE))
            {
                regFreeState(state);
                ctxt.states[statenr] = null;
            }
        }

        has_epsilon = 0;

        /*
         * Build the completed transitions bypassing the epsilons
         * Use a marking algorithm to avoid loops
         * Mark sink states too.
         * Process from the latest states backward to the start when
         * there is long cascading epsilon chains this minimize the
         * recursions and transition compares when adding the new ones
         */
        for (statenr = ctxt.nb_states - 1; statenr >= 0; statenr--)
        {
            state = ctxt.states[statenr];
            if (state == null)
                continue;
            if ((state.nb_trans == 0) &&
                (state.type != XML_REGEXP_FINAL_STATE))
            {
                state.type = XML_REGEXP_SINK_STATE;
            }
            for (transnr = 0; transnr < state.nb_trans; transnr++)
            {
                if ((state.trans[transnr].atom == null) &&
                    (state.trans[transnr].to >= 0))
                {
                    if (state.trans[transnr].to == statenr)
                    {
                        state.trans[transnr].to = -1;
                    }
                    else if (state.trans[transnr].count < 0)
                    {
                        int newto = state.trans[transnr].to;

                        has_epsilon = 1;
                        state.trans[transnr].to = -2;
                        state.mark = XML_REGEXP_MARK_START;
                        faReduceEpsilonTransitions(ctxt, statenr,
                                                   newto, state.trans[transnr].counter);
                        faFinishReduceEpsilonTransitions(ctxt, newto);
                        state.mark = XML_REGEXP_MARK_NORMAL;
                    }
                }
            }
        }
        /*
         * Eliminate the epsilon transitions
         */
        if (has_epsilon != 0)
        {
            for (statenr = 0; statenr < ctxt.nb_states; statenr++)
            {
                state = ctxt.states[statenr];
                if (state == null)
                    continue;
                for (transnr = 0; transnr < state.nb_trans; transnr++)
                {
                    RegTrans trans = state.trans[transnr];
                    if ((trans.atom == null) &&
                        (trans.count < 0) &&
                        (trans.to >= 0))
                    {
                        trans.to = -1;
                    }
                }
            }
        }

        /*
         * Use this pass to detect unreachable states too
         */
        for (statenr = 0; statenr < ctxt.nb_states; statenr++)
        {
            state = ctxt.states[statenr];
            if (state != null)
                state.reached = XML_REGEXP_MARK_NORMAL;
        }
        state = ctxt.states[0];
        if (state != null)
            state.reached = XML_REGEXP_MARK_START;
        while (state != null)
        {
            XmlAutomataState target = null;
            state.reached = XML_REGEXP_MARK_VISITED;
            /*
             * Mark all states reachable from the current reachable state
             */
            for (transnr = 0; transnr < state.nb_trans; transnr++)
            {
                if ((state.trans[transnr].to >= 0) &&
                    ((state.trans[transnr].atom != null) ||
                     (state.trans[transnr].count >= 0)))
                {
                    int newto = state.trans[transnr].to;

                    if (ctxt.states[newto] == null)
                        continue;
                    if (ctxt.states[newto].reached == XML_REGEXP_MARK_NORMAL)
                    {
                        ctxt.states[newto].reached = XML_REGEXP_MARK_START;
                        target = ctxt.states[newto];
                    }
                }
            }

            /*
             * find the next accessible state not explored
             */
            if (target == null)
            {
                for (statenr = 1; statenr < ctxt.nb_states; statenr++)
                {
                    state = ctxt.states[statenr];
                    if ((state != null) && (state.reached ==
                                            XML_REGEXP_MARK_START))
                    {
                        target = state;
                        break;
                    }
                }
            }
            state = target;
        }
        for (statenr = 0; statenr < ctxt.nb_states; statenr++)
        {
            state = ctxt.states[statenr];
            if ((state != null) && (state.reached == XML_REGEXP_MARK_NORMAL))
            {
                regFreeState(state);
                ctxt.states[statenr] = null;
            }
        }

    }

    // xml_fa_compare_ranges
    /* Port of xmlFACompareRanges. */
    private static int faCompareRanges(RegRange range1, RegRange range2)
    {
        int ret = 0;

        if ((range1.type == XML_REGEXP_RANGES) ||
            (range2.type == XML_REGEXP_RANGES) ||
            (range2.type == XML_REGEXP_SUBREG) ||
            (range1.type == XML_REGEXP_SUBREG) ||
            (range1.type == XML_REGEXP_STRING) ||
            (range2.type == XML_REGEXP_STRING))
            return -1;

        /* put them in order */
        if (range1.type > range2.type)
        {
            RegRange tmp;

            tmp = range1;
            range1 = range2;
            range2 = tmp;
        }
        if ((range1.type == XML_REGEXP_ANYCHAR) ||
            (range2.type == XML_REGEXP_ANYCHAR))
        {
            ret = 1;
        }
        else if ((range1.type == XML_REGEXP_EPSILON) ||
                 (range2.type == XML_REGEXP_EPSILON))
        {
            return 0;
        }
        else if (range1.type == range2.type)
        {
            if (range1.type != XML_REGEXP_CHARVAL)
                ret = 1;
            else if ((range1.end < range2.start) ||
                     (range2.end < range1.start))
                ret = 0;
            else
                ret = 1;
        }
        else if (range1.type == XML_REGEXP_CHARVAL)
        {
            int codepoint;
            int neg = 0;

            /*
             * just check all codepoints in the range for acceptance,
             * this is usually way cheaper since done only once at
             * compilation than testing over and over at runtime or
             * pushing too many states when evaluating.
             */
            if (((range1.neg == 0) && (range2.neg != 0)) ||
                ((range1.neg != 0) && (range2.neg == 0)))
                neg = 1;

            for (codepoint = range1.start; codepoint <= range1.end; codepoint++)
            {
                ret = regCheckCharacterRange(range2.type, codepoint,
                                             0, range2.start, range2.end,
                                             range2.block_name);
                if (ret < 0)
                    return -1;
                if (((neg == 1) && (ret == 0)) ||
                    ((neg == 0) && (ret == 1)))
                    return 1;
                if (codepoint == INT_MAX) break; /* C would overflow */
            }
            return 0;
        }
        else if ((range1.type == XML_REGEXP_BLOCK_NAME) ||
                 (range2.type == XML_REGEXP_BLOCK_NAME))
        {
            if (range1.type == range2.type)
            {
                ret = regStrEqual(range1.block_name, range2.block_name);
            }
            else
            {
                /*
                 * comparing a block range with anything else is way
                 * too costly, and maintaining the table is like too much
                 * memory too, so let's force the automata to save state
                 * here.
                 */
                return 1;
            }
        }
        else if ((range1.type < XML_REGEXP_LETTER) ||
                 (range2.type < XML_REGEXP_LETTER))
        {
            if ((range1.type == XML_REGEXP_ANYSPACE) &&
                (range2.type == XML_REGEXP_NOTSPACE))
                ret = 0;
            else if ((range1.type == XML_REGEXP_INITNAME) &&
                     (range2.type == XML_REGEXP_NOTINITNAME))
                ret = 0;
            else if ((range1.type == XML_REGEXP_NAMECHAR) &&
                     (range2.type == XML_REGEXP_NOTNAMECHAR))
                ret = 0;
            else if ((range1.type == XML_REGEXP_DECIMAL) &&
                     (range2.type == XML_REGEXP_NOTDECIMAL))
                ret = 0;
            else if ((range1.type == XML_REGEXP_REALCHAR) &&
                     (range2.type == XML_REGEXP_NOTREALCHAR))
                ret = 0;
            else
            {
                /* same thing to limit complexity */
                return 1;
            }
        }
        else
        {
            ret = 0;
            /* range1->type < range2->type here */
            switch (range1.type)
            {
            case XML_REGEXP_LETTER:
                /* all disjoint except in the subgroups */
                if ((range2.type == XML_REGEXP_LETTER_UPPERCASE) ||
                    (range2.type == XML_REGEXP_LETTER_LOWERCASE) ||
                    (range2.type == XML_REGEXP_LETTER_TITLECASE) ||
                    (range2.type == XML_REGEXP_LETTER_MODIFIER) ||
                    (range2.type == XML_REGEXP_LETTER_OTHERS))
                    ret = 1;
                break;
            case XML_REGEXP_MARK:
                if ((range2.type == XML_REGEXP_MARK_NONSPACING) ||
                    (range2.type == XML_REGEXP_MARK_SPACECOMBINING) ||
                    (range2.type == XML_REGEXP_MARK_ENCLOSING))
                    ret = 1;
                break;
            case XML_REGEXP_NUMBER:
                if ((range2.type == XML_REGEXP_NUMBER_DECIMAL) ||
                    (range2.type == XML_REGEXP_NUMBER_LETTER) ||
                    (range2.type == XML_REGEXP_NUMBER_OTHERS))
                    ret = 1;
                break;
            case XML_REGEXP_PUNCT:
                if ((range2.type == XML_REGEXP_PUNCT_CONNECTOR) ||
                    (range2.type == XML_REGEXP_PUNCT_DASH) ||
                    (range2.type == XML_REGEXP_PUNCT_OPEN) ||
                    (range2.type == XML_REGEXP_PUNCT_CLOSE) ||
                    (range2.type == XML_REGEXP_PUNCT_INITQUOTE) ||
                    (range2.type == XML_REGEXP_PUNCT_FINQUOTE) ||
                    (range2.type == XML_REGEXP_PUNCT_OTHERS))
                    ret = 1;
                break;
            case XML_REGEXP_SEPAR:
                if ((range2.type == XML_REGEXP_SEPAR_SPACE) ||
                    (range2.type == XML_REGEXP_SEPAR_LINE) ||
                    (range2.type == XML_REGEXP_SEPAR_PARA))
                    ret = 1;
                break;
            case XML_REGEXP_SYMBOL:
                if ((range2.type == XML_REGEXP_SYMBOL_MATH) ||
                    (range2.type == XML_REGEXP_SYMBOL_CURRENCY) ||
                    (range2.type == XML_REGEXP_SYMBOL_MODIFIER) ||
                    (range2.type == XML_REGEXP_SYMBOL_OTHERS))
                    ret = 1;
                break;
            case XML_REGEXP_OTHER:
                if ((range2.type == XML_REGEXP_OTHER_CONTROL) ||
                    (range2.type == XML_REGEXP_OTHER_FORMAT) ||
                    (range2.type == XML_REGEXP_OTHER_PRIVATE))
                    ret = 1;
                break;
            default:
                if ((range2.type >= XML_REGEXP_LETTER) &&
                    (range2.type < XML_REGEXP_BLOCK_NAME))
                    ret = 0;
                else
                {
                    /* safety net ! */
                    return 1;
                }
            }
        }
        if (((range1.neg == 0) && (range2.neg != 0)) ||
            ((range1.neg != 0) && (range2.neg == 0)))
            ret = (ret == 0) ? 1 : 0;
        return ret;
    }

    // xml_fa_compare_atom_types
    /*
     * Port of xmlFACompareAtomTypes.
     *
     * Compares two atoms type to check whether they intersect in some ways,
     * this is used by xml_fa_compare_atoms only
     * returns 1 if they may intersect and 0 otherwise
     */
    private static int faCompareAtomTypes(int type1, int type2)
    {
        if ((type1 == XML_REGEXP_EPSILON) ||
            (type1 == XML_REGEXP_CHARVAL) ||
            (type1 == XML_REGEXP_RANGES) ||
            (type1 == XML_REGEXP_SUBREG) ||
            (type1 == XML_REGEXP_STRING) ||
            (type1 == XML_REGEXP_ANYCHAR))
            return 1;
        if ((type2 == XML_REGEXP_EPSILON) ||
            (type2 == XML_REGEXP_CHARVAL) ||
            (type2 == XML_REGEXP_RANGES) ||
            (type2 == XML_REGEXP_SUBREG) ||
            (type2 == XML_REGEXP_STRING) ||
            (type2 == XML_REGEXP_ANYCHAR))
            return 1;

        if (type1 == type2) return 1;

        /* simplify subsequent compares by making sure type1 < type2 */
        if (type1 > type2)
        {
            int tmp = type1;
            type1 = type2;
            type2 = tmp;
        }
        switch (type1)
        {
        case XML_REGEXP_ANYSPACE: /* \s */
            /* can't be a letter, number, mark, punctuation, symbol */
            if ((type2 == XML_REGEXP_NOTSPACE) ||
                ((type2 >= XML_REGEXP_LETTER) &&
                 (type2 <= XML_REGEXP_LETTER_OTHERS)) ||
                ((type2 >= XML_REGEXP_NUMBER) &&
                 (type2 <= XML_REGEXP_NUMBER_OTHERS)) ||
                ((type2 >= XML_REGEXP_MARK) &&
                 (type2 <= XML_REGEXP_MARK_ENCLOSING)) ||
                ((type2 >= XML_REGEXP_PUNCT) &&
                 (type2 <= XML_REGEXP_PUNCT_OTHERS)) ||
                ((type2 >= XML_REGEXP_SYMBOL) &&
                 (type2 <= XML_REGEXP_SYMBOL_OTHERS))
                ) return 0;
            break;
        case XML_REGEXP_NOTSPACE: /* \S */
            break;
        case XML_REGEXP_INITNAME: /* \l */
            /* can't be a number, mark, separator, punctuation, symbol or other */
            if ((type2 == XML_REGEXP_NOTINITNAME) ||
                ((type2 >= XML_REGEXP_NUMBER) &&
                 (type2 <= XML_REGEXP_NUMBER_OTHERS)) ||
                ((type2 >= XML_REGEXP_MARK) &&
                 (type2 <= XML_REGEXP_MARK_ENCLOSING)) ||
                ((type2 >= XML_REGEXP_SEPAR) &&
                 (type2 <= XML_REGEXP_SEPAR_PARA)) ||
                ((type2 >= XML_REGEXP_PUNCT) &&
                 (type2 <= XML_REGEXP_PUNCT_OTHERS)) ||
                ((type2 >= XML_REGEXP_SYMBOL) &&
                 (type2 <= XML_REGEXP_SYMBOL_OTHERS)) ||
                ((type2 >= XML_REGEXP_OTHER) &&
                 (type2 <= XML_REGEXP_OTHER_NA))
                ) return 0;
            break;
        case XML_REGEXP_NOTINITNAME: /* \L */
            break;
        case XML_REGEXP_NAMECHAR: /* \c */
            /* can't be a mark, separator, punctuation, symbol or other */
            if ((type2 == XML_REGEXP_NOTNAMECHAR) ||
                ((type2 >= XML_REGEXP_MARK) &&
                 (type2 <= XML_REGEXP_MARK_ENCLOSING)) ||
                ((type2 >= XML_REGEXP_PUNCT) &&
                 (type2 <= XML_REGEXP_PUNCT_OTHERS)) ||
                ((type2 >= XML_REGEXP_SEPAR) &&
                 (type2 <= XML_REGEXP_SEPAR_PARA)) ||
                ((type2 >= XML_REGEXP_SYMBOL) &&
                 (type2 <= XML_REGEXP_SYMBOL_OTHERS)) ||
                ((type2 >= XML_REGEXP_OTHER) &&
                 (type2 <= XML_REGEXP_OTHER_NA))
                ) return 0;
            break;
        case XML_REGEXP_NOTNAMECHAR: /* \C */
            break;
        case XML_REGEXP_DECIMAL: /* \d */
            /* can't be a letter, mark, separator, punctuation, symbol or other */
            if ((type2 == XML_REGEXP_NOTDECIMAL) ||
                (type2 == XML_REGEXP_REALCHAR) ||
                ((type2 >= XML_REGEXP_LETTER) &&
                 (type2 <= XML_REGEXP_LETTER_OTHERS)) ||
                ((type2 >= XML_REGEXP_MARK) &&
                 (type2 <= XML_REGEXP_MARK_ENCLOSING)) ||
                ((type2 >= XML_REGEXP_PUNCT) &&
                 (type2 <= XML_REGEXP_PUNCT_OTHERS)) ||
                ((type2 >= XML_REGEXP_SEPAR) &&
                 (type2 <= XML_REGEXP_SEPAR_PARA)) ||
                ((type2 >= XML_REGEXP_SYMBOL) &&
                 (type2 <= XML_REGEXP_SYMBOL_OTHERS)) ||
                ((type2 >= XML_REGEXP_OTHER) &&
                 (type2 <= XML_REGEXP_OTHER_NA))
                ) return 0;
            break;
        case XML_REGEXP_NOTDECIMAL: /* \D */
            break;
        case XML_REGEXP_REALCHAR: /* \w */
            /* can't be a mark, separator, punctuation, symbol or other */
            if ((type2 == XML_REGEXP_NOTDECIMAL) ||
                ((type2 >= XML_REGEXP_MARK) &&
                 (type2 <= XML_REGEXP_MARK_ENCLOSING)) ||
                ((type2 >= XML_REGEXP_PUNCT) &&
                 (type2 <= XML_REGEXP_PUNCT_OTHERS)) ||
                ((type2 >= XML_REGEXP_SEPAR) &&
                 (type2 <= XML_REGEXP_SEPAR_PARA)) ||
                ((type2 >= XML_REGEXP_SYMBOL) &&
                 (type2 <= XML_REGEXP_SYMBOL_OTHERS)) ||
                ((type2 >= XML_REGEXP_OTHER) &&
                 (type2 <= XML_REGEXP_OTHER_NA))
                ) return 0;
            break;
        case XML_REGEXP_NOTREALCHAR: /* \W */
            break;
            /*
             * at that point we know both type 1 and type2 are from
             * character categories are ordered and are different,
             * it becomes simple because this is a partition
             */
        case XML_REGEXP_LETTER:
            if (type2 <= XML_REGEXP_LETTER_OTHERS)
                return 1;
            return 0;
        case XML_REGEXP_LETTER_UPPERCASE:
        case XML_REGEXP_LETTER_LOWERCASE:
        case XML_REGEXP_LETTER_TITLECASE:
        case XML_REGEXP_LETTER_MODIFIER:
        case XML_REGEXP_LETTER_OTHERS:
            return 0;
        case XML_REGEXP_MARK:
            if (type2 <= XML_REGEXP_MARK_ENCLOSING)
                return 1;
            return 0;
        case XML_REGEXP_MARK_NONSPACING:
        case XML_REGEXP_MARK_SPACECOMBINING:
        case XML_REGEXP_MARK_ENCLOSING:
            return 0;
        case XML_REGEXP_NUMBER:
            if (type2 <= XML_REGEXP_NUMBER_OTHERS)
                return 1;
            return 0;
        case XML_REGEXP_NUMBER_DECIMAL:
        case XML_REGEXP_NUMBER_LETTER:
        case XML_REGEXP_NUMBER_OTHERS:
            return 0;
        case XML_REGEXP_PUNCT:
            if (type2 <= XML_REGEXP_PUNCT_OTHERS)
                return 1;
            return 0;
        case XML_REGEXP_PUNCT_CONNECTOR:
        case XML_REGEXP_PUNCT_DASH:
        case XML_REGEXP_PUNCT_OPEN:
        case XML_REGEXP_PUNCT_CLOSE:
        case XML_REGEXP_PUNCT_INITQUOTE:
        case XML_REGEXP_PUNCT_FINQUOTE:
        case XML_REGEXP_PUNCT_OTHERS:
            return 0;
        case XML_REGEXP_SEPAR:
            if (type2 <= XML_REGEXP_SEPAR_PARA)
                return 1;
            return 0;
        case XML_REGEXP_SEPAR_SPACE:
        case XML_REGEXP_SEPAR_LINE:
        case XML_REGEXP_SEPAR_PARA:
            return 0;
        case XML_REGEXP_SYMBOL:
            if (type2 <= XML_REGEXP_SYMBOL_OTHERS)
                return 1;
            return 0;
        case XML_REGEXP_SYMBOL_MATH:
        case XML_REGEXP_SYMBOL_CURRENCY:
        case XML_REGEXP_SYMBOL_MODIFIER:
        case XML_REGEXP_SYMBOL_OTHERS:
            return 0;
        case XML_REGEXP_OTHER:
            if (type2 <= XML_REGEXP_OTHER_NA)
                return 1;
            return 0;
        case XML_REGEXP_OTHER_CONTROL:
        case XML_REGEXP_OTHER_FORMAT:
        case XML_REGEXP_OTHER_PRIVATE:
        case XML_REGEXP_OTHER_NA:
            return 0;
        default:
            break;
        }
        return 1;
    }

    // xml_fa_equal_atoms
    /*
     * Port of xmlFAEqualAtoms.
     *
     * Compares two atoms to check whether they are the same exactly
     * this is used to remove equivalent transitions
     * deep: if not set only compare string pointers
     * returns 1 if same and 0 otherwise
     */
    private static int faEqualAtoms(RegAtom atom1, RegAtom atom2, int deep)
    {
        int ret = 0;

        if (atom1 == atom2)
            return 1;
        if ((atom1 == null) || (atom2 == null))
            return 0;

        if (atom1.type != atom2.type)
            return 0;
        switch (atom1.type)
        {
        case XML_REGEXP_EPSILON:
            ret = 0;
            break;
        case XML_REGEXP_STRING:
            if (deep == 0)
                ret = (atom1.valuep == atom2.valuep) ? 1 : 0;
            else
                ret = regStrEqual((String) atom1.valuep, (String) atom2.valuep);
            break;
        case XML_REGEXP_CHARVAL:
            ret = (atom1.codepoint == atom2.codepoint) ? 1 : 0;
            break;
        case XML_REGEXP_RANGES:
            /* too hard to do in the general case */
            ret = 0;
            // falls through
        default:
            break;
        }
        return ret;
    }

    // xml_fa_compare_atoms
    /*
     * Port of xmlFACompareAtoms.
     *
     * Compares two atoms to check whether they intersect in some ways,
     * this is used by xml_fa_computes_determinism and xml_fa_recurse_determinism only
     * deep: if not set only compare string pointers
     * returns 1 if yes and 0 otherwise
     */
    private static int faCompareAtoms(RegAtom atom1, RegAtom atom2, int deep)
    {
        int ret = 1;

        if (atom1 == atom2)
            return 1;
        if ((atom1 == null) || (atom2 == null))
            return 0;

        if ((atom1.type == XML_REGEXP_ANYCHAR) ||
            (atom2.type == XML_REGEXP_ANYCHAR))
            return 1;

        if (atom1.type > atom2.type)
        {
            RegAtom tmp;
            tmp = atom1;
            atom1 = atom2;
            atom2 = tmp;
        }
        if (atom1.type != atom2.type)
        {
            ret = faCompareAtomTypes(atom1.type, atom2.type);
            /* if they can't intersect at the type level break now */
            if (ret == 0)
                return 0;
        }
        switch (atom1.type)
        {
        case XML_REGEXP_STRING:
            if (deep == 0)
                ret = (atom1.valuep != atom2.valuep) ? 1 : 0;
            else
            {
                String val1 = (String) atom1.valuep;
                String val2 = (String) atom2.valuep;
                boolean compound1 = regStrchr(val1, '|');
                boolean compound2 = regStrchr(val2, '|');

                /* Ignore negative match flag for ##other namespaces */
                if (compound1 != compound2)
                    return 0;

                ret = regStrEqualWildcard(val1, val2);
            }
            break;
        case XML_REGEXP_EPSILON:
            /* goto not_determinist */
            return 1;
        case XML_REGEXP_CHARVAL:
            if (atom2.type == XML_REGEXP_CHARVAL)
            {
                ret = (atom1.codepoint == atom2.codepoint) ? 1 : 0;
            }
            else
            {
                ret = regCheckCharacter(atom2, atom1.codepoint);
                if (ret < 0)
                    ret = 1;
            }
            break;
        case XML_REGEXP_RANGES:
            if (atom2.type == XML_REGEXP_RANGES)
            {
                int i, j, res;
                RegRange r1, r2;

                /*
                 * need to check that none of the ranges eventually matches
                 */
                ret = 0;
            done:
                for (i = 0; i < atom1.nb_ranges; i++)
                {
                    for (j = 0; j < atom2.nb_ranges; j++)
                    {
                        r1 = atom1.ranges[i];
                        r2 = atom2.ranges[j];
                        res = faCompareRanges(r1, r2);
                        if (res == 1)
                        {
                            ret = 1;
                            break done;
                        }
                    }
                }
            }
            break;
        default:
            /* goto not_determinist */
            return 1;
        }
        /* done: */
        if (atom1.neg != atom2.neg)
        {
            ret = (ret == 0) ? 1 : 0;
        }
        if (ret == 0)
            return 0;
        /* not_determinist: */
        return 1;
    }

    // xml_fa_recurse_determinism
    /*
     * Port of xmlFARecurseDeterminism.
     *
     * Check whether the associated regexp is determinist,
     * should be called after xml_fa_eliminate_epsilon_transitions
     */
    private static int faRecurseDeterminism(XmlAutomata ctxt, XmlAutomataState state,
                                            int fromnr, int tonr, RegAtom atom)
    {
        int ret = 1;
        int res;
        int transnr, nb_trans;
        RegTrans t1;
        int deep = 1;

        if (state == null)
            return ret;
        if (state.markd == XML_REGEXP_MARK_VISITED)
            return ret;

        if ((ctxt.flags & AM_AUTOMATA_RNG) != 0)
            deep = 0;

        /*
         * don't recurse on transitions potentially added in the course of
         * the elimination.
         */
        nb_trans = state.nb_trans;
        for (transnr = 0; transnr < nb_trans; transnr++)
        {
            t1 = state.trans[transnr];
            /*
             * check transitions conflicting with the one looked at
             */
            if ((t1.to < 0) || (t1.to == fromnr))
                continue;
            if (t1.atom == null)
            {
                state.markd = XML_REGEXP_MARK_VISITED;
                res = faRecurseDeterminism(ctxt, ctxt.states[t1.to],
                                           fromnr, tonr, atom);
                if (res == 0)
                {
                    ret = 0;
                    /* t1->nd = 1; */
                }
                continue;
            }
            if (faCompareAtoms(t1.atom, atom, deep) != 0)
            {
                /* Treat equal transitions as deterministic. */
                if ((t1.to != tonr) ||
                    (faEqualAtoms(t1.atom, atom, deep) == 0))
                    ret = 0;
                /* mark the transition as non-deterministic */
                t1.nd = 1;
            }
        }
        return ret;
    }

    // xml_fa_finish_recurse_determinism
    /*
     * Port of xmlFAFinishRecurseDeterminism.
     * Reset flags after checking determinism.
     */
    private static void faFinishRecurseDeterminism(XmlAutomata ctxt, XmlAutomataState state)
    {
        int transnr, nb_trans;

        if (state == null)
            return;
        if (state.markd != XML_REGEXP_MARK_VISITED)
            return;
        state.markd = XML_REGEXP_MARK_NORMAL;

        nb_trans = state.nb_trans;
        for (transnr = 0; transnr < nb_trans; transnr++)
        {
            RegTrans t1 = state.trans[transnr];
            if ((t1.atom == null) && (t1.to >= 0))
                faFinishRecurseDeterminism(ctxt, ctxt.states[t1.to]);
        }
    }

    // xml_fa_computes_determinism
    /*
     * Port of xmlFAComputesDeterminism.
     *
     * Check whether the associated regexp is determinist,
     * should be called after xml_fa_eliminate_epsilon_transitions
     */
    private static int faComputesDeterminism(XmlAutomata ctxt)
    {
        int statenr, transnr;
        XmlAutomataState state;
        RegTrans t1, t2, last;
        int i;
        int ret = 1;
        int deep = 1;

        if (ctxt.determinist != -1)
            return ctxt.determinist;

        if ((ctxt.flags & AM_AUTOMATA_RNG) != 0)
            deep = 0;

        /*
         * First cleanup the automata removing cancelled transitions
         */
        for (statenr = 0; statenr < ctxt.nb_states; statenr++)
        {
            state = ctxt.states[statenr];
            if (state == null)
                continue;
            if (state.nb_trans < 2)
                continue;
            for (transnr = 0; transnr < state.nb_trans; transnr++)
            {
                t1 = state.trans[transnr];
                /*
                 * Determinism checks in case of counted or all transitions
                 * will have to be handled separately
                 */
                if (t1.atom == null)
                {
                    /* t1->nd = 1; */
                    continue;
                }
                if (t1.to < 0) /* eliminated */
                    continue;
                for (i = 0; i < transnr; i++)
                {
                    t2 = state.trans[i];
                    if (t2.to < 0) /* eliminated */
                        continue;
                    if (t2.atom != null)
                    {
                        if (t1.to == t2.to)
                        {
                            /*
                             * Here we use deep because we want to keep the
                             * transitions which indicate a conflict
                             */
                            if ((faEqualAtoms(t1.atom, t2.atom, deep) != 0) &&
                                (t1.counter == t2.counter) &&
                                (t1.count == t2.count))
                                t2.to = -1; /* eliminated */
                        }
                    }
                }
            }
        }

        /*
         * Check for all states that there aren't 2 transitions
         * with the same atom and a different target.
         */
        for (statenr = 0; statenr < ctxt.nb_states; statenr++)
        {
            state = ctxt.states[statenr];
            if (state == null)
                continue;
            if (state.nb_trans < 2)
                continue;
            last = null;
            for (transnr = 0; transnr < state.nb_trans; transnr++)
            {
                t1 = state.trans[transnr];
                /*
                 * Determinism checks in case of counted or all transitions
                 * will have to be handled separately
                 */
                if (t1.atom == null)
                {
                    continue;
                }
                if (t1.to < 0) /* eliminated */
                    continue;
                for (i = 0; i < transnr; i++)
                {
                    t2 = state.trans[i];
                    if (t2.to < 0) /* eliminated */
                        continue;
                    if (t2.atom != null)
                    {
                        /*
                         * But here we don't use deep because we want to
                         * find transitions which indicate a conflict
                         */
                        if (faCompareAtoms(t1.atom, t2.atom, 1) != 0)
                        {
                            /*
                             * Treat equal counter transitions that couldn't be
                             * eliminated as deterministic.
                             */
                            if ((t1.to != t2.to) ||
                                (t1.counter == t2.counter) ||
                                (faEqualAtoms(t1.atom, t2.atom, deep) == 0))
                                ret = 0;
                            /* mark the transitions as non-deterministic ones */
                            t1.nd = 1;
                            t2.nd = 1;
                            last = t1;
                        }
                    }
                    else
                    {
                        int res;

                        /*
                         * do the closure in case of remaining specific
                         * epsilon transitions like choices or all
                         */
                        res = faRecurseDeterminism(ctxt, ctxt.states[t2.to],
                                                   statenr, t1.to, t1.atom);
                        faFinishRecurseDeterminism(ctxt, ctxt.states[t2.to]);
                        /* don't shortcut the computation so all non deterministic
                           transition get marked down
                        if (ret == 0)
                            return(0);
                         */
                        if (res == 0)
                        {
                            t1.nd = 1;
                            /* t2->nd = 1; */
                            last = t1;
                            ret = 0;
                        }
                    }
                }
                /* don't shortcut the computation so all non deterministic
                   transition get marked down
                if (ret == 0)
                    break; */
            }

            /*
             * mark specifically the last non-deterministic transition
             * from a state since there is no need to set-up rollback
             * from it
             */
            if (last != null)
            {
                last.nd = 2;
            }

            /* don't shortcut the computation so all non deterministic
               transition get marked down
            if (ret == 0)
                break; */
        }

        ctxt.determinist = ret;
        return ret;
    }

    /* ***********************************************************************
     *									*
     *	Routines to check input against transition atoms		*
     *									*
     ************************************************************************/

    private static int b(boolean v)
    {
        return v ? 1 : 0;
    }

    // xml_reg_check_character_range
    /* Port of xmlRegCheckCharacterRange. */
    private static int regCheckCharacterRange(int type, int codepoint, int neg,
                                              int start, int end, String block_name)
    {
        int ret = 0;

        switch (type)
        {
        case XML_REGEXP_STRING:
        case XML_REGEXP_SUBREG:
        case XML_REGEXP_RANGES:
        case XML_REGEXP_EPSILON:
            return -1;
        case XML_REGEXP_ANYCHAR:
            ret = b((codepoint != '\n') && (codepoint != '\r'));
            break;
        case XML_REGEXP_CHARVAL:
            ret = b((codepoint >= start) && (codepoint <= end));
            break;
        case XML_REGEXP_NOTSPACE:
            neg = b(neg == 0);
            /* Falls through. */
        case XML_REGEXP_ANYSPACE:
            ret = b((codepoint == '\n') || (codepoint == '\r') ||
                    (codepoint == '\t') || (codepoint == ' '));
            break;
        case XML_REGEXP_NOTINITNAME:
            neg = b(neg == 0);
            /* Falls through. */
        case XML_REGEXP_INITNAME:
            ret = b(XmlChars.isLetter(codepoint) ||
                    (codepoint == '_') || (codepoint == ':'));
            break;
        case XML_REGEXP_NOTNAMECHAR:
            neg = b(neg == 0);
            /* Falls through. */
        case XML_REGEXP_NAMECHAR:
            ret = b(XmlChars.isLetter(codepoint) || XmlChars.isDigit(codepoint) ||
                    (codepoint == '.') || (codepoint == '-') ||
                    (codepoint == '_') || (codepoint == ':') ||
                    XmlChars.isCombining(codepoint) || XmlChars.isExtender(codepoint));
            break;
        case XML_REGEXP_NOTDECIMAL:
            neg = b(neg == 0);
            /* Falls through. */
        case XML_REGEXP_DECIMAL:
            ret = XmlUnicode.ucsIsCatNd(codepoint);
            break;
        case XML_REGEXP_REALCHAR:
            neg = b(neg == 0);
            /* Falls through. */
        case XML_REGEXP_NOTREALCHAR:
            ret = XmlUnicode.ucsIsCatP(codepoint);
            if (ret == 0)
                ret = XmlUnicode.ucsIsCatZ(codepoint);
            if (ret == 0)
                ret = XmlUnicode.ucsIsCatC(codepoint);
            break;
        case XML_REGEXP_LETTER:
            ret = XmlUnicode.ucsIsCatL(codepoint);
            break;
        case XML_REGEXP_LETTER_UPPERCASE:
            ret = XmlUnicode.ucsIsCatLu(codepoint);
            break;
        case XML_REGEXP_LETTER_LOWERCASE:
            ret = XmlUnicode.ucsIsCatLl(codepoint);
            break;
        case XML_REGEXP_LETTER_TITLECASE:
            ret = XmlUnicode.ucsIsCatLt(codepoint);
            break;
        case XML_REGEXP_LETTER_MODIFIER:
            ret = XmlUnicode.ucsIsCatLm(codepoint);
            break;
        case XML_REGEXP_LETTER_OTHERS:
            ret = XmlUnicode.ucsIsCatLo(codepoint);
            break;
        case XML_REGEXP_MARK:
            ret = XmlUnicode.ucsIsCatM(codepoint);
            break;
        case XML_REGEXP_MARK_NONSPACING:
            ret = XmlUnicode.ucsIsCatMn(codepoint);
            break;
        case XML_REGEXP_MARK_SPACECOMBINING:
            ret = XmlUnicode.ucsIsCatMc(codepoint);
            break;
        case XML_REGEXP_MARK_ENCLOSING:
            ret = XmlUnicode.ucsIsCatMe(codepoint);
            break;
        case XML_REGEXP_NUMBER:
            ret = XmlUnicode.ucsIsCatN(codepoint);
            break;
        case XML_REGEXP_NUMBER_DECIMAL:
            ret = XmlUnicode.ucsIsCatNd(codepoint);
            break;
        case XML_REGEXP_NUMBER_LETTER:
            ret = XmlUnicode.ucsIsCatNl(codepoint);
            break;
        case XML_REGEXP_NUMBER_OTHERS:
            ret = XmlUnicode.ucsIsCatNo(codepoint);
            break;
        case XML_REGEXP_PUNCT:
            ret = XmlUnicode.ucsIsCatP(codepoint);
            break;
        case XML_REGEXP_PUNCT_CONNECTOR:
            ret = XmlUnicode.ucsIsCatPc(codepoint);
            break;
        case XML_REGEXP_PUNCT_DASH:
            ret = XmlUnicode.ucsIsCatPd(codepoint);
            break;
        case XML_REGEXP_PUNCT_OPEN:
            ret = XmlUnicode.ucsIsCatPs(codepoint);
            break;
        case XML_REGEXP_PUNCT_CLOSE:
            ret = XmlUnicode.ucsIsCatPe(codepoint);
            break;
        case XML_REGEXP_PUNCT_INITQUOTE:
            ret = XmlUnicode.ucsIsCatPi(codepoint);
            break;
        case XML_REGEXP_PUNCT_FINQUOTE:
            ret = XmlUnicode.ucsIsCatPf(codepoint);
            break;
        case XML_REGEXP_PUNCT_OTHERS:
            ret = XmlUnicode.ucsIsCatPo(codepoint);
            break;
        case XML_REGEXP_SEPAR:
            ret = XmlUnicode.ucsIsCatZ(codepoint);
            break;
        case XML_REGEXP_SEPAR_SPACE:
            ret = XmlUnicode.ucsIsCatZs(codepoint);
            break;
        case XML_REGEXP_SEPAR_LINE:
            ret = XmlUnicode.ucsIsCatZl(codepoint);
            break;
        case XML_REGEXP_SEPAR_PARA:
            ret = XmlUnicode.ucsIsCatZp(codepoint);
            break;
        case XML_REGEXP_SYMBOL:
            ret = XmlUnicode.ucsIsCatS(codepoint);
            break;
        case XML_REGEXP_SYMBOL_MATH:
            ret = XmlUnicode.ucsIsCatSm(codepoint);
            break;
        case XML_REGEXP_SYMBOL_CURRENCY:
            ret = XmlUnicode.ucsIsCatSc(codepoint);
            break;
        case XML_REGEXP_SYMBOL_MODIFIER:
            ret = XmlUnicode.ucsIsCatSk(codepoint);
            break;
        case XML_REGEXP_SYMBOL_OTHERS:
            ret = XmlUnicode.ucsIsCatSo(codepoint);
            break;
        case XML_REGEXP_OTHER:
            ret = XmlUnicode.ucsIsCatC(codepoint);
            break;
        case XML_REGEXP_OTHER_CONTROL:
            ret = XmlUnicode.ucsIsCatCc(codepoint);
            break;
        case XML_REGEXP_OTHER_FORMAT:
            ret = XmlUnicode.ucsIsCatCf(codepoint);
            break;
        case XML_REGEXP_OTHER_PRIVATE:
            ret = XmlUnicode.ucsIsCatCo(codepoint);
            break;
        case XML_REGEXP_OTHER_NA:
            /* ret = xml_ucs_is_cat_cn(codepoint); */
            /* Seems it doesn't exist anymore in recent Unicode releases */
            ret = 0;
            break;
        case XML_REGEXP_BLOCK_NAME:
            ret = XmlUnicode.ucsIsBlock(codepoint, block_name);
            break;
        default:
            break;
        }
        if (neg != 0)
            return b(ret == 0);
        return ret;
    }

    // xml_reg_check_character
    /* Port of xmlRegCheckCharacter. */
    private static int regCheckCharacter(RegAtom atom, int codepoint)
    {
        int i, ret = 0;
        RegRange range;

        if ((atom == null) || (!regIsChar(codepoint)))
            return -1;

        switch (atom.type)
        {
        case XML_REGEXP_SUBREG:
        case XML_REGEXP_EPSILON:
            return -1;
        case XML_REGEXP_CHARVAL:
            return b(codepoint == atom.codepoint);
        case XML_REGEXP_RANGES:
        {
            int accept = 0;

            for (i = 0; i < atom.nb_ranges; i++)
            {
                range = atom.ranges[i];
                if (range.neg == 2)
                {
                    ret = regCheckCharacterRange(range.type, codepoint,
                                                 0, range.start, range.end,
                                                 range.block_name);
                    if (ret != 0)
                        return 0; /* excluded char */
                }
                else if (range.neg != 0)
                {
                    ret = regCheckCharacterRange(range.type, codepoint,
                                                 0, range.start, range.end,
                                                 range.block_name);
                    if (ret == 0)
                        accept = 1;
                    else
                        return 0;
                }
                else
                {
                    ret = regCheckCharacterRange(range.type, codepoint,
                                                 0, range.start, range.end,
                                                 range.block_name);
                    if (ret != 0)
                        accept = 1; /* might still be excluded */
                }
            }
            return accept;
        }
        case XML_REGEXP_STRING:
            return -1;
        case XML_REGEXP_ANYCHAR:
        case XML_REGEXP_ANYSPACE:
        case XML_REGEXP_NOTSPACE:
        case XML_REGEXP_INITNAME:
        case XML_REGEXP_NOTINITNAME:
        case XML_REGEXP_NAMECHAR:
        case XML_REGEXP_NOTNAMECHAR:
        case XML_REGEXP_DECIMAL:
        case XML_REGEXP_NOTDECIMAL:
        case XML_REGEXP_REALCHAR:
        case XML_REGEXP_NOTREALCHAR:
        case XML_REGEXP_LETTER:
        case XML_REGEXP_LETTER_UPPERCASE:
        case XML_REGEXP_LETTER_LOWERCASE:
        case XML_REGEXP_LETTER_TITLECASE:
        case XML_REGEXP_LETTER_MODIFIER:
        case XML_REGEXP_LETTER_OTHERS:
        case XML_REGEXP_MARK:
        case XML_REGEXP_MARK_NONSPACING:
        case XML_REGEXP_MARK_SPACECOMBINING:
        case XML_REGEXP_MARK_ENCLOSING:
        case XML_REGEXP_NUMBER:
        case XML_REGEXP_NUMBER_DECIMAL:
        case XML_REGEXP_NUMBER_LETTER:
        case XML_REGEXP_NUMBER_OTHERS:
        case XML_REGEXP_PUNCT:
        case XML_REGEXP_PUNCT_CONNECTOR:
        case XML_REGEXP_PUNCT_DASH:
        case XML_REGEXP_PUNCT_OPEN:
        case XML_REGEXP_PUNCT_CLOSE:
        case XML_REGEXP_PUNCT_INITQUOTE:
        case XML_REGEXP_PUNCT_FINQUOTE:
        case XML_REGEXP_PUNCT_OTHERS:
        case XML_REGEXP_SEPAR:
        case XML_REGEXP_SEPAR_SPACE:
        case XML_REGEXP_SEPAR_LINE:
        case XML_REGEXP_SEPAR_PARA:
        case XML_REGEXP_SYMBOL:
        case XML_REGEXP_SYMBOL_MATH:
        case XML_REGEXP_SYMBOL_CURRENCY:
        case XML_REGEXP_SYMBOL_MODIFIER:
        case XML_REGEXP_SYMBOL_OTHERS:
        case XML_REGEXP_OTHER:
        case XML_REGEXP_OTHER_CONTROL:
        case XML_REGEXP_OTHER_FORMAT:
        case XML_REGEXP_OTHER_PRIVATE:
        case XML_REGEXP_OTHER_NA:
        case XML_REGEXP_BLOCK_NAME:
            ret = regCheckCharacterRange(atom.type, codepoint, 0, 0, 0,
                                         (String) atom.valuep);
            if (atom.neg != 0)
                ret = b(ret == 0);
            break;
        default:
            break;
        }
        return ret;
    }

    /* ***********************************************************************
     *									*
     *	Saving and restoring state of an execution context		*
     *									*
     ************************************************************************/

    // xml_fa_reg_exec_save
    /* Port of xmlFARegExecSave. */
    private static void faRegExecSave(XmlRegExecCtxt exec)
    {
        if (exec.nb_push > MAX_PUSH)
        {
            exec.status = XML_REGEXP_INTERNAL_LIMIT;
            return;
        }
        exec.nb_push++;

        if (exec.nb_rollbacks >= exec.max_rollbacks)
        {
            int new_size;
            int len = exec.nb_rollbacks;

            new_size = regGrowCapacity(exec.max_rollbacks, 4, XML_MAX_ITEMS);
            if (new_size < 0)
            {
                exec.status = XML_REGEXP_OUT_OF_MEMORY;
                return;
            }
            exec.rollbacks = exec.rollbacks == null ? new RegExecRollback[new_size] : Arrays.copyOf(exec.rollbacks, new_size);
            exec.max_rollbacks = new_size;
            for (int k = len; k < exec.max_rollbacks; k++)
                exec.rollbacks[k] = new RegExecRollback();
        }
        exec.rollbacks[exec.nb_rollbacks].state = exec.state;
        exec.rollbacks[exec.nb_rollbacks].index = exec.index;
        exec.rollbacks[exec.nb_rollbacks].nextbranch = exec.transno + 1;
        if (exec.comp.nb_counters > 0)
        {
            if (exec.rollbacks[exec.nb_rollbacks].counts == null)
            {
                exec.rollbacks[exec.nb_rollbacks].counts = new int[exec.comp.nb_counters];
            }
            System.arraycopy(exec.counts, 0, exec.rollbacks[exec.nb_rollbacks].counts, 0,
                             exec.comp.nb_counters);
        }
        exec.nb_rollbacks++;
    }

    // xml_fa_reg_exec_roll_back
    /* Port of xmlFARegExecRollBack. */
    private static void faRegExecRollBack(XmlRegExecCtxt exec)
    {
        if (exec.status != XML_REGEXP_OK)
            return;
        if (exec.nb_rollbacks <= 0)
        {
            exec.status = XML_REGEXP_NOT_FOUND;
            return;
        }
        exec.nb_rollbacks--;
        exec.state = exec.rollbacks[exec.nb_rollbacks].state;
        exec.index = exec.rollbacks[exec.nb_rollbacks].index;
        exec.transno = exec.rollbacks[exec.nb_rollbacks].nextbranch;
        if (exec.comp.nb_counters > 0)
        {
            if (exec.rollbacks[exec.nb_rollbacks].counts == null)
            {
                exec.status = XML_REGEXP_INTERNAL_ERROR;
                return;
            }
            if (exec.counts != null)
            {
                System.arraycopy(exec.rollbacks[exec.nb_rollbacks].counts, 0, exec.counts, 0,
                                 exec.comp.nb_counters);
            }
        }
    }

    /* ***********************************************************************
     *									*
     *	Verifier, running an input against a compiled regexp		*
     *									*
     ************************************************************************/

    // xml_fa_reg_exec
    /* Port of xmlFARegExec. */
    private static int faRegExec(XmlRegexp comp, byte[] content)
    {
        XmlRegExecCtxt exec = new XmlRegExecCtxt();
        int ret, codepoint = 0, len, deter;
        int[] lenp = new int[1];

        exec.input_string = content;
        exec.index = 0;
        exec.nb_push = 0;
        exec.determinist = 1;
        exec.max_rollbacks = 0;
        exec.nb_rollbacks = 0;
        exec.rollbacks = null;
        exec.status = XML_REGEXP_OK;
        exec.comp = comp;
        exec.state = comp.states[0];
        exec.transno = 0;
        exec.transcount = 0;
        exec.input_stack = null;
        exec.input_stack_max = 0;
        if (comp.nb_counters > 0)
        {
            exec.counts = new int[comp.nb_counters];
        }
        else
            exec.counts = null;
        byte[] in = exec.input_string;
    outer:
        while ((exec.status == XML_REGEXP_OK) && (exec.state != null) &&
               ((at(in, exec.index) != 0) ||
                ((exec.state != null) &&
                 (exec.state.type != XML_REGEXP_FINAL_STATE))))
        {
            RegTrans trans;
            RegAtom atom;
            boolean do_rollback = false;

            /*
             * If end of input on non-terminal state, rollback, however we may
             * still have epsilon like transition for counted transitions
             * on counters, in that case don't break too early.  Additionally,
             * if we are working on a range like "AB{0,2}", where B is not present,
             * we don't want to break.
             */
            len = 1;
            if ((at(in, exec.index) == 0) && (exec.counts == null))
            {
                /*
                 * if there is a transition, we must check if
                 *  atom allows minOccurs of 0
                 */
                if (exec.transno < exec.state.nb_trans)
                {
                    trans = exec.state.trans[exec.transno];
                    if (trans.to >= 0)
                    {
                        atom = trans.atom;
                        if (!((atom.min == 0) && (atom.max > 0)))
                            do_rollback = true;
                    }
                }
                else
                    do_rollback = true;
            }

            if (!do_rollback)
            {
                exec.transcount = 0;
            transloop:
                for (; exec.transno < exec.state.nb_trans; exec.transno++)
                {
                    trans = exec.state.trans[exec.transno];
                    if (trans.to < 0)
                        continue;
                    atom = trans.atom;
                    ret = 0;
                    deter = 1;
                    if (trans.count >= 0)
                    {
                        int count;
                        RegCounter counter;

                        if (exec.counts == null)
                        {
                            exec.status = XML_REGEXP_INTERNAL_ERROR;
                            break outer;
                        }
                        /*
                         * A counted transition.
                         */

                        count = exec.counts[trans.count];
                        counter = exec.comp.counters[trans.count];
                        ret = b((count >= counter.min) && (count <= counter.max));
                        if ((ret != 0) && (counter.min != counter.max))
                            deter = 0;
                    }
                    else if (atom == null)
                    {
                        exec.status = XML_REGEXP_INTERNAL_ERROR;
                        break;
                    }
                    else if (at(in, exec.index) != 0)
                    {
                        lenp[0] = 4;
                        codepoint = regGetUtf8Char(in, exec.index, lenp);
                        len = lenp[0];
                        if (codepoint < 0)
                        {
                            exec.status = XML_REGEXP_INVALID_UTF8;
                            break outer;
                        }
                        ret = regCheckCharacter(atom, codepoint);
                        if ((ret == 1) && (atom.min >= 0) && (atom.max > 0))
                        {
                            XmlAutomataState to = comp.states[trans.to];

                            /*
                             * this is a multiple input sequence
                             * If there is a counter associated increment it now.
                             * do not increment if the counter is already over the
                             * maximum limit in which case get to next transition
                             */
                            if (trans.counter >= 0)
                            {
                                RegCounter counter;

                                if ((exec.counts == null) ||
                                    (exec.comp == null) ||
                                    (exec.comp.counters == null))
                                {
                                    exec.status = XML_REGEXP_INTERNAL_ERROR;
                                    break outer;
                                }
                                counter = exec.comp.counters[trans.counter];
                                if (exec.counts[trans.counter] >= counter.max)
                                    continue; /* for loop on transitions */
                            }
                            /* Save before incrementing */
                            if (exec.state.nb_trans > exec.transno + 1)
                            {
                                faRegExecSave(exec);
                                if (exec.status != XML_REGEXP_OK)
                                    break outer;
                            }
                            if (trans.counter >= 0)
                            {
                                exec.counts[trans.counter]++;
                            }
                            exec.transcount = 1;
                            do
                            {
                                /*
                                 * Try to progress as much as possible on the input
                                 */
                                if (exec.transcount == atom.max)
                                {
                                    break;
                                }
                                exec.index += len;
                                /*
                                 * End of input: stop here
                                 */
                                if (at(in, exec.index) == 0)
                                {
                                    exec.index -= len;
                                    break;
                                }
                                if (exec.transcount >= atom.min)
                                {
                                    int transno = exec.transno;
                                    XmlAutomataState state = exec.state;

                                    /*
                                     * The transition is acceptable save it
                                     */
                                    exec.transno = -1; /* trick */
                                    exec.state = to;
                                    faRegExecSave(exec);
                                    if (exec.status != XML_REGEXP_OK)
                                        break outer;
                                    exec.transno = transno;
                                    exec.state = state;
                                }
                                lenp[0] = 4;
                                codepoint = regGetUtf8Char(in, exec.index, lenp);
                                len = lenp[0];
                                if (codepoint < 0)
                                {
                                    exec.status = XML_REGEXP_INVALID_UTF8;
                                    break outer;
                                }
                                ret = regCheckCharacter(atom, codepoint);
                                exec.transcount++;
                            } while (ret == 1);
                            if (exec.transcount < atom.min)
                                ret = 0;

                            /*
                             * If the last check failed but one transition was found
                             * possible, rollback
                             */
                            if (ret < 0)
                                ret = 0;
                            if (ret == 0)
                            {
                                do_rollback = true;
                                break transloop;
                            }
                            if (trans.counter >= 0)
                            {
                                if (exec.counts == null)
                                {
                                    exec.status = XML_REGEXP_INTERNAL_ERROR;
                                    break outer;
                                }
                                exec.counts[trans.counter]--;
                            }
                        }
                        else if ((ret == 0) && (atom.min == 0) && (atom.max > 0))
                        {
                            /*
                             * we don't match on the codepoint, but minOccurs of 0
                             * says that's ok.  Setting len to 0 inhibits stepping
                             * over the codepoint.
                             */
                            exec.transcount = 1;
                            len = 0;
                            ret = 1;
                        }
                    }
                    else if ((atom.min == 0) && (atom.max > 0))
                    {
                        /* another spot to match when minOccurs is 0 */
                        exec.transcount = 1;
                        len = 0;
                        ret = 1;
                    }
                    if (ret == 1)
                    {
                        if ((trans.nd == 1) ||
                            ((trans.count >= 0) && (deter == 0) &&
                             (exec.state.nb_trans > exec.transno + 1)))
                        {
                            faRegExecSave(exec);
                            if (exec.status != XML_REGEXP_OK)
                                break outer;
                        }
                        if (trans.counter >= 0)
                        {
                            RegCounter counter;

                            /* make sure we don't go over the counter maximum value */
                            if ((exec.counts == null) ||
                                (exec.comp == null) ||
                                (exec.comp.counters == null))
                            {
                                exec.status = XML_REGEXP_INTERNAL_ERROR;
                                break outer;
                            }
                            counter = exec.comp.counters[trans.counter];
                            if (exec.counts[trans.counter] >= counter.max)
                                continue; /* for loop on transitions */
                            exec.counts[trans.counter]++;
                        }
                        if ((trans.count >= 0) &&
                            (trans.count < REGEXP_ALL_COUNTER))
                        {
                            if (exec.counts == null)
                            {
                                exec.status = XML_REGEXP_INTERNAL_ERROR;
                                break outer;
                            }
                            exec.counts[trans.count] = 0;
                        }
                        exec.state = comp.states[trans.to];
                        exec.transno = 0;
                        if (trans.atom != null)
                        {
                            exec.index += len;
                        }
                        /* goto progress */
                        continue outer;
                    }
                    else if (ret < 0)
                    {
                        exec.status = XML_REGEXP_INTERNAL_ERROR;
                        break;
                    }
                }
            }
            if (do_rollback || (exec.transno != 0) || (exec.state.nb_trans == 0))
            {
                /* rollback: */
                /*
                 * Failed to find a way out
                 */
                exec.determinist = 0;
                faRegExecRollBack(exec);
            }
            /* progress: */
        }
        /* error: */
        if (exec.state == null)
            return XML_REGEXP_INTERNAL_ERROR;
        if (exec.status == XML_REGEXP_OK)
            return 1;
        if (exec.status == XML_REGEXP_NOT_FOUND)
            return 0;
        return exec.status;
    }

    /* ***********************************************************************
     *									*
     *	Progressive interface to the verifier one atom at a time	*
     *									*
     ************************************************************************/

    /**
     * Port of xmlRegNewExecCtxt.
     * Build a context used for progressive evaluation of a regexp.
     * @param comp a precompiled regular expression
     * @param callback a callback function used for handling progresses in the
     *        automata matching phase
     * @param data the context data associated to the callback in this context
     * @return the new context
     */
    // xml_reg_exec_new
    public static XmlRegExecCtxt regExecNew(XmlRegexp comp, ExecCallback callback, Object data)
    {
        XmlRegExecCtxt exec;

        if (comp == null)
            return null;
        if ((comp.compact == null) && (comp.states == null))
            return null;
        exec = new XmlRegExecCtxt();
        exec.input_string = null;
        exec.index = 0;
        exec.determinist = 1;
        exec.max_rollbacks = 0;
        exec.nb_rollbacks = 0;
        exec.rollbacks = null;
        exec.status = XML_REGEXP_OK;
        exec.comp = comp;
        if (comp.compact == null)
            exec.state = comp.states[0];
        exec.transno = 0;
        exec.transcount = 0;
        exec.callback = callback;
        exec.data = data;
        if (comp.nb_counters > 0)
        {
            /*
             * For error handling, exec->counts is allocated twice the size
             * the second half is used to store the data in case of rollback
             * (here err_counts is a separate array of the same size).
             */
            exec.counts = new int[comp.nb_counters];
            exec.err_counts = new int[comp.nb_counters];
        }
        else
        {
            exec.counts = null;
            exec.err_counts = null;
        }
        exec.input_stack_max = 0;
        exec.input_stack_nr = 0;
        exec.input_stack = null;
        exec.err_state_no = -1;
        exec.err_string = null;
        exec.nb_push = 0;
        return exec;
    }

    /**
     * Port of xmlRegFreeExecCtxt.
     * Free the structures associated to a regular expression evaluation context.
     * @param exec a regular expression evaluation context
     */
    // xml_reg_exec_free
    public static void regExecFree(XmlRegExecCtxt exec)
    {
    }

    // xml_reg_exec_set_err_string
    /* Port of xmlRegExecSetErrString. */
    private static int regExecSetErrString(XmlRegExecCtxt exec, String value)
    {
        if (value == null)
        {
            exec.err_string = null;
        }
        else
        {
            exec.err_string = regStrdup(value);
        }
        return 0;
    }

    // xml_fa_reg_exec_save_input_string
    /* Port of xmlFARegExecSaveInputString. */
    private static void faRegExecSaveInputString(XmlRegExecCtxt exec, String value, Object data)
    {
        if (exec.input_stack_nr + 1 >= exec.input_stack_max)
        {
            int new_size;
            int old = exec.input_stack_max;

            new_size = regGrowCapacity(exec.input_stack_max, 4, XML_MAX_ITEMS);
            if (new_size < 0)
            {
                exec.status = XML_REGEXP_OUT_OF_MEMORY;
                return;
            }
            exec.input_stack = exec.input_stack == null ? new RegInputToken[new_size] : Arrays.copyOf(exec.input_stack, new_size);
            exec.input_stack_max = new_size;
            for (int k = old; k < new_size; k++)
                exec.input_stack[k] = new RegInputToken();
        }
        if (value == null)
        {
            exec.input_stack[exec.input_stack_nr].value = null;
        }
        else
        {
            exec.input_stack[exec.input_stack_nr].value = regStrdup(value);
        }
        exec.input_stack[exec.input_stack_nr].data = data;
        exec.input_stack_nr++;
        exec.input_stack[exec.input_stack_nr].value = null;
        exec.input_stack[exec.input_stack_nr].data = null;
    }

    // xml_reg_str_equal_wildcard
    /*
     * Port of xmlRegStrEqualWildcard.
     *
     * Checks if both strings are equal or have the same content. "*"
     * can be used as a wildcard in `val_str`; "|" is used as a separator of
     * substrings in both `expStr` and `val_str`.
     * returns 1 if the comparison is satisfied and the number of substrings
     * is equal, 0 otherwise.
     */
    private static int regStrEqualWildcard(String exp_str, String val_str)
    {
        if (exp_str == val_str) return 1;
        if (exp_str == null) return 0;
        if (val_str == null) return 0;
        /* e and v are the indexes of the C pointers into exp and val */
        String exp = exp_str, val = val_str;
        int e = 0, v = 0;
        do
        {
            /*
             * Eval if we have a wildcard for the current item.
             */
            if (at(exp, e) != at(val, v))
            {
                /* if one of them starts with a wildcard make val_str be it */
                if (at(val, v) == '*')
                {
                    String tmps = val;
                    int tmpi = v;
                    val = exp;
                    v = e;
                    exp = tmps;
                    e = tmpi;
                }
                if ((at(val, v) != 0) && (at(exp, e) != 0) && (at(exp, e++) == '*'))
                {
                    do
                    {
                        if (at(val, v) == XML_REG_STRING_SEPARATOR)
                            break;
                        v++;
                    } while (at(val, v) != 0);
                    continue;
                }
                else
                    return 0;
            }
            e++;
            v++;
        } while (at(val, v) != 0);
        if (at(exp, e) != 0)
            return 0;
        else
            return 1;
    }

    // xml_reg_compact_push_string
    /*
     * Port of xmlRegCompactPushString.
     * Push one input token in the execution context
     * returns 1 if the regexp reached a final state, 0 if non-final, and
     *     a negative value in case of error.
     */
    private static int regCompactPushString(XmlRegExecCtxt exec, XmlRegexp comp,
                                            String value, Object data)
    {
        int state = exec.index;
        int i, target;

        if ((comp == null) || (comp.compact == null) || (comp.string_map == null))
            return -1;

        if (value == null)
        {
            /*
             * are we at a final state ?
             */
            if (comp.compact[state * (comp.nbstrings + 1)] ==
                XML_REGEXP_FINAL_STATE)
                return 1;
            return 0;
        }

        /*
         * Examine all outside transitions from current state
         */
        for (i = 0; i < comp.nbstrings; i++)
        {
            target = comp.compact[state * (comp.nbstrings + 1) + i + 1];
            if ((target > 0) && (target <= comp.nbstates))
            {
                target--; /* to avoid 0 */
                if (regStrEqualWildcard(comp.string_map[i], value) != 0)
                {
                    exec.index = target;
                    if ((exec.callback != null) && (comp.transdata != null))
                    {
                        exec.callback.callback(exec, value,
                                               comp.transdata[state * comp.nbstrings + i], data);
                    }
                    if (comp.compact[target * (comp.nbstrings + 1)] ==
                        XML_REGEXP_SINK_STATE)
                        break; /* goto error */

                    if (comp.compact[target * (comp.nbstrings + 1)] ==
                        XML_REGEXP_FINAL_STATE)
                        return 1;
                    return 0;
                }
            }
        }
        /*
         * Failed to find an exit transition out from current state for the
         * current token
         */
        /* error: */
        exec.err_state_no = state;
        exec.status = XML_REGEXP_NOT_FOUND;
        regExecSetErrString(exec, value);
        return exec.status;
    }

    // xml_reg_exec_push_string_internal
    /*
     * Port of xmlRegExecPushStringInternal.
     * Push one input token in the execution context
     * compound: value was assembled from 2 strings
     * returns 1 if the regexp reached a final state, 0 if non-final, and
     *     a negative value in case of error.
     */
    private static int regExecPushStringInternal(XmlRegExecCtxt exec, String value,
                                                 Object data, int compound)
    {
        RegTrans trans;
        RegAtom atom;
        int ret;
        int final_ = 0;
        int progress = 1;
        int nonconsuming = 0;

        if (exec == null)
            return -1;
        int lastpush = exec.nb_push;
        if (exec.comp == null)
            return -1;
        if (exec.status != XML_REGEXP_OK)
            return exec.status;

        if (exec.comp.compact != null)
            return regCompactPushString(exec, exec.comp, value, data);

        if (value == null)
        {
            if (exec.state.type == XML_REGEXP_FINAL_STATE)
                return 1;
            final_ = 1;
        }

        /*
         * If we have an active rollback stack push the new value there
         * and get back to where we were left
         */
        if ((value != null) && (exec.input_stack_nr > 0))
        {
            faRegExecSaveInputString(exec, value, data);
            value = exec.input_stack[exec.index].value;
            data = exec.input_stack[exec.index].data;
        }

    outer:
        while ((exec.status == XML_REGEXP_OK) &&
               ((value != null) ||
                ((final_ == 1) &&
                 (exec.state.type != XML_REGEXP_FINAL_STATE))))
        {
            boolean do_rollback = false;

            /*
             * End of input on non-terminal state, rollback, however we may
             * still have epsilon like transition for counted transitions
             * on counters, in that case don't break too early.
             */
            if ((value == null) && (exec.counts == null))
                do_rollback = true;

            if (!do_rollback)
            {
                exec.transcount = 0;
            transloop:
                for (; exec.transno < exec.state.nb_trans; exec.transno++)
                {
                    trans = exec.state.trans[exec.transno];
                    if (trans.to < 0)
                        continue;
                    atom = trans.atom;
                    ret = 0;
                    if (trans.count == REGEXP_ALL_LAX_COUNTER)
                    {
                        int i;
                        int count;
                        RegTrans t;
                        RegCounter counter;

                        ret = 0;

                        /*
                         * Check all counted transitions from the current state
                         */
                        if ((value == null) && (final_ != 0))
                        {
                            ret = 1;
                        }
                        else if (value != null)
                        {
                            for (i = 0; i < exec.state.nb_trans; i++)
                            {
                                t = exec.state.trans[i];
                                if ((t.counter < 0) || (t == trans))
                                    continue;
                                counter = exec.comp.counters[t.counter];
                                count = exec.counts[t.counter];
                                if ((count < counter.max) &&
                                    (t.atom != null) &&
                                    (regStrEqual(value, (String) t.atom.valuep) != 0))
                                {
                                    ret = 0;
                                    break;
                                }
                                if ((count >= counter.min) &&
                                    (count < counter.max) &&
                                    (t.atom != null) &&
                                    (regStrEqual(value, (String) t.atom.valuep) != 0))
                                {
                                    ret = 1;
                                    break;
                                }
                            }
                        }
                    }
                    else if (trans.count == REGEXP_ALL_COUNTER)
                    {
                        int i;
                        int count;
                        RegTrans t;
                        RegCounter counter;

                        ret = 1;

                        /*
                         * Check all counted transitions from the current state
                         */
                        for (i = 0; i < exec.state.nb_trans; i++)
                        {
                            t = exec.state.trans[i];
                            if ((t.counter < 0) || (t == trans))
                                continue;
                            counter = exec.comp.counters[t.counter];
                            count = exec.counts[t.counter];
                            if ((count < counter.min) || (count > counter.max))
                            {
                                ret = 0;
                                break;
                            }
                        }
                    }
                    else if (trans.count >= 0)
                    {
                        int count;
                        RegCounter counter;

                        /*
                         * A counted transition.
                         */

                        count = exec.counts[trans.count];
                        counter = exec.comp.counters[trans.count];
                        ret = b((count >= counter.min) && (count <= counter.max));
                    }
                    else if (atom == null)
                    {
                        exec.status = XML_REGEXP_INTERNAL_ERROR;
                        break;
                    }
                    else if (value != null)
                    {
                        ret = regStrEqualWildcard((String) atom.valuep, value);
                        if (atom.neg != 0)
                        {
                            ret = b(ret == 0);
                            if (compound == 0)
                                ret = 0;
                        }
                        if ((ret == 1) && (trans.counter >= 0))
                        {
                            RegCounter counter;
                            int count;

                            count = exec.counts[trans.counter];
                            counter = exec.comp.counters[trans.counter];
                            if (count >= counter.max)
                                ret = 0;
                        }

                        if ((ret == 1) && (atom.min > 0) && (atom.max > 0))
                        {
                            XmlAutomataState to = exec.comp.states[trans.to];

                            /*
                             * this is a multiple input sequence
                             */
                            if (exec.state.nb_trans > exec.transno + 1)
                            {
                                if (exec.input_stack_nr <= 0)
                                {
                                    faRegExecSaveInputString(exec, value, data);
                                }
                                faRegExecSave(exec);
                            }
                            exec.transcount = 1;
                            do
                            {
                                /*
                                 * Try to progress as much as possible on the input
                                 */
                                if (exec.transcount == atom.max)
                                {
                                    break;
                                }
                                /*
                                 * libxml2 2.16 dereferences NULL here: when the state has
                                 * a single transition nothing was saved and there is no
                                 * input stack. No further input has been pushed then, so
                                 * stop like at the end of the input stack below.
                                 */
                                if (exec.input_stack == null)
                                    break;
                                exec.index++;
                                value = exec.input_stack[exec.index].value;
                                data = exec.input_stack[exec.index].data;

                                /*
                                 * End of input: stop here
                                 */
                                if (value == null)
                                {
                                    exec.index--;
                                    break;
                                }
                                if (exec.transcount >= atom.min)
                                {
                                    int transno = exec.transno;
                                    XmlAutomataState state = exec.state;

                                    /*
                                     * The transition is acceptable save it
                                     */
                                    exec.transno = -1; /* trick */
                                    exec.state = to;
                                    if (exec.input_stack_nr <= 0)
                                    {
                                        faRegExecSaveInputString(exec, value, data);
                                    }
                                    faRegExecSave(exec);
                                    exec.transno = transno;
                                    exec.state = state;
                                }
                                ret = regStrEqual(value, (String) atom.valuep);
                                exec.transcount++;
                            } while (ret == 1);
                            if (exec.transcount < atom.min)
                                ret = 0;

                            /*
                             * If the last check failed but one transition was found
                             * possible, rollback
                             */
                            if (ret < 0)
                                ret = 0;
                            if (ret == 0)
                            {
                                do_rollback = true;
                                break transloop;
                            }
                        }
                    }
                    if (ret == 1)
                    {
                        if ((exec.callback != null) && (atom != null) &&
                            (data != null))
                        {
                            exec.callback.callback(exec, (String) atom.valuep,
                                                   atom.data, data);
                        }
                        if (exec.state.nb_trans > exec.transno + 1)
                        {
                            if (exec.input_stack_nr <= 0)
                            {
                                faRegExecSaveInputString(exec, value, data);
                            }
                            faRegExecSave(exec);
                        }
                        if (trans.counter >= 0)
                        {
                            exec.counts[trans.counter]++;
                        }
                        if ((trans.count >= 0) &&
                            (trans.count < REGEXP_ALL_COUNTER))
                        {
                            exec.counts[trans.count] = 0;
                        }
                        if ((exec.comp.states[trans.to] != null) &&
                            (exec.comp.states[trans.to].type ==
                             XML_REGEXP_SINK_STATE))
                        {
                            /*
                             * entering a sink state, save the current state as error
                             * state.
                             */
                            if (regExecSetErrString(exec, value) < 0)
                                break outer;
                            exec.err_state = exec.state;
                            /* libxml2 copies also when there are no counters (NULL pointers). */
                            if (exec.comp.nb_counters != 0)
                                System.arraycopy(exec.counts, 0, exec.err_counts, 0,
                                                 exec.comp.nb_counters);
                        }
                        exec.state = exec.comp.states[trans.to];
                        exec.transno = 0;
                        /*
                         * libxml2 2.16 loops forever here on a cycle of all or counter
                         * transitions (no input consumed, no rollback saved). Such a
                         * transition only resets counters, so without a cycle at most
                         * nb_states * (nb_counters + 1) of them can follow each other
                         * before an input is consumed, a rollback is saved or one is
                         * taken, stop with XML_REGEXP_INTERNAL_LIMIT after that. A cycle
                         * that saves rollbacks is left alone, libxml2 ends it with the
                         * same error at MAX_PUSH.
                         */
                        if (exec.nb_push != lastpush)
                        {
                            nonconsuming = 0;
                            lastpush = exec.nb_push;
                        }
                        if (trans.atom == null)
                        {
                            nonconsuming++;
                            if (nonconsuming > exec.comp.nb_states *
                                (exec.comp.nb_counters + 1) + 1)
                            {
                                exec.status = XML_REGEXP_INTERNAL_LIMIT;
                                break outer;
                            }
                        }
                        else
                        {
                            nonconsuming = 0;
                        }
                        if (trans.atom != null)
                        {
                            if (exec.input_stack != null)
                            {
                                exec.index++;
                                if (exec.index < exec.input_stack_nr)
                                {
                                    value = exec.input_stack[exec.index].value;
                                    data = exec.input_stack[exec.index].data;
                                }
                                else
                                {
                                    value = null;
                                    data = null;
                                }
                            }
                            else
                            {
                                value = null;
                                data = null;
                            }
                        }
                        /* goto progress */
                        progress = 1;
                        continue outer;
                    }
                    else if (ret < 0)
                    {
                        exec.status = XML_REGEXP_INTERNAL_ERROR;
                        break;
                    }
                }
            }
            if (do_rollback || (exec.transno != 0) || (exec.state.nb_trans == 0))
            {
                /* rollback: */
                /*
                 * if we didn't yet rollback on the current input
                 * store the current state as the error state.
                 */
                if ((progress != 0) && (exec.state != null) &&
                    (exec.state.type != XML_REGEXP_SINK_STATE))
                {
                    progress = 0;
                    if (regExecSetErrString(exec, value) < 0)
                        break;
                    exec.err_state = exec.state;
                    if (exec.comp.nb_counters != 0)
                        System.arraycopy(exec.counts, 0, exec.err_counts, 0,
                                         exec.comp.nb_counters);
                }

                /*
                 * Failed to find a way out
                 */
                exec.determinist = 0;
                nonconsuming = 0;
                faRegExecRollBack(exec);
                if ((exec.input_stack != null) &&
                    (exec.status == XML_REGEXP_OK))
                {
                    value = exec.input_stack[exec.index].value;
                    data = exec.input_stack[exec.index].data;
                }
            }
            continue;
        }
        if (exec.status == XML_REGEXP_OK)
        {
            return b(exec.state.type == XML_REGEXP_FINAL_STATE);
        }
        return exec.status;
    }

    /**
     * Port of xmlRegExecPushString.
     * Push one input token in the execution context
     * @param exec a regexp execution context or null to indicate the end
     * @param value a string token input
     * @param data data associated to the token to reuse in callbacks
     * @return 1 if the regexp reached a final state, 0 if non-final, and
     *     a negative value in case of error.
     */
    // xml_reg_exec_push_string
    public static int regExecPushString(XmlRegExecCtxt exec, String value, Object data)
    {
        return regExecPushStringInternal(exec, value, data, 0);
    }

    /**
     * Port of xmlRegExecPushString2.
     * Push one input token in the execution context
     * @param exec a regexp execution context or null to indicate the end
     * @param value the first string token input
     * @param value2 the second string token input
     * @param data data associated to the token to reuse in callbacks
     * @return 1 if the regexp reached a final state, 0 if non-final, and
     *     a negative value in case of error.
     */
    // xml_reg_exec_push_string2
    public static int regExecPushString2(XmlRegExecCtxt exec, String value,
                                         String value2, Object data)
    {
        int ret;
        String str;

        if (exec == null)
            return -1;
        if (exec.comp == null)
            return -1;
        if (exec.status != XML_REGEXP_OK)
            return exec.status;

        if (value2 == null)
            return regExecPushString(exec, value, data);

        str = value + (char) XML_REG_STRING_SEPARATOR + value2;

        if (exec.comp.compact != null)
            ret = regCompactPushString(exec, exec.comp, str, data);
        else
            ret = regExecPushStringInternal(exec, str, data, 1);

        return ret;
    }

    // xml_reg_exec_get_values
    /*
     * Port of xmlRegExecGetValues.
     *
     * Extract information from the regexp execution. Internal routine to
     * implement #xml_reg_exec_next_values and #xml_reg_exec_err_info
     * returns 0 in case of success or -1 in case of error.
     */
    private static int regExecGetValues(XmlRegExecCtxt exec, int err,
                                        int[] nbval, int[] nbneg,
                                        String[] values, int[] terminal)
    {
        int maxval;
        int nb = 0;

        if ((exec == null) || (nbval == null) || (nbneg == null) ||
            (values == null) || (nbval[0] <= 0))
            return -1;

        maxval = nbval[0];
        nbval[0] = 0;
        nbneg[0] = 0;
        if ((exec.comp != null) && (exec.comp.compact != null))
        {
            XmlRegexp comp;
            int target, i, state;

            comp = exec.comp;

            if (err != 0)
            {
                if (exec.err_state_no == -1) return -1;
                state = exec.err_state_no;
            }
            else
            {
                state = exec.index;
            }
            if (terminal != null)
            {
                if (comp.compact[state * (comp.nbstrings + 1)] ==
                    XML_REGEXP_FINAL_STATE)
                    terminal[0] = 1;
                else
                    terminal[0] = 0;
            }
            for (i = 0; (i < comp.nbstrings) && (nb < maxval); i++)
            {
                target = comp.compact[state * (comp.nbstrings + 1) + i + 1];
                if ((target > 0) && (target <= comp.nbstates) &&
                    (comp.compact[(target - 1) * (comp.nbstrings + 1)] !=
                     XML_REGEXP_SINK_STATE))
                {
                    values[nb++] = comp.string_map[i];
                    nbval[0]++;
                }
            }
            for (i = 0; (i < comp.nbstrings) && (nb < maxval); i++)
            {
                target = comp.compact[state * (comp.nbstrings + 1) + i + 1];
                if ((target > 0) && (target <= comp.nbstates) &&
                    (comp.compact[(target - 1) * (comp.nbstrings + 1)] ==
                     XML_REGEXP_SINK_STATE))
                {
                    values[nb++] = comp.string_map[i];
                    nbneg[0]++;
                }
            }
        }
        else
        {
            int transno;
            RegTrans trans;
            RegAtom atom;
            XmlAutomataState state;

            if (terminal != null)
            {
                if ((exec.state != null) && (exec.state.type == XML_REGEXP_FINAL_STATE))
                    terminal[0] = 1;
                else
                    terminal[0] = 0;
            }

            if (err != 0)
            {
                if (exec.err_state == null) return -1;
                state = exec.err_state;
            }
            else
            {
                if (exec.state == null) return -1;
                state = exec.state;
            }
            for (transno = 0;
                 (transno < state.nb_trans) && (nb < maxval);
                 transno++)
            {
                trans = state.trans[transno];
                if (trans.to < 0)
                    continue;
                atom = trans.atom;
                if ((atom == null) || (atom.valuep == null))
                    continue;
                if (trans.count == REGEXP_ALL_LAX_COUNTER)
                {
                    /* this should not be reached but ... */
                }
                else if (trans.count == REGEXP_ALL_COUNTER)
                {
                    /* this should not be reached but ... */
                }
                else if (trans.counter >= 0)
                {
                    RegCounter counter = null;
                    int count;

                    if (err != 0)
                        count = exec.err_counts[trans.counter];
                    else
                        count = exec.counts[trans.counter];
                    if (exec.comp != null)
                        counter = exec.comp.counters[trans.counter];
                    if ((counter == null) || (count < counter.max))
                    {
                        if (atom.neg != 0)
                            values[nb++] = (String) atom.valuep2;
                        else
                            values[nb++] = (String) atom.valuep;
                        nbval[0]++;
                    }
                }
                else
                {
                    if ((exec.comp != null) && (exec.comp.states[trans.to] != null) &&
                        (exec.comp.states[trans.to].type !=
                         XML_REGEXP_SINK_STATE))
                    {
                        if (atom.neg != 0)
                            values[nb++] = (String) atom.valuep2;
                        else
                            values[nb++] = (String) atom.valuep;
                        nbval[0]++;
                    }
                }
            }
            for (transno = 0;
                 (transno < state.nb_trans) && (nb < maxval);
                 transno++)
            {
                trans = state.trans[transno];
                if (trans.to < 0)
                    continue;
                atom = trans.atom;
                if ((atom == null) || (atom.valuep == null))
                    continue;
                if (trans.count == REGEXP_ALL_LAX_COUNTER)
                {
                    continue;
                }
                else if (trans.count == REGEXP_ALL_COUNTER)
                {
                    continue;
                }
                else if (trans.counter >= 0)
                {
                    continue;
                }
                else
                {
                    if ((exec.comp.states[trans.to] != null) &&
                        (exec.comp.states[trans.to].type ==
                         XML_REGEXP_SINK_STATE))
                    {
                        if (atom.neg != 0)
                            values[nb++] = (String) atom.valuep2;
                        else
                            values[nb++] = (String) atom.valuep;
                        nbneg[0]++;
                    }
                }
            }
        }
        return 0;
    }

    /**
     * Port of xmlRegExecNextValues.
     * Extract information from the regexp execution.
     * The parameter `values` must point to an array of `nbval` string pointers
     * on return nbval will contain the number of possible strings in that
     * state and the `values` array will be updated with them.
     * @param exec a regexp execution context
     * @param nbval pointer to the number of accepted values IN/OUT
     * @param nbneg return number of negative transitions
     * @param values pointer to the array of acceptable values
     * @param terminal return value if this was a terminal state
     * @return 0 in case of success or -1 in case of error.
     */
    // xml_reg_exec_next_values
    public static int regExecNextValues(XmlRegExecCtxt exec, int[] nbval, int[] nbneg,
                                        String[] values, int[] terminal)
    {
        return regExecGetValues(exec, 0, nbval, nbneg, values, terminal);
    }

    /**
     * Port of xmlRegExecErrInfo.
     * Extract error information from the regexp execution. The parameter
     * `string` will be updated with the value pushed and not accepted,
     * the parameter `values` must point to an array of `nbval` string pointers
     * on return nbval will contain the number of possible strings in that
     * state and the `values` array will be updated with them.
     * @param exec a regexp execution context generating an error
     * @param string return value for the error string
     * @param nbval pointer to the number of accepted values IN/OUT
     * @param nbneg return number of negative transitions
     * @param values pointer to the array of acceptable values
     * @param terminal return value if this was a terminal state
     * @return 0 in case of success or -1 in case of error.
     */
    // xml_reg_exec_err_info
    public static int regExecErrInfo(XmlRegExecCtxt exec, String[] string,
                                     int[] nbval, int[] nbneg, String[] values, int[] terminal)
    {
        if (exec == null)
            return -1;
        if (string != null)
        {
            if (exec.status != XML_REGEXP_OK)
                string[0] = exec.err_string;
            else
                string[0] = null;
        }
        return regExecGetValues(exec, 1, nbval, nbneg, values, terminal);
    }

    /**
     * Port of xmlRegExecClearErrors.
     * Clear errors in the context, allowing to recover
     * from errors on specific scenarios
     * @param exec a regexp execution context
     */
    // xml_reg_exec_clear_errors
    public static void regExecClearErrors(XmlRegExecCtxt exec)
    {
        exec.status = 0;
        exec.err_state = null;
        exec.err_state_no = -1;
        exec.err_string = null;
    }

    /* ***********************************************************************
     *									*
     *	Parser for the Schemas Datatype Regular Expressions		*
     *	http://www.w3.org/TR/2001/REC-xmlschema-2-20010502/#regexs	*
     *									*
     ************************************************************************/

    // xml_fa_is_char
    /*
     * Port of xmlFAIsChar.
     * [10]   Char   ::=   [^.\?*+()|\#x5B\#x5D]
     */
    private static int faIsChar(XmlAutomata ctxt)
    {
        int cur;
        int[] len = new int[1];

        len[0] = 4;
        cur = regGetUtf8Char(ctxt.string, ctxt.cur, len);
        if (cur < 0)
        {
            ERROR(ctxt, "Invalid UTF-8");
            return 0;
        }
        if ((cur == '.') || (cur == '\\') || (cur == '?') ||
            (cur == '*') || (cur == '+') || (cur == '(') ||
            (cur == ')') || (cur == '|') || (cur == 0x5B) ||
            (cur == 0x5D) || (cur == 0))
            return -1;
        return cur;
    }

    // xml_fa_parse_char_prop
    /*
     * Port of xmlFAParseCharProp.
     *
     * [27]   charProp   ::=   IsCategory | IsBlock
     * [28]   IsCategory ::= Letters | Marks | Numbers | Punctuation |
     *                       Separators | Symbols | Others
     * [29]   Letters   ::=   'L' [ultmo]?
     * [30]   Marks   ::=   'M' [nce]?
     * [31]   Numbers   ::=   'N' [dlo]?
     * [32]   Punctuation   ::=   'P' [cdseifo]?
     * [33]   Separators   ::=   'Z' [slp]?
     * [34]   Symbols   ::=   'S' [mcko]?
     * [35]   Others   ::=   'C' [cfon]?
     * [36]   IsBlock   ::=   'Is' [a-zA-Z0-9\#x2D]+
     */
    private static void faParseCharProp(XmlAutomata ctxt)
    {
        int cur;
        int type = 0;
        String block_name = null;

        cur = CUR(ctxt);
        if (cur == 'L')
        {
            NEXT(ctxt);
            cur = CUR(ctxt);
            if (cur == 'u')
            {
                NEXT(ctxt);
                type = XML_REGEXP_LETTER_UPPERCASE;
            }
            else if (cur == 'l')
            {
                NEXT(ctxt);
                type = XML_REGEXP_LETTER_LOWERCASE;
            }
            else if (cur == 't')
            {
                NEXT(ctxt);
                type = XML_REGEXP_LETTER_TITLECASE;
            }
            else if (cur == 'm')
            {
                NEXT(ctxt);
                type = XML_REGEXP_LETTER_MODIFIER;
            }
            else if (cur == 'o')
            {
                NEXT(ctxt);
                type = XML_REGEXP_LETTER_OTHERS;
            }
            else
            {
                type = XML_REGEXP_LETTER;
            }
        }
        else if (cur == 'M')
        {
            NEXT(ctxt);
            cur = CUR(ctxt);
            if (cur == 'n')
            {
                NEXT(ctxt);
                /* nonspacing */
                type = XML_REGEXP_MARK_NONSPACING;
            }
            else if (cur == 'c')
            {
                NEXT(ctxt);
                /* spacing combining */
                type = XML_REGEXP_MARK_SPACECOMBINING;
            }
            else if (cur == 'e')
            {
                NEXT(ctxt);
                /* enclosing */
                type = XML_REGEXP_MARK_ENCLOSING;
            }
            else
            {
                /* all marks */
                type = XML_REGEXP_MARK;
            }
        }
        else if (cur == 'N')
        {
            NEXT(ctxt);
            cur = CUR(ctxt);
            if (cur == 'd')
            {
                NEXT(ctxt);
                /* digital */
                type = XML_REGEXP_NUMBER_DECIMAL;
            }
            else if (cur == 'l')
            {
                NEXT(ctxt);
                /* letter */
                type = XML_REGEXP_NUMBER_LETTER;
            }
            else if (cur == 'o')
            {
                NEXT(ctxt);
                /* other */
                type = XML_REGEXP_NUMBER_OTHERS;
            }
            else
            {
                /* all numbers */
                type = XML_REGEXP_NUMBER;
            }
        }
        else if (cur == 'P')
        {
            NEXT(ctxt);
            cur = CUR(ctxt);
            if (cur == 'c')
            {
                NEXT(ctxt);
                /* connector */
                type = XML_REGEXP_PUNCT_CONNECTOR;
            }
            else if (cur == 'd')
            {
                NEXT(ctxt);
                /* dash */
                type = XML_REGEXP_PUNCT_DASH;
            }
            else if (cur == 's')
            {
                NEXT(ctxt);
                /* open */
                type = XML_REGEXP_PUNCT_OPEN;
            }
            else if (cur == 'e')
            {
                NEXT(ctxt);
                /* close */
                type = XML_REGEXP_PUNCT_CLOSE;
            }
            else if (cur == 'i')
            {
                NEXT(ctxt);
                /* initial quote */
                type = XML_REGEXP_PUNCT_INITQUOTE;
            }
            else if (cur == 'f')
            {
                NEXT(ctxt);
                /* final quote */
                type = XML_REGEXP_PUNCT_FINQUOTE;
            }
            else if (cur == 'o')
            {
                NEXT(ctxt);
                /* other */
                type = XML_REGEXP_PUNCT_OTHERS;
            }
            else
            {
                /* all punctuation */
                type = XML_REGEXP_PUNCT;
            }
        }
        else if (cur == 'Z')
        {
            NEXT(ctxt);
            cur = CUR(ctxt);
            if (cur == 's')
            {
                NEXT(ctxt);
                /* space */
                type = XML_REGEXP_SEPAR_SPACE;
            }
            else if (cur == 'l')
            {
                NEXT(ctxt);
                /* line */
                type = XML_REGEXP_SEPAR_LINE;
            }
            else if (cur == 'p')
            {
                NEXT(ctxt);
                /* paragraph */
                type = XML_REGEXP_SEPAR_PARA;
            }
            else
            {
                /* all separators */
                type = XML_REGEXP_SEPAR;
            }
        }
        else if (cur == 'S')
        {
            NEXT(ctxt);
            cur = CUR(ctxt);
            if (cur == 'm')
            {
                NEXT(ctxt);
                type = XML_REGEXP_SYMBOL_MATH;
                /* math */
            }
            else if (cur == 'c')
            {
                NEXT(ctxt);
                type = XML_REGEXP_SYMBOL_CURRENCY;
                /* currency */
            }
            else if (cur == 'k')
            {
                NEXT(ctxt);
                type = XML_REGEXP_SYMBOL_MODIFIER;
                /* modifiers */
            }
            else if (cur == 'o')
            {
                NEXT(ctxt);
                type = XML_REGEXP_SYMBOL_OTHERS;
                /* other */
            }
            else
            {
                /* all symbols */
                type = XML_REGEXP_SYMBOL;
            }
        }
        else if (cur == 'C')
        {
            NEXT(ctxt);
            cur = CUR(ctxt);
            if (cur == 'c')
            {
                NEXT(ctxt);
                /* control */
                type = XML_REGEXP_OTHER_CONTROL;
            }
            else if (cur == 'f')
            {
                NEXT(ctxt);
                /* format */
                type = XML_REGEXP_OTHER_FORMAT;
            }
            else if (cur == 'o')
            {
                NEXT(ctxt);
                /* private use */
                type = XML_REGEXP_OTHER_PRIVATE;
            }
            else if (cur == 'n')
            {
                NEXT(ctxt);
                /* not assigned */
                type = XML_REGEXP_OTHER_NA;
            }
            else
            {
                /* all others */
                type = XML_REGEXP_OTHER;
            }
        }
        else if (cur == 'I')
        {
            int start;
            NEXT(ctxt);
            cur = CUR(ctxt);
            if (cur != 's')
            {
                ERROR(ctxt, "IsXXXX expected");
                return;
            }
            NEXT(ctxt);
            start = ctxt.cur;
            cur = CUR(ctxt);
            if (((cur >= 'a') && (cur <= 'z')) ||
                ((cur >= 'A') && (cur <= 'Z')) ||
                ((cur >= '0') && (cur <= '9')) ||
                (cur == 0x2D))
            {
                NEXT(ctxt);
                cur = CUR(ctxt);
                while (((cur >= 'a') && (cur <= 'z')) ||
                       ((cur >= 'A') && (cur <= 'Z')) ||
                       ((cur >= '0') && (cur <= '9')) ||
                       (cur == 0x2D))
                {
                    NEXT(ctxt);
                    cur = CUR(ctxt);
                }
            }
            type = XML_REGEXP_BLOCK_NAME;
            block_name = regStrndup(ctxt.string, start, ctxt.cur - start);
            if (block_name == null)
                regexpErrMemory(ctxt);
        }
        else
        {
            ERROR(ctxt, "Unknown char property");
            return;
        }
        if (ctxt.atom == null)
        {
            ctxt.atom = regNewAtom(ctxt, type);
            if (ctxt.atom == null)
            {
                return;
            }
            ctxt.atom.valuep = block_name;
        }
        else if (ctxt.atom.type == XML_REGEXP_RANGES)
        {
            regAtomAddRange(ctxt, ctxt.atom, ctxt.neg, type, 0, 0, block_name);
        }
    }

    // xml_reg_parse_escaped_codeunit
    /* Port of parse_escaped_codeunit. */
    private static int regParseEscapedCodeunit(XmlAutomata ctxt)
    {
        int val = 0, i, cur;
        for (i = 0; i < 4; i++)
        {
            NEXT(ctxt);
            val *= 16;
            cur = CUR(ctxt);
            if (cur >= '0' && cur <= '9')
            {
                val += cur - '0';
            }
            else if (cur >= 'A' && cur <= 'F')
            {
                val += cur - 'A' + 10;
            }
            else if (cur >= 'a' && cur <= 'f')
            {
                val += cur - 'a' + 10;
            }
            else
            {
                ERROR(ctxt, "Expecting hex digit");
                return -1;
            }
        }
        return val;
    }

    // xml_reg_parse_escaped_codepoint
    /* Port of parse_escaped_codepoint. */
    private static int regParseEscapedCodepoint(XmlAutomata ctxt)
    {
        int val = regParseEscapedCodeunit(ctxt);
        if (0xD800 <= val && val <= 0xDBFF)
        {
            NEXT(ctxt);
            if (CUR(ctxt) == '\\')
            {
                NEXT(ctxt);
                if (CUR(ctxt) == 'u')
                {
                    int low = regParseEscapedCodeunit(ctxt);
                    if (0xDC00 <= low && low <= 0xDFFF)
                    {
                        return (val - 0xD800) * 0x400 + (low - 0xDC00) + 0x10000;
                    }
                }
            }
            ERROR(ctxt, "Invalid low surrogate pair code unit");
            val = -1;
        }
        return val;
    }

    // xml_fa_parse_char_class_esc
    /*
     * Port of xmlFAParseCharClassEsc.
     *
     * [23] charClassEsc ::= ( SingleCharEsc | MultiCharEsc | catEsc | complEsc )
     * [24] SingleCharEsc ::= '\' [nrt\|.?*+(){}\#x2D\#x5B\#x5D\#x5E]
     * [25] catEsc   ::=   '\p{' charProp '}'
     * [26] complEsc ::=   '\P{' charProp '}'
     * [37] MultiCharEsc ::= '.' | ('\' [sSiIcCdDwW])
     */
    private static void faParseCharClassEsc(XmlAutomata ctxt)
    {
        int cur;

        if (CUR(ctxt) == '.')
        {
            if (ctxt.atom == null)
            {
                ctxt.atom = regNewAtom(ctxt, XML_REGEXP_ANYCHAR);
            }
            else if (ctxt.atom.type == XML_REGEXP_RANGES)
            {
                regAtomAddRange(ctxt, ctxt.atom, ctxt.neg,
                                XML_REGEXP_ANYCHAR, 0, 0, null);
            }
            NEXT(ctxt);
            return;
        }
        if (CUR(ctxt) != '\\')
        {
            ERROR(ctxt, "Escaped sequence: expecting \\");
            return;
        }
        NEXT(ctxt);
        cur = CUR(ctxt);
        if (cur == 'p')
        {
            NEXT(ctxt);
            if (CUR(ctxt) != '{')
            {
                ERROR(ctxt, "Expecting '{'");
                return;
            }
            NEXT(ctxt);
            faParseCharProp(ctxt);
            if (CUR(ctxt) != '}')
            {
                ERROR(ctxt, "Expecting '}'");
                return;
            }
            NEXT(ctxt);
        }
        else if (cur == 'P')
        {
            NEXT(ctxt);
            if (CUR(ctxt) != '{')
            {
                ERROR(ctxt, "Expecting '{'");
                return;
            }
            NEXT(ctxt);
            faParseCharProp(ctxt);
            if (ctxt.atom != null)
                ctxt.atom.neg = 1;
            if (CUR(ctxt) != '}')
            {
                ERROR(ctxt, "Expecting '}'");
                return;
            }
            NEXT(ctxt);
        }
        else if ((cur == 'n') || (cur == 'r') || (cur == 't') || (cur == '\\') ||
                 (cur == '|') || (cur == '.') || (cur == '?') || (cur == '*') ||
                 (cur == '+') || (cur == '(') || (cur == ')') || (cur == '{') ||
                 (cur == '}') || (cur == 0x2D) || (cur == 0x5B) || (cur == 0x5D) ||
                 (cur == 0x5E) ||

                 /* Non-standard escape sequences:
                  *                  Java 1.8|.NET Core 3.1|MSXML 6 */
                 (cur == '!') ||     /*   +  |     +       |    +   */
                 (cur == '"') ||     /*   +  |     +       |    +   */
                 (cur == '#') ||     /*   +  |     +       |    +   */
                 (cur == '$') ||     /*   +  |     +       |    +   */
                 (cur == '%') ||     /*   +  |     +       |    +   */
                 (cur == ',') ||     /*   +  |     +       |    +   */
                 (cur == '/') ||     /*   +  |     +       |    +   */
                 (cur == ':') ||     /*   +  |     +       |    +   */
                 (cur == ';') ||     /*   +  |     +       |    +   */
                 (cur == '=') ||     /*   +  |     +       |    +   */
                 (cur == '>') ||     /*      |     +       |    +   */
                 (cur == '@') ||     /*   +  |     +       |    +   */
                 (cur == '`') ||     /*   +  |     +       |    +   */
                 (cur == '~') ||     /*   +  |     +       |    +   */
                 (cur == 'u'))       /*      |     +       |    +   */
        {
            if (ctxt.atom == null)
            {
                ctxt.atom = regNewAtom(ctxt, XML_REGEXP_CHARVAL);
                if (ctxt.atom != null)
                {
                    switch (cur)
                    {
                    case 'n':
                        ctxt.atom.codepoint = '\n';
                        break;
                    case 'r':
                        ctxt.atom.codepoint = '\r';
                        break;
                    case 't':
                        ctxt.atom.codepoint = '\t';
                        break;
                    case 'u':
                        cur = regParseEscapedCodepoint(ctxt);
                        if (cur < 0)
                        {
                            return;
                        }
                        ctxt.atom.codepoint = cur;
                        break;
                    default:
                        ctxt.atom.codepoint = cur;
                    }
                }
            }
            else if (ctxt.atom.type == XML_REGEXP_RANGES)
            {
                switch (cur)
                {
                case 'n':
                    cur = '\n';
                    break;
                case 'r':
                    cur = '\r';
                    break;
                case 't':
                    cur = '\t';
                    break;
                case 'u':
                    cur = regParseEscapedCodepoint(ctxt);
                    if (cur < 0)
                    {
                        return;
                    }
                    break;
                default:
                    break;
                }
                regAtomAddRange(ctxt, ctxt.atom, ctxt.neg,
                                XML_REGEXP_CHARVAL, cur, cur, null);
            }
            NEXT(ctxt);
        }
        else if ((cur == 's') || (cur == 'S') || (cur == 'i') || (cur == 'I') ||
                 (cur == 'c') || (cur == 'C') || (cur == 'd') || (cur == 'D') ||
                 (cur == 'w') || (cur == 'W'))
        {
            int type = XML_REGEXP_ANYSPACE;

            switch (cur)
            {
            case 's':
                type = XML_REGEXP_ANYSPACE;
                break;
            case 'S':
                type = XML_REGEXP_NOTSPACE;
                break;
            case 'i':
                type = XML_REGEXP_INITNAME;
                break;
            case 'I':
                type = XML_REGEXP_NOTINITNAME;
                break;
            case 'c':
                type = XML_REGEXP_NAMECHAR;
                break;
            case 'C':
                type = XML_REGEXP_NOTNAMECHAR;
                break;
            case 'd':
                type = XML_REGEXP_DECIMAL;
                break;
            case 'D':
                type = XML_REGEXP_NOTDECIMAL;
                break;
            case 'w':
                type = XML_REGEXP_REALCHAR;
                break;
            case 'W':
                type = XML_REGEXP_NOTREALCHAR;
                break;
            default:
                break;
            }
            NEXT(ctxt);
            if (ctxt.atom == null)
            {
                ctxt.atom = regNewAtom(ctxt, type);
            }
            else if (ctxt.atom.type == XML_REGEXP_RANGES)
            {
                regAtomAddRange(ctxt, ctxt.atom, ctxt.neg,
                                type, 0, 0, null);
            }
        }
        else
        {
            ERROR(ctxt, "Wrong escape sequence, misuse of character '\\'");
        }
    }

    /* The escapes accepted by xml_fa_parse_char_range after '\' (other than n r t u). */
    private static boolean isRangeEscape(int cur)
    {
        switch (cur)
        {
        case '\\': case '|': case '.': case '-': case '^': case '?':
        case '*': case '+': case '{': case '}': case '(': case ')':
        case '[': case ']': case '!': case '"': case '#': case '$':
        case '%': case ',': case '/': case ':': case ';': case '=':
        case '>': case '@': case '`': case '~':
            return true;
        default:
            return false;
        }
    }

    // xml_fa_parse_char_range
    /*
     * Port of xmlFAParseCharRange.
     *
     * [17]   charRange   ::=     seRange | XmlCharRef | XmlCharIncDash
     * [18]   seRange   ::=   charOrEsc '-' charOrEsc
     * [20]   charOrEsc   ::=   XmlChar | SingleCharEsc
     * [21]   XmlChar   ::=   [^\\#x2D\#x5B\#x5D]
     * [22]   XmlCharIncDash   ::=   [^\\#x5B\#x5D]
     */
    private static void faParseCharRange(XmlAutomata ctxt)
    {
        int cur, len;
        int start = -1;
        int end = -1;
        int[] lenp = new int[1];

        if (CUR(ctxt) == '\0')
        {
            ERROR(ctxt, "Expecting ']'");
            return;
        }

        cur = CUR(ctxt);
        if (cur == '\\')
        {
            NEXT(ctxt);
            cur = CUR(ctxt);
            switch (cur)
            {
            case 'n': start = 0xA; break;
            case 'r': start = 0xD; break;
            case 't': start = 0x9; break;
            case 'u':
                start = regParseEscapedCodepoint(ctxt);
                if (start < 0)
                {
                    return;
                }
                break;
            default:
                if (isRangeEscape(cur))
                {
                    start = cur;
                    break;
                }
                ERROR(ctxt, "Invalid escape value");
                return;
            }
            end = start;
            len = 1;
        }
        else if ((cur != 0x5B) && (cur != 0x5D))
        {
            lenp[0] = 4;
            end = start = regGetUtf8Char(ctxt.string, ctxt.cur, lenp);
            len = lenp[0];
            if (start < 0)
            {
                ERROR(ctxt, "Invalid UTF-8");
                return;
            }
        }
        else
        {
            ERROR(ctxt, "Expecting a char range");
            return;
        }
        /*
         * Since we are "inside" a range, we can assume ctxt->cur is past
         * the start of ctxt->string, and PREV should be safe
         */
        if ((start == '-') && (NXT(ctxt, 1) != ']') && (PREV(ctxt) != '[') && (PREV(ctxt) != '^'))
        {
            NEXTL(ctxt, len);
            return;
        }
        NEXTL(ctxt, len);
        cur = CUR(ctxt);
        if ((cur != '-') || (NXT(ctxt, 1) == '[') || (NXT(ctxt, 1) == ']'))
        {
            regAtomAddRange(ctxt, ctxt.atom, ctxt.neg,
                            XML_REGEXP_CHARVAL, start, end, null);
            return;
        }
        NEXT(ctxt);
        cur = CUR(ctxt);
        if (cur == '\\')
        {
            NEXT(ctxt);
            cur = CUR(ctxt);
            switch (cur)
            {
            case 'n': end = 0xA; break;
            case 'r': end = 0xD; break;
            case 't': end = 0x9; break;
            case 'u':
                end = regParseEscapedCodepoint(ctxt);
                if (end < 0)
                {
                    return;
                }
                break;
            default:
                if (isRangeEscape(cur))
                {
                    end = cur;
                    break;
                }
                ERROR(ctxt, "Invalid escape value");
                return;
            }
            len = 1;
        }
        else if ((cur != '\0') && (cur != 0x5B) && (cur != 0x5D))
        {
            lenp[0] = 4;
            end = regGetUtf8Char(ctxt.string, ctxt.cur, lenp);
            len = lenp[0];
            if (end < 0)
            {
                ERROR(ctxt, "Invalid UTF-8");
                return;
            }
        }
        else
        {
            ERROR(ctxt, "Expecting the end of a char range");
            return;
        }

        /* TODO check that the values are acceptable character ranges for XML */
        if (end < start)
        {
            ERROR(ctxt, "End of range is before start of range");
        }
        else
        {
            NEXTL(ctxt, len);
            regAtomAddRange(ctxt, ctxt.atom, ctxt.neg,
                            XML_REGEXP_CHARVAL, start, end, null);
        }
    }

    // xml_fa_parse_pos_char_group
    /*
     * Port of xmlFAParsePosCharGroup.
     * [14]   posCharGroup ::= ( charRange | charClassEsc  )+
     */
    private static void faParsePosCharGroup(XmlAutomata ctxt)
    {
        do
        {
            if (CUR(ctxt) == '\\')
            {
                int n1 = NXT(ctxt, 1);
                if ((n1 == 'n') || (n1 == 'r') || (n1 == 't') || isRangeEscape(n1))
                {
                    if (NXT(ctxt, 2) == '-')
                    {
                        faParseCharRange(ctxt);
                    }
                    else
                    {
                        faParseCharClassEsc(ctxt);
                    }
                }
                else if (n1 == 'u')
                {
                    if (NXT(ctxt, 6) == '-')
                    {
                        faParseCharRange(ctxt);
                    }
                    else
                    {
                        faParseCharClassEsc(ctxt);
                    }
                }
                else
                {
                    faParseCharClassEsc(ctxt);
                }
            }
            else
            {
                faParseCharRange(ctxt);
            }
        } while ((CUR(ctxt) != ']') && (CUR(ctxt) != '-') &&
                 (CUR(ctxt) != 0) && (ctxt.error == 0));
    }

    // xml_fa_parse_char_group
    /*
     * Port of xmlFAParseCharGroup.
     *
     * [13]   charGroup    ::= posCharGroup | negCharGroup | charClassSub
     * [15]   negCharGroup ::= '^' posCharGroup
     * [16]   charClassSub ::= ( posCharGroup | negCharGroup ) '-' charClassExpr
     * [12]   charClassExpr ::= '[' charGroup ']'
     */
    private static void faParseCharGroup(XmlAutomata ctxt)
    {
        int neg = ctxt.neg;

        if (CUR(ctxt) == '^')
        {
            NEXT(ctxt);
            ctxt.neg = b(ctxt.neg == 0);
            faParsePosCharGroup(ctxt);
            ctxt.neg = neg;
        }
        while ((CUR(ctxt) != ']') && (ctxt.error == 0))
        {
            if ((CUR(ctxt) == '-') && (NXT(ctxt, 1) == '['))
            {
                NEXT(ctxt);	/* eat the '-' */
                NEXT(ctxt);	/* eat the '[' */
                ctxt.neg = 2;
                faParseCharGroup(ctxt);
                ctxt.neg = neg;
                if (CUR(ctxt) == ']')
                {
                    NEXT(ctxt);
                }
                else
                {
                    ERROR(ctxt, "charClassExpr: ']' expected");
                }
                break;
            }
            else
            {
                faParsePosCharGroup(ctxt);
            }
        }
    }

    // xml_fa_parse_char_class
    /*
     * Port of xmlFAParseCharClass.
     *
     * [11]   charClass   ::=     charClassEsc | charClassExpr
     * [12]   charClassExpr   ::=   '[' charGroup ']'
     */
    private static void faParseCharClass(XmlAutomata ctxt)
    {
        if (CUR(ctxt) == '[')
        {
            NEXT(ctxt);
            ctxt.atom = regNewAtom(ctxt, XML_REGEXP_RANGES);
            if (ctxt.atom == null)
                return;
            faParseCharGroup(ctxt);
            if (CUR(ctxt) == ']')
            {
                NEXT(ctxt);
            }
            else
            {
                ERROR(ctxt, "xmlFAParseCharClass: ']' expected");
            }
        }
        else
        {
            faParseCharClassEsc(ctxt);
        }
    }

    // xml_fa_parse_quant_exact
    /*
     * Port of xmlFAParseQuantExact.
     * [8]   QuantExact   ::=   [0-9]+
     * returns 0 if success or -1 in case of error
     */
    private static int faParseQuantExact(XmlAutomata ctxt)
    {
        int ret = 0;
        int ok = 0;
        int overflow = 0;

        while ((CUR(ctxt) >= '0') && (CUR(ctxt) <= '9'))
        {
            if (ret > INT_MAX / 10)
            {
                overflow = 1;
            }
            else
            {
                int digit = CUR(ctxt) - '0';

                ret *= 10;
                if (ret > INT_MAX - digit)
                    overflow = 1;
                else
                    ret += digit;
            }
            ok = 1;
            NEXT(ctxt);
        }
        if ((ok != 1) || (overflow == 1))
        {
            return -1;
        }
        return ret;
    }

    // xml_fa_parse_quantifier
    /*
     * Port of xmlFAParseQuantifier.
     *
     * [4]   quantifier   ::=   [?*+] | ( '{' quantity '}' )
     * [5]   quantity   ::=   quantRange | quantMin | QuantExact
     * [6]   quantRange   ::=   QuantExact ',' QuantExact
     * [7]   quantMin   ::=   QuantExact ','
     * [8]   QuantExact   ::=   [0-9]+
     */
    private static int faParseQuantifier(XmlAutomata ctxt)
    {
        int cur;

        cur = CUR(ctxt);
        if ((cur == '?') || (cur == '*') || (cur == '+'))
        {
            if (ctxt.atom != null)
            {
                if (cur == '?')
                    ctxt.atom.quant = XML_REGEXP_QUANT_OPT;
                else if (cur == '*')
                    ctxt.atom.quant = XML_REGEXP_QUANT_MULT;
                else if (cur == '+')
                    ctxt.atom.quant = XML_REGEXP_QUANT_PLUS;
            }
            NEXT(ctxt);
            return 1;
        }
        if (cur == '{')
        {
            int min = 0, max = 0;

            NEXT(ctxt);
            cur = faParseQuantExact(ctxt);
            if (cur >= 0)
                min = cur;
            else
            {
                ERROR(ctxt, "Improper quantifier");
            }
            if (CUR(ctxt) == ',')
            {
                NEXT(ctxt);
                if (CUR(ctxt) == '}')
                    max = INT_MAX;
                else
                {
                    cur = faParseQuantExact(ctxt);
                    if (cur >= 0)
                        max = cur;
                    else
                    {
                        ERROR(ctxt, "Improper quantifier");
                    }
                }
            }
            if (CUR(ctxt) == '}')
            {
                NEXT(ctxt);
            }
            else
            {
                ERROR(ctxt, "Unterminated quantifier");
            }
            if (max == 0)
                max = min;
            if (ctxt.atom != null)
            {
                ctxt.atom.quant = XML_REGEXP_QUANT_RANGE;
                ctxt.atom.min = min;
                ctxt.atom.max = max;
            }
            return 1;
        }
        return 0;
    }

    // xml_fa_parse_atom
    /*
     * Port of xmlFAParseAtom.
     * [9]   atom   ::=   Char | charClass | ( '(' regExp ')' )
     */
    private static int faParseAtom(XmlAutomata ctxt)
    {
        int codepoint, len;
        int[] lenp = new int[1];

        codepoint = faIsChar(ctxt);
        if (codepoint > 0)
        {
            ctxt.atom = regNewAtom(ctxt, XML_REGEXP_CHARVAL);
            if (ctxt.atom == null)
                return -1;
            lenp[0] = 4;
            codepoint = regGetUtf8Char(ctxt.string, ctxt.cur, lenp);
            len = lenp[0];
            if (codepoint < 0)
            {
                ERROR(ctxt, "Invalid UTF-8");
                return -1;
            }
            ctxt.atom.codepoint = codepoint;
            NEXTL(ctxt, len);
            return 1;
        }
        else if (CUR(ctxt) == '|')
        {
            return 0;
        }
        else if (CUR(ctxt) == 0)
        {
            return 0;
        }
        else if (CUR(ctxt) == ')')
        {
            return 0;
        }
        else if (CUR(ctxt) == '(')
        {
            XmlAutomataState start, oldend, start0;

            NEXT(ctxt);
            if (ctxt.depth >= 50)
            {
                ERROR(ctxt, "xmlFAParseAtom: maximum nesting depth exceeded");
                return -1;
            }
            /*
             * this extra Epsilon transition is needed if we count with 0 allowed
             * unfortunately this can't be known at that point
             */
            faGenerateEpsilonTransition(ctxt, ctxt.state, null);
            start0 = ctxt.state;
            faGenerateEpsilonTransition(ctxt, ctxt.state, null);
            start = ctxt.state;
            oldend = ctxt.end;
            ctxt.end = null;
            ctxt.atom = null;
            ctxt.depth++;
            faParseRegExp(ctxt, 0);
            ctxt.depth--;
            if (CUR(ctxt) == ')')
            {
                NEXT(ctxt);
            }
            else
            {
                ERROR(ctxt, "xmlFAParseAtom: expecting ')'");
            }
            ctxt.atom = regNewAtom(ctxt, XML_REGEXP_SUBREG);
            if (ctxt.atom == null)
                return -1;
            ctxt.atom.start = start;
            ctxt.atom.start0 = start0;
            ctxt.atom.stop = ctxt.state;
            ctxt.end = oldend;
            return 1;
        }
        else if ((CUR(ctxt) == '[') || (CUR(ctxt) == '\\') || (CUR(ctxt) == '.'))
        {
            faParseCharClass(ctxt);
            return 1;
        }
        return 0;
    }

    // xml_fa_parse_piece
    /*
     * Port of xmlFAParsePiece.
     * [3]   piece   ::=   atom quantifier?
     */
    private static int faParsePiece(XmlAutomata ctxt)
    {
        int ret;

        ctxt.atom = null;
        ret = faParseAtom(ctxt);
        if (ret == 0)
            return 0;
        if (ctxt.atom == null)
        {
            ERROR(ctxt, "internal: no atom generated");
        }
        faParseQuantifier(ctxt);
        return 1;
    }

    // xml_fa_parse_branch
    /*
     * Port of xmlFAParseBranch.
     *
     * `to` is used to optimize by removing duplicate path in automata
     * in expressions like (a|b)(c|d)
     *
     * [2]   branch   ::=   piece*
     */
    private static int faParseBranch(XmlAutomata ctxt, XmlAutomataState to)
    {
        XmlAutomataState previous;
        int ret;

        previous = ctxt.state;
        ret = faParsePiece(ctxt);
        if (ret == 0)
        {
            /* Empty branch */
            faGenerateEpsilonTransition(ctxt, previous, to);
        }
        else
        {
            if (faGenerateTransitions(ctxt, previous,
                                      (CUR(ctxt) == '|' || CUR(ctxt) == ')' || CUR(ctxt) == 0) ? to : null,
                                      ctxt.atom) < 0)
            {
                regFreeAtom(ctxt.atom);
                ctxt.atom = null;
                return -1;
            }
            previous = ctxt.state;
            ctxt.atom = null;
        }
        while ((ret != 0) && (ctxt.error == 0))
        {
            ret = faParsePiece(ctxt);
            if (ret != 0)
            {
                if (faGenerateTransitions(ctxt, previous,
                                          (CUR(ctxt) == '|' || CUR(ctxt) == ')' || CUR(ctxt) == 0) ? to : null,
                                          ctxt.atom) < 0)
                {
                    regFreeAtom(ctxt.atom);
                    ctxt.atom = null;
                    return -1;
                }
                previous = ctxt.state;
                ctxt.atom = null;
            }
        }
        return 0;
    }

    // xml_fa_parse_reg_exp
    /*
     * Port of xmlFAParseRegExp.
     * [1]   regExp   ::=     branch  ( '|' branch )*
     * top: is this the top-level expression ?
     */
    private static void faParseRegExp(XmlAutomata ctxt, int top)
    {
        XmlAutomataState start, end;

        /* if not top start should have been generated by an epsilon trans */
        start = ctxt.state;
        ctxt.end = null;
        faParseBranch(ctxt, null);
        if (top != 0)
        {
            ctxt.state.type = XML_REGEXP_FINAL_STATE;
        }
        if (CUR(ctxt) != '|')
        {
            ctxt.end = ctxt.state;
            return;
        }
        end = ctxt.state;
        while ((CUR(ctxt) == '|') && (ctxt.error == 0))
        {
            NEXT(ctxt);
            ctxt.state = start;
            ctxt.end = null;
            faParseBranch(ctxt, end);
        }
        if (top == 0)
        {
            ctxt.state = end;
            ctxt.end = end;
        }
    }

    /* ***********************************************************************
     *									*
     *			The basic API					*
     *									*
     ************************************************************************/

    /**
     * Port of xmlRegexpPrint. No-op since 2.14.0.
     * @param output the file for the output debug
     * @param regexp the compiled regexp
     */
    // xml_regexp_print
    public static void regexpPrint(PrintStream output, XmlRegexp regexp)
    {
    }

    /**
     * Port of xmlRegexpCompile.
     *
     * Parses a regular expression conforming to XML Schemas Part 2 Datatype
     * Appendix F and builds an automata suitable for testing strings against
     * that regular expression.
     * @param regexp a regular expression string
     * @param error_func called for each compile error, or null
     * @param error_data passed to error_func
     * @return the compiled expression or null in case of error
     */
    // xml_regexp_compile_with_errors
    public static XmlRegexp regexpCompileWithErrors(String regexp, ErrorFunc error_func,
                                                    Object error_data)
    {
        XmlRegexp ret = null;
        XmlAutomata ctxt;

        if (regexp == null)
            return null;

        ctxt = regNewParserCtxt(regexp);
        ctxt.error_func = error_func;
        ctxt.error_data = error_data;

        /* initialize the parser */
        ctxt.state = regStatePush(ctxt);
        if (ctxt.state == null)
        {
            regFreeParserCtxt(ctxt);
            return null;
        }
        ctxt.start = ctxt.state;
        ctxt.end = null;

        /* parse the expression building an automata */
        faParseRegExp(ctxt, 1);
        if (CUR(ctxt) != 0)
        {
            ERROR(ctxt, "xmlFAParseRegExp: extra characters");
        }
        if (ctxt.error != 0)
        {
            regFreeParserCtxt(ctxt);
            return null;
        }
        ctxt.end = ctxt.state;
        ctxt.start.type = XML_REGEXP_START_STATE;
        ctxt.end.type = XML_REGEXP_FINAL_STATE;

        /* remove the Epsilon except for counted transitions */
        faEliminateEpsilonTransitions(ctxt);


        if (ctxt.error != 0)
        {
            regFreeParserCtxt(ctxt);
            return null;
        }
        ret = regEpxFromParse(ctxt);

        regFreeParserCtxt(ctxt);
        return ret;
    }

    /**
     * Port of xmlRegexpCompile, the compile errors are not reported.
     * @param regexp a regular expression string
     * @return the compiled expression or null in case of error
     */
    // xml_regexp_compile
    public static XmlRegexp regexpCompile(String regexp)
    {
        return regexpCompileWithErrors(regexp, null, null);
    }

    /**
     * Port of xmlRegexpExec.
     * Check if the regular expression matches a string.
     * @param comp the compiled regular expression
     * @param content the value to check against the regular expression
     * @return 1 if it matches, 0 if not and a negative value in case of error
     */
    // xml_regexp_exec
    public static int regexpExec(XmlRegexp comp, String content)
    {
        if ((comp == null) || (content == null))
            return -1;
        if (comp.states == null)
        {
            /* C dereferences comp->states[0]: a regexp compiled to the compact
               form (only automata of strings) can not be executed on characters. */
            return XML_REGEXP_INTERNAL_ERROR;
        }
        return faRegExec(comp, toUtf8(content));
    }

    /**
     * Port of xmlRegexpIsDeterminist.
     *
     * Check if the regular expression is deterministic.
     *
     * DTD and XML Schemas require a deterministic content model,
     * so the automaton compiled from the regex must be a DFA.
     *
     * The runtime of this function is quadratic in the number of
     * outgoing edges, causing serious worst-case performance issues.
     * @param comp the compiled regular expression
     * @return 1 if it yes, 0 if not and a negative value in case of error
     */
    // xml_regexp_is_determinist
    public static int regexpIsDeterminist(XmlRegexp comp)
    {
        XmlAutomata am;
        int ret;

        if (comp == null)
            return -1;
        if (comp.determinist != -1)
            return comp.determinist;

        am = automataNew();
        if (am == null)
            return -1;
        am.nb_atoms = comp.nb_atoms;
        am.atoms = comp.atoms;
        am.nb_states = comp.nb_states;
        am.states = comp.states;
        am.determinist = -1;
        am.flags = comp.flags;
        ret = faComputesDeterminism(am);
        am.atoms = null;
        am.states = null;
        automataFree(am);
        comp.determinist = ret;
        return ret;
    }

    /**
     * Port of xmlRegFreeRegexp.
     * @param regexp the regexp
     */
    // xml_regexp_free
    public static void regexpFree(XmlRegexp regexp)
    {
    }

    /* ***********************************************************************
     *									*
     *			The Automata interface				*
     *									*
     ************************************************************************/

    /**
     * Port of xmlNewAutomata. Create a new automata
     * @return the new object or null in case of failure
     */
    // xml_automata_new
    public static XmlAutomata automataNew()
    {
        XmlAutomata ctxt;

        ctxt = regNewParserCtxt(null);

        /* initialize the parser */
        ctxt.state = regStatePush(ctxt);
        if (ctxt.state == null)
        {
            automataFree(ctxt);
            return null;
        }
        ctxt.start = ctxt.state;
        ctxt.end = null;

        ctxt.start.type = XML_REGEXP_START_STATE;
        ctxt.flags = 0;

        return ctxt;
    }

    /**
     * Port of xmlFreeAutomata.
     * @param am an automata
     */
    // xml_automata_free
    public static void automataFree(XmlAutomata am)
    {
        if (am == null)
            return;
        regFreeParserCtxt(am);
    }

    /**
     * Port of xmlAutomataSetFlags. Set some flags on the automata
     * @param am an automata
     * @param flags a set of internal flags
     */
    // xml_automata_set_flags
    public static void automataSetFlags(XmlAutomata am, int flags)
    {
        if (am == null)
            return;
        am.flags |= flags;
    }

    /**
     * Set the function receiving the errors raised while building the
     * automata (libxml2 raises them on the global error handler).
     * @param am an automata
     * @param error_func called for each error, or null
     * @param error_data passed to error_func
     */
    // xml_automata_set_error_func
    public static void automataSetErrorFunc(XmlAutomata am, ErrorFunc error_func, Object error_data)
    {
        if (am == null)
            return;
        am.error_func = error_func;
        am.error_data = error_data;
    }

    /**
     * Port of xmlAutomataGetInitState. Initial state lookup
     * @param am an automata
     * @return the initial state of the automata
     */
    // xml_automata_get_init_state
    public static XmlAutomataState automataGetInitState(XmlAutomata am)
    {
        if (am == null)
            return null;
        return am.start;
    }

    /**
     * Port of xmlAutomataSetFinalState. Makes that state a final state
     * @param am an automata
     * @param state a state in this automata
     * @return 0 or -1 in case of error
     */
    // xml_automata_set_final_state
    public static int automataSetFinalState(XmlAutomata am, XmlAutomataState state)
    {
        if ((am == null) || (state == null))
            return -1;
        state.type = XML_REGEXP_FINAL_STATE;
        return 0;
    }

    /* token|token2 as built by the C transition functions (a new String object). */
    private static String joinTokens(String token, String token2)
    {
        return token + "|" + token2;
    }

    /**
     * Port of xmlAutomataNewTransition.
     *
     * If `to` is null, this creates first a new target state in the automata
     * and then adds a transition from the `from` state to the target state
     * activated by the value of `token`
     * @param am an automata
     * @param from the starting point of the transition
     * @param to the target point of the transition or null
     * @param token the input string associated to that transition
     * @param data data passed to the callback function if the transition is activated
     * @return the target state or null in case of error
     */
    // xml_automata_new_transition
    public static XmlAutomataState automataNewTransition(XmlAutomata am, XmlAutomataState from,
                                                         XmlAutomataState to, String token,
                                                         Object data)
    {
        RegAtom atom;

        if ((am == null) || (from == null) || (token == null))
            return null;
        atom = regNewAtom(am, XML_REGEXP_STRING);
        atom.data = data;
        atom.valuep = regStrdup(token);

        if (faGenerateTransitions(am, from, to, atom) < 0)
        {
            regFreeAtom(atom);
            return null;
        }
        if (to == null)
            return am.state;
        return to;
    }

    /**
     * Port of xmlAutomataNewTransition2.
     *
     * If `to` is null, this creates first a new target state in the automata
     * and then adds a transition from the `from` state to the target state
     * activated by the value of `token`
     * @param am an automata
     * @param from the starting point of the transition
     * @param to the target point of the transition or null
     * @param token the first input string associated to that transition
     * @param token2 the second input string associated to that transition
     * @param data data passed to the callback function if the transition is activated
     * @return the target state or null in case of error
     */
    // xml_automata_new_transition2
    public static XmlAutomataState automataNewTransition2(XmlAutomata am, XmlAutomataState from,
                                                          XmlAutomataState to, String token,
                                                          String token2, Object data)
    {
        RegAtom atom;

        if ((am == null) || (from == null) || (token == null))
            return null;
        atom = regNewAtom(am, XML_REGEXP_STRING);
        atom.data = data;
        if ((token2 == null) || (token2.isEmpty()))
        {
            atom.valuep = regStrdup(token);
        }
        else
        {
            atom.valuep = joinTokens(token, token2);
        }

        if (faGenerateTransitions(am, from, to, atom) < 0)
        {
            regFreeAtom(atom);
            return null;
        }
        if (to == null)
            return am.state;
        return to;
    }

    /**
     * Port of xmlAutomataNewNegTrans.
     *
     * If `to` is null, this creates first a new target state in the automata
     * and then adds a transition from the `from` state to the target state
     * activated by any value except (`token`,`token2`)
     * Note that if `token2` is not null, then (X, null) won't match to follow
     * the semantic of XSD ##other
     * @param am an automata
     * @param from the starting point of the transition
     * @param to the target point of the transition or null
     * @param token the first input string associated to that transition
     * @param token2 the second input string associated to that transition
     * @param data data passed to the callback function if the transition is activated
     * @return the target state or null in case of error
     */
    // xml_automata_new_neg_trans
    public static XmlAutomataState automataNewNegTrans(XmlAutomata am, XmlAutomataState from,
                                                       XmlAutomataState to, String token,
                                                       String token2, Object data)
    {
        RegAtom atom;

        if ((am == null) || (from == null) || (token == null))
            return null;
        atom = regNewAtom(am, XML_REGEXP_STRING);
        atom.data = data;
        atom.neg = 1;
        if ((token2 == null) || (token2.isEmpty()))
        {
            atom.valuep = regStrdup(token);
        }
        else
        {
            atom.valuep = joinTokens(token, token2);
        }
        /* snprintf(err_msg, 199, "not %s"): at most 198 bytes */
        byte[] msg = toUtf8("not " + (String) atom.valuep);
        if (msg.length > 198)
            msg = Arrays.copyOf(msg, 198);
        atom.valuep2 = new String(msg, StandardCharsets.UTF_8);

        if (faGenerateTransitions(am, from, to, atom) < 0)
        {
            regFreeAtom(atom);
            return null;
        }
        am.negs++;
        if (to == null)
            return am.state;
        return to;
    }

    /**
     * Port of xmlAutomataNewCountTrans2.
     *
     * If `to` is null, this creates first a new target state in the automata
     * and then adds a transition from the `from` state to the target state
     * activated by a succession of input of value `token` and `token2` and
     * whose number is between `min` and `max`
     * @param am an automata
     * @param from the starting point of the transition
     * @param to the target point of the transition or null
     * @param token the input string associated to that transition
     * @param token2 the second input string associated to that transition
     * @param min the minimum successive occurrences of token
     * @param max the maximum successive occurrences of token
     * @param data data associated to the transition
     * @return the target state or null in case of error
     */
    // xml_automata_new_count_trans2
    public static XmlAutomataState automataNewCountTrans2(XmlAutomata am, XmlAutomataState from,
                                                          XmlAutomataState to, String token,
                                                          String token2,
                                                          int min, int max, Object data)
    {
        RegAtom atom;
        int counter;

        if ((am == null) || (from == null) || (token == null))
            return null;
        if (min < 0)
            return null;
        if ((max < min) || (max < 1))
            return null;
        atom = regNewAtom(am, XML_REGEXP_STRING);
        if ((token2 == null) || (token2.isEmpty()))
        {
            atom.valuep = regStrdup(token);
        }
        else
        {
            atom.valuep = joinTokens(token, token2);
        }
        atom.data = data;
        if (min == 0)
            atom.min = 1;
        else
            atom.min = min;
        atom.max = max;

        /*
         * associate a counter to the transition.
         */
        counter = regGetCounter(am);
        if (counter < 0)
            return null;
        am.counters[counter].min = min;
        am.counters[counter].max = max;

        /* xml_fa_generate_transitions(am, from, to, atom); */
        if (to == null)
        {
            to = regStatePush(am);
            if (to == null)
                return null;
        }
        regStateAddTrans(am, from, atom, to, counter, -1);
        if (regAtomPush(am, atom) < 0)
            return null;
        am.state = to;

        if (to == null)
            to = am.state;
        if (to == null)
            return null;
        if (min == 0)
            faGenerateEpsilonTransition(am, from, to);
        return to;
    }

    /**
     * Port of xmlAutomataNewCountTrans.
     *
     * If `to` is null, this creates first a new target state in the automata
     * and then adds a transition from the `from` state to the target state
     * activated by a succession of input of value `token` and whose number
     * is between `min` and `max`
     * @param am an automata
     * @param from the starting point of the transition
     * @param to the target point of the transition or null
     * @param token the input string associated to that transition
     * @param min the minimum successive occurrences of token
     * @param max the maximum successive occurrences of token
     * @param data data associated to the transition
     * @return the target state or null in case of error
     */
    // xml_automata_new_count_trans
    public static XmlAutomataState automataNewCountTrans(XmlAutomata am, XmlAutomataState from,
                                                         XmlAutomataState to, String token,
                                                         int min, int max, Object data)
    {
        RegAtom atom;
        int counter;

        if ((am == null) || (from == null) || (token == null))
            return null;
        if (min < 0)
            return null;
        if ((max < min) || (max < 1))
            return null;
        atom = regNewAtom(am, XML_REGEXP_STRING);
        atom.valuep = regStrdup(token);
        atom.data = data;
        if (min == 0)
            atom.min = 1;
        else
            atom.min = min;
        atom.max = max;

        /*
         * associate a counter to the transition.
         */
        counter = regGetCounter(am);
        if (counter < 0)
            return null;
        am.counters[counter].min = min;
        am.counters[counter].max = max;

        /* xml_fa_generate_transitions(am, from, to, atom); */
        if (to == null)
        {
            to = regStatePush(am);
            if (to == null)
                return null;
        }
        regStateAddTrans(am, from, atom, to, counter, -1);
        if (regAtomPush(am, atom) < 0)
            return null;
        am.state = to;

        if (to == null)
            to = am.state;
        if (to == null)
            return null;
        if (min == 0)
            faGenerateEpsilonTransition(am, from, to);
        return to;
    }

    /**
     * Port of xmlAutomataNewOnceTrans2.
     *
     * If `to` is null, this creates first a new target state in the automata
     * and then adds a transition from the `from` state to the target state
     * activated by a succession of input of value `token` and `token2` and whose
     * number is between `min` and `max`, moreover that transition can only be
     * crossed once.
     * @param am an automata
     * @param from the starting point of the transition
     * @param to the target point of the transition or null
     * @param token the input string associated to that transition
     * @param token2 the second input string associated to that transition
     * @param min the minimum successive occurrences of token
     * @param max the maximum successive occurrences of token
     * @param data data associated to the transition
     * @return the target state or null in case of error
     */
    // xml_automata_new_once_trans2
    public static XmlAutomataState automataNewOnceTrans2(XmlAutomata am, XmlAutomataState from,
                                                         XmlAutomataState to, String token,
                                                         String token2,
                                                         int min, int max, Object data)
    {
        RegAtom atom;
        int counter;

        if ((am == null) || (from == null) || (token == null))
            return null;
        if (min < 1)
            return null;
        if (max < min)
            return null;
        atom = regNewAtom(am, XML_REGEXP_STRING);
        if ((token2 == null) || (token2.isEmpty()))
        {
            atom.valuep = regStrdup(token);
        }
        else
        {
            atom.valuep = joinTokens(token, token2);
        }
        atom.data = data;
        atom.quant = XML_REGEXP_QUANT_ONCEONLY;
        atom.min = min;
        atom.max = max;
        /*
         * associate a counter to the transition.
         */
        counter = regGetCounter(am);
        if (counter < 0)
            return null;
        am.counters[counter].min = 1;
        am.counters[counter].max = 1;

        /* xml_fa_generate_transitions(am, from, to, atom); */
        if (to == null)
        {
            to = regStatePush(am);
            if (to == null)
                return null;
        }
        regStateAddTrans(am, from, atom, to, counter, -1);
        if (regAtomPush(am, atom) < 0)
            return null;
        am.state = to;
        return to;
    }

    /**
     * Port of xmlAutomataNewOnceTrans.
     *
     * If `to` is null, this creates first a new target state in the automata
     * and then adds a transition from the `from` state to the target state
     * activated by a succession of input of value `token` and whose number
     * is between `min` and `max`, moreover that transition can only be crossed
     * once.
     * @param am an automata
     * @param from the starting point of the transition
     * @param to the target point of the transition or null
     * @param token the input string associated to that transition
     * @param min the minimum successive occurrences of token
     * @param max the maximum successive occurrences of token
     * @param data data associated to the transition
     * @return the target state or null in case of error
     */
    // xml_automata_new_once_trans
    public static XmlAutomataState automataNewOnceTrans(XmlAutomata am, XmlAutomataState from,
                                                        XmlAutomataState to, String token,
                                                        int min, int max, Object data)
    {
        RegAtom atom;
        int counter;

        if ((am == null) || (from == null) || (token == null))
            return null;
        if (min < 1)
            return null;
        if (max < min)
            return null;
        atom = regNewAtom(am, XML_REGEXP_STRING);
        atom.valuep = regStrdup(token);
        atom.data = data;
        atom.quant = XML_REGEXP_QUANT_ONCEONLY;
        atom.min = min;
        atom.max = max;
        /*
         * associate a counter to the transition.
         */
        counter = regGetCounter(am);
        if (counter < 0)
            return null;
        am.counters[counter].min = 1;
        am.counters[counter].max = 1;

        /* xml_fa_generate_transitions(am, from, to, atom); */
        if (to == null)
        {
            to = regStatePush(am);
            if (to == null)
                return null;
        }
        regStateAddTrans(am, from, atom, to, counter, -1);
        if (regAtomPush(am, atom) < 0)
            return null;
        am.state = to;
        return to;
    }

    /**
     * Port of xmlAutomataNewState. Create a new disconnected state in the automata
     * @param am an automata
     * @return the new state or null in case of error
     */
    // xml_automata_new_state
    public static XmlAutomataState automataNewState(XmlAutomata am)
    {
        if (am == null)
            return null;
        return regStatePush(am);
    }

    /**
     * Port of xmlAutomataNewEpsilon.
     *
     * If `to` is null, this creates first a new target state in the automata
     * and then adds an epsilon transition from the `from` state to the
     * target state
     * @param am an automata
     * @param from the starting point of the transition
     * @param to the target point of the transition or null
     * @return the target state or null in case of error
     */
    // xml_automata_new_epsilon
    public static XmlAutomataState automataNewEpsilon(XmlAutomata am, XmlAutomataState from,
                                                      XmlAutomataState to)
    {
        if ((am == null) || (from == null))
            return null;
        faGenerateEpsilonTransition(am, from, to);
        if (to == null)
            return am.state;
        return to;
    }

    /**
     * Port of xmlAutomataNewAllTrans.
     *
     * If `to` is null, this creates first a new target state in the automata
     * and then adds a an ALL transition from the `from` state to the
     * target state. That transition is an epsilon transition allowed only when
     * all transitions from the `from` node have been activated.
     * @param am an automata
     * @param from the starting point of the transition
     * @param to the target point of the transition or null
     * @param lax allow to transition if not all all transitions have been activated
     * @return the target state or null in case of error
     */
    // xml_automata_new_all_trans
    public static XmlAutomataState automataNewAllTrans(XmlAutomata am, XmlAutomataState from,
                                                       XmlAutomataState to, int lax)
    {
        if ((am == null) || (from == null))
            return null;
        faGenerateAllTransition(am, from, to, lax);
        if (to == null)
            return am.state;
        return to;
    }

    /**
     * Port of xmlAutomataNewCounter. Create a new counter
     * @param am an automata
     * @param min the minimal value on the counter
     * @param max the maximal value on the counter
     * @return the counter number or -1 in case of error
     */
    // xml_automata_new_counter
    public static int automataNewCounter(XmlAutomata am, int min, int max)
    {
        int ret;

        if (am == null)
            return -1;

        ret = regGetCounter(am);
        if (ret < 0)
            return -1;
        am.counters[ret].min = min;
        am.counters[ret].max = max;
        return ret;
    }

    /**
     * Port of xmlAutomataNewCountedTrans.
     *
     * If `to` is null, this creates first a new target state in the automata
     * and then adds an epsilon transition from the `from` state to the target state
     * which will increment the counter provided
     * @param am an automata
     * @param from the starting point of the transition
     * @param to the target point of the transition or null
     * @param counter the counter associated to that transition
     * @return the target state or null in case of error
     */
    // xml_automata_new_counted_trans
    public static XmlAutomataState automataNewCountedTrans(XmlAutomata am, XmlAutomataState from,
                                                           XmlAutomataState to, int counter)
    {
        if ((am == null) || (from == null) || (counter < 0))
            return null;
        faGenerateCountedEpsilonTransition(am, from, to, counter);
        if (to == null)
            return am.state;
        return to;
    }

    /**
     * Port of xmlAutomataNewCounterTrans.
     *
     * If `to` is null, this creates first a new target state in the automata
     * and then adds an epsilon transition from the `from` state to the target state
     * which will be allowed only if the counter is within the right range.
     * @param am an automata
     * @param from the starting point of the transition
     * @param to the target point of the transition or null
     * @param counter the counter associated to that transition
     * @return the target state or null in case of error
     */
    // xml_automata_new_counter_trans
    public static XmlAutomataState automataNewCounterTrans(XmlAutomata am, XmlAutomataState from,
                                                           XmlAutomataState to, int counter)
    {
        if ((am == null) || (from == null) || (counter < 0))
            return null;
        faGenerateCountedTransition(am, from, to, counter);
        if (to == null)
            return am.state;
        return to;
    }

    /**
     * Port of xmlAutomataCompile.
     *
     * Compile the automata into a Reg Exp ready for being executed.
     * The automata should be free after this point.
     * @param am an automata
     * @return the compiled regexp or null in case of error
     */
    // xml_automata_compile
    public static XmlRegexp automataCompile(XmlAutomata am)
    {
        XmlRegexp ret;

        if ((am == null) || (am.error != 0)) return null;
        faEliminateEpsilonTransitions(am);
        if (am.error != 0)
            return null;
        /* xml_fa_computes_determinism(am); */
        ret = regEpxFromParse(am);

        return ret;
    }

    /**
     * Port of xmlAutomataIsDeterminist. Checks if an automata is determinist.
     * @param am an automata
     * @return 1 if true, 0 if not, and -1 in case of error
     */
    // xml_automata_is_determinist
    public static int automataIsDeterminist(XmlAutomata am)
    {
        int ret;

        if (am == null)
            return -1;

        ret = faComputesDeterminism(am);
        return ret;
    }
}
