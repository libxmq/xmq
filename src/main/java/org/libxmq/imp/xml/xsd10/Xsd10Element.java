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

import org.libxmq.imp.xml.XmlNode;

/** An element definition (libxml2 xmlSchemaElement). XmlXsd10Type, XmlXsd10Facet and XmlXsd10Element start of structures must be kept similar (C {@code XmlXsd10Element}). */
public final class Xsd10Element extends Xsd10BasicItem
{
    /** Not used? */
    Xsd10Type next;
    /** C {@code const char * name}. */
    String name;
    /** Deprecated; not used */
    String id;
    /** Deprecated; not used */
    String ref;
    /** Deprecated; not used */
    String refNs;
    /** C {@code XmlXsd10Annot * annot}. */
    Xsd10Annot annot;
    /** the type definition */
    Xsd10Type subtypes;
    /** C {@code XmlXsd10Attribute * attributes}. */
    Xsd10Attribute attributes;
    /** C {@code XMQNode * node}. */
    XmlNode node;
    /** Deprecated; not used */
    int minOccurs;
    /** Deprecated; not used */
    int maxOccurs;
    /** C {@code int flags}. */
    int flags;
    /** C {@code const char * targetNamespace}. */
    String targetNamespace;
    /** C {@code const char * namedType}. */
    String namedType;
    /** C {@code const char * namedTypeNs}. */
    String namedTypeNs;
    /** C {@code const char * substGroup}. */
    String substGroup;
    /** C {@code const char * substGroupNs}. */
    String substGroupNs;
    /** C {@code const char * scope}. */
    String scope;
    /** The original value of the value constraint. */
    String value;
    /** C {@code struct XmlXsd10Element * refDecl}. */
    Xsd10Element refDecl;
    /** Obsolete for WXS, maybe used for RelaxNG */
    Object contModel;
    /** C {@code XmlXsd10ContentType contentType}. */
    int contentType;
    /** Deprecated; not used */
    String refPrefix;
    /** The compiled value constraint. */
    Xsd10Val defVal;
    /** The identity-constraint defs */
    Object idcs;
}
