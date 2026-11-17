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

/* Portions of this file are ported from libxslt (version 1.1.45 as found in the
   libxmq fork in 3rdparty/libxslt). libxslt is covered by the following notice
   (from libxslt/Copyright):

 Copyright (C) 2001-2002 Daniel Veillard.  All Rights Reserved.

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
DANIEL VEILLARD BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CON-
NECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of Daniel Veillard shall not
be used in advertising or otherwise to promote the sale, use or other deal-
ings in this Software without prior written authorization from him.
*/

package org.libxmq.imp.xml.libxslt1;

import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import org.libxmq.imp.xml.XPath;
import org.libxmq.imp.xml.XmlNode;

/** A compiled pattern (libxslt xsltCompMatch, XmlLibxslt1Pattern in C), a list of alternatives. */
final class Pattern
{
    // The operations of the steps (libxslt xsltOp).
    static final int OP_END = 0;
    static final int OP_ROOT = 1;
    static final int OP_ELEM = 2;
    static final int OP_ATTR = 3;
    static final int OP_PARENT = 4;
    static final int OP_ANCESTOR = 5;
    static final int OP_ID = 6;
    static final int OP_KEY = 7;
    static final int OP_NS = 8;
    static final int OP_ALL = 9;
    static final int OP_PI = 10;
    static final int OP_COMMENT = 11;
    static final int OP_TEXT = 12;
    static final int OP_NODE = 13;
    static final int OP_PREDICATE = 14;

    // The axis of a step (libxslt xsltAxis).
    static final int AXIS_NONE = 0;
    static final int AXIS_CHILD = 1;
    static final int AXIS_ATTRIBUTE = 2;

    /** A step of a pattern (libxslt xsltStepOp, XsltStepOp in C). */
    static final class StepOp
    {
        int op;
        String value;
        String value2;
        String value3;
        XPath comp;
        // Optimisations for count.
        int previous_extra;
        int index_extra;
        int len_extra;
    }

    /** A state of the pattern interpreter, for backtracking (XsltStepState in C). */
    static final class StepState
    {
        int step;
        XmlNode node;

        StepState(int step, XmlNode node)
        {
            this.step = step;
            this.node = node;
        }
    }

    Pattern next;           // Siblings in the name hash.
    float priority;
    String pattern;
    String mode;
    String mode_uri;
    Template templ;
    XmlNode node;           // The containing element.
    int direct;
    int nb_step;
    Map<String, String> ns_list; // The namespaces in scope.
    List<StepOp> steps = new ArrayList<>();

    /** The step i, a zeroed (OP_END) step beyond the steps like the calloced C array. */
    StepOp step(int i)
    {
        if (i < steps.size()) return steps.get(i);
        return new StepOp();
    }
}
