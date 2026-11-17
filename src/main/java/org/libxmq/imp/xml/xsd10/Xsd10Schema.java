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

import org.libxmq.imp.xml.XmlDocument;

/** A schema (libxml2 struct _xmlSchema of schemasInternals.h). (C {@code XmlXsd10Schema}). */
public final class Xsd10Schema
{
    /** Construct with default field values. */
    public Xsd10Schema()
    {
    }

    /** schema name */
    String name;
    /** the target namespace */
    String target_namespace;
    /** C {@code const char * version}. */
    String version;
    /** Obsolete */
    String id;
    /** C {@code XmlDom * doc}. */
    XmlDocument doc;
    /** C {@code XmlXsd10Annot * annot}. */
    Xsd10Annot annot;
    /** C {@code int flags}. */
    int flags;
    /** C {@code XmlXsd10Hash * type_decl}. */
    Xsd10Hash type_decl;
    /** C {@code XmlXsd10Hash * attr_decl}. */
    Xsd10Hash attr_decl;
    /** C {@code XmlXsd10Hash * attrgrp_decl}. */
    Xsd10Hash attrgrp_decl;
    /** C {@code XmlXsd10Hash * elem_decl}. */
    Xsd10Hash elem_decl;
    /** C {@code XmlXsd10Hash * nota_decl}. */
    Xsd10Hash nota_decl;
    /** C {@code XmlXsd10Hash * schemas_imports}. */
    Xsd10Hash schemas_imports;
    /** unused by the library for users or bindings */
    Object _private;
    /** C {@code XmlXsd10Hash * group_decl}. */
    Xsd10Hash group_decl;
    /** C {@code XmlXsd10Dict * dict}. */
    Xsd10Dict dict;
    /** the includes, this is opaque for now */
    Object includes;
    /** whether to free the document */
    int preserve;
    /** used to give anonymous components unique names */
    int counter;
    /** All identity-constraint defs. */
    Xsd10Hash idc_def;
    /** Obsolete */
    Object volatiles;
}
