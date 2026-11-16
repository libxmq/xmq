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

/**
 * A progressive execution context (libxml2 xmlRegExecCtxt), port of struct
 * XmlRegExecCtxt in xml_regexp.c.
 */
public final class XmlRegExecCtxt
{
    int status;		/* execution status != 0 indicate an error */
    int determinist;	/* did we find an indeterministic behaviour */
    XmlRegexp comp;	/* the compiled regexp */
    XmlRegexp.ExecCallback callback;
    /** The data given to XmlRegexp.regExecNew (the C callback receives it as first argument). */
    public Object data;

    XmlAutomataState state; /* the current state */
    int transno;	/* the current transition on that state */
    int transcount;	/* the number of chars in char counted transitions */

    /*
     * A stack of rollback states
     */
    int maxRollbacks;
    int nbRollbacks;
    XmlRegexp.RegExecRollback[] rollbacks;

    /*
     * The state of the automata if any
     */
    int[] counts;

    /*
     * The input stack
     */
    int inputStackMax;
    int inputStackNr;
    int index;
    int[] charStack;
    byte[] inputString; /* when operating on characters (UTF-8, read as 0 past the end) */
    XmlRegexp.RegInputToken[] inputStack; /* when operating on strings */

    /*
     * error handling
     */
    int errStateNo;		/* the error state number */
    XmlAutomataState errState;    /* the error state */
    String errString;		/* the string raising the error */
    int[] errCounts;		/* counters at the error state (C: second half of counts) */
    int nbPush;

    XmlRegExecCtxt()
    {
    }
}
