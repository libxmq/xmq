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

/* Portions of this file are ported from libxml2 (xmlschemastypes.h and
   schemasInternals.h, version 2.16.0 as found in the libxmq fork in 3rdparty/libxml2).
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

package org.libxmq.imp.xml.xsd10;

/** Regular expressions for pattern facets (C {@code XmlXsd10RegexpHooks}: libxml2 xmlRegexpCompile,
    xmlRegexpExec, xmlRegFreeRegexp). */
public final class Xsd10RegexpHooks
{
    /** Construct with default field values. */
    public Xsd10RegexpHooks()
    {
    }

    /** C {@code void *(*compile)(const char *regexp)}. */
    public interface Compile
    {
        /**
         * Compile a regular expression.
         *
         * @param regexp the expression
         * @return the compiled regexp or null
         */
        Object compile(String regexp);
    }
    /** C {@code int (*exec)(void *regexp, const char *value)}. */
    public interface Exec
    {
        /**
         * Match a value against a compiled regular expression.
         *
         * @param regexp compiled
         * @param value value
         * @return 1 match, 0 no, negative error
         */
        int exec(Object regexp, String value);
    }
    /** C {@code void (*free)(void *regexp)}. */
    public interface Free
    {
        /**
         * Free a compiled regular expression.
         *
         * @param regexp compiled
         */
        void free(Object regexp);
    }

    /** Compile an XML Schema regular expression, null if invalid. */
    public Compile compile;
    /** 1 if value matches, 0 if not, negative on errors. */
    public Exec exec;
    /** Free a compiled regexp. */
    public Free free;
}
