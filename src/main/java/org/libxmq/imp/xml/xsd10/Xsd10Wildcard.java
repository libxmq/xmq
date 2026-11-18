/* This file is ported from libxml2 (xmlschemastypes.h and
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

/** A wildcard (libxml2 xmlSchemaWildcard). (C {@code XmlXsd10Wildcard}). */
public final class Xsd10Wildcard extends Xsd10BasicItem
{
    /** Construct with default field values. */
    public Xsd10Wildcard()
    {
    }

    /** Deprecated; not used */
    String id;
    /** C {@code XmlXsd10Annot * annot}. */
    Xsd10Annot annot;
    /** C {@code XMQNode * node}. */
    XmlNode node;
    /** Deprecated; not used */
    int min_occurs;
    /** Deprecated; not used */
    int max_occurs;
    /** C {@code int process_contents}. */
    int process_contents;
    /** Indicates if the ns constraint is of ##any */
    int any;
    /** The list of allowed namespaces */
    Xsd10WildcardNs ns_set;
    /** The negated namespace */
    Xsd10WildcardNs neg_ns_set;
    /** C {@code int flags}. */
    int flags;
}
