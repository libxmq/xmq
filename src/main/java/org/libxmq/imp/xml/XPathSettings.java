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

/* Portions of this file are ported from libxml2 (xpath.c).
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

import java.util.HashMap;
import java.util.Map;

/**
 * Settings of a compiled xpath expression, like XmlXPathSettings in the C version.
 */
public final class XPathSettings
{
    /** Maximum nesting of an expression, libxml2 XPATH_MAX_RECURSION_DEPTH. */
    public static final int DEFAULT_MAX_DEPTH = 1000;

    /** Prefixes that may be used in expressions (the xml prefix is always known). */
    public Map<String, String> namespaces = new HashMap<>();
    /** id() also finds id attributes (and name on a), like libxml2 for html documents. */
    public boolean html_ids;
    /** Maximum nesting of the expression. */
    public int max_depth = DEFAULT_MAX_DEPTH;
    /** There is no context node (like a fresh libxml2 xmlXPathNewContext): relative paths select nothing. */
    public boolean no_context_node;
    /** Variable references are compile errors (libxml2 XML_XPATH_NOVAR, xslt patterns). */
    public boolean no_variables;

    /** Default settings. */
    public XPathSettings()
    {
    }

    /**
     * A copy of settings.
     * @param s The settings to copy.
     */
    public XPathSettings(XPathSettings s)
    {
        namespaces = new HashMap<>(s.namespaces);
        html_ids = s.html_ids;
        max_depth = s.max_depth;
        no_context_node = s.no_context_node;
        no_variables = s.no_variables;
    }
}
