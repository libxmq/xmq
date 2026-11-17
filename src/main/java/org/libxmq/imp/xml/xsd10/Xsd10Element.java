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
    /** Construct with default field values. */
    public Xsd10Element()
    {
    }

    /** Not used? */
    Xsd10Type next;
    /** C {@code const char * name}. */
    String name;
    /** Deprecated; not used */
    String id;
    /** Deprecated; not used */
    String ref;
    /** Deprecated; not used */
    String ref_ns;
    /** C {@code XmlXsd10Annot * annot}. */
    Xsd10Annot annot;
    /** the type definition */
    Xsd10Type subtypes;
    /** C {@code XmlXsd10Attribute * attributes}. */
    Xsd10Attribute attributes;
    /** C {@code XMQNode * node}. */
    XmlNode node;
    /** Deprecated; not used */
    int min_occurs;
    /** Deprecated; not used */
    int max_occurs;
    /** C {@code int flags}. */
    int flags;
    /** C {@code const char * target_namespace}. */
    String target_namespace;
    /** C {@code const char * named_type}. */
    String named_type;
    /** C {@code const char * named_type_ns}. */
    String named_type_ns;
    /** C {@code const char * subst_group}. */
    String subst_group;
    /** C {@code const char * subst_group_ns}. */
    String subst_group_ns;
    /** C {@code const char * scope}. */
    String scope;
    /** The original value of the value constraint. */
    String value;
    /** C {@code struct XmlXsd10Element * ref_decl}. */
    Xsd10Element ref_decl;
    /** Obsolete for WXS, maybe used for RelaxNG */
    Object cont_model;
    /** C {@code XmlXsd10ContentType content_type}. */
    int content_type;
    /** Deprecated; not used */
    String ref_prefix;
    /** The compiled value constraint. */
    Xsd10Val def_val;
    /** The identity-constraint defs */
    Object idcs;
}
