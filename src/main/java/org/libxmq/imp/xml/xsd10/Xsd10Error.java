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

/* Portions of this file are ported from libxml2 (xmlschemas.c, xmlschemas.h,
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

import org.libxmq.imp.xml.XmlNode;

/** An error (libxml2 xmlError, C {@code XmlXsd10Error}). Passed to the structured error callback,
    only valid during the call. */
public final class Xsd10Error
{
    /** Construct with default field values. */
    public Xsd10Error()
    {
    }

    /** What part of the library raised this error */
    int domain;
    /** The error code, e.g. an xmlParserErrors */
    int code;
    /** human-readable informative error message */
    String message;
    /** how consequent is the error (XML_XSD10_ERR_*) */
    int level;
    /** the filename */
    String file;
    /** the line number if available */
    int line;
    /** extra string information */
    String str1;
    /** extra string information */
    String str2;
    /** extra string information */
    String str3;
    /** extra number information */
    int int1;
    /** error column # or 0 if N/A */
    int int2;
    /** libxml2: the parser context. Native: null, or the context of the parser input as text (a String,
        eg "\n^\n") that formatError prints after the message like libxml2. */
    Object ctxt;
    /** the node in the tree */
    XmlNode node;

    /**
     * The domain.
     *
     * @return the domain
     */
    public int domain() { return domain; }
    /**
     * The code.
     *
     * @return the code
     */
    public int code() { return code; }
    /**
     * The message.
     *
     * @return the message
     */
    public String message() { return message; }
    /**
     * The level.
     *
     * @return the level
     */
    public int level() { return level; }
    /**
     * The file.
     *
     * @return the file
     */
    public String file() { return file; }
    /**
     * The line.
     *
     * @return the line
     */
    public int line() { return line; }
    /**
     * The node.
     *
     * @return the node
     */
    public XmlNode node() { return node; }
}
