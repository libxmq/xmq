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
import org.libxmq.imp.xml.XmlRegExecCtxt;

/** Holds information of an element node (libxml2 xmlSchemaNodeInfo). (C {@code XmlXsd10NodeInfo}). */
public class Xsd10NodeInfo
{
    /** C {@code int nodeType}. */
    int nodeType;
    /** C {@code XMQNode * node}. */
    XmlNode node;
    /** C {@code int nodeLine}. */
    int nodeLine;
    /** C {@code const char * localName}. */
    String localName;
    /** C {@code const char * nsName}. */
    String nsName;
    /** C {@code const char * value}. */
    String value;
    /** the pre-computed value if any */
    Xsd10Val val;
    /** the complex/simple type definition if any */
    Xsd10Type typeDef;
    /** combination of node info flags */
    int flags;
    /** C {@code int valNeeded}. */
    int valNeeded;
    /** C {@code int normVal}. */
    int normVal;
    /** the element/attribute declaration */
    Xsd10Element decl;
    /** C {@code int depth}. */
    int depth;
    /** C {@code XmlXsd10PSVIIDCBinding * idcTable}. */
    Xsd10PSVIIDCBinding idcTable;
    /** C {@code XmlXsd10IDCMatcher * idcMatchers}. */
    Xsd10IDCMatcher idcMatchers;
    /** C {@code XmlRegExecCtxt * regexCtxt}. */
    XmlRegExecCtxt regexCtxt;
    /** Namespace bindings on this element */
    String[] nsBindings;
    /** C {@code int nbNsBindings}. */
    int nbNsBindings;
    /** C {@code int sizeNsBindings}. */
    int sizeNsBindings;
    /** C {@code int hasKeyrefs}. */
    int hasKeyrefs;
    /** Indicates that an XPath has been applied. */
    int appliedXPath;
}
