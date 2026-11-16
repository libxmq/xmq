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

/** Schemas type definition (libxml2 xmlSchemaType). (C {@code XmlXsd10Type}). */
public final class Xsd10Type extends Xsd10BasicItem
{
    /** the next type if in a sequence ... */
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
    /** C {@code XmlXsd10Type * subtypes}. */
    Xsd10BasicItem subtypes;
    /** Deprecated; not used */
    Xsd10Attribute attributes;
    /** C {@code XMQNode * node}. */
    XmlNode node;
    /** Deprecated; not used */
    int minOccurs;
    /** Deprecated; not used */
    int maxOccurs;
    /** C {@code int flags}. */
    int flags;
    /** C {@code XmlXsd10ContentType contentType}. */
    int contentType;
    /** Base type's local name */
    String base;
    /** Base type's target namespace */
    String baseNs;
    /** The base type component */
    Xsd10Type baseType;
    /** Local facets */
    Xsd10Facet facets;
    /** Deprecated; not used */
    Xsd10Type redef;
    /** Obsolete */
    int recurse;
    /** Deprecated; not used */
    Xsd10AttributeLink[] attributeUses;
    /** C {@code XmlXsd10Wildcard * attributeWildcard}. */
    Xsd10Wildcard attributeWildcard;
    /** Type of built-in types. */
    int builtInType;
    /** member-types if a union type. */
    Xsd10TypeLink memberTypes;
    /** All facets (incl. inherited) */
    Xsd10FacetLink facetSet;
    /** Deprecated; not used */
    String refPrefix;
    /** C {@code XmlXsd10Type * contentTypeDef}. */
    Xsd10Type contentTypeDef;
    /** Holds the automaton of the content model (a compiled regexp) */
    Object contModel;
    /** C {@code const char * targetNamespace}. */
    String targetNamespace;
    /** C {@code void * attrUses}. */
    Object attrUses;
}
