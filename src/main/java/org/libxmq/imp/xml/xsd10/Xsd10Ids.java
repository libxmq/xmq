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

import java.util.ArrayList;
import java.util.HashMap;
import java.util.IdentityHashMap;

import org.libxmq.imp.xml.XmlNode;

/** The ID table of a document (C {@code struct XmlXsd10Ids} of xml_xsd10_types.c): replaces libxml2
    doc->ids, doc->refs and attr->atype. The functions are idsNew, idsAddId ... in Xsd10Types. */
public final class Xsd10Ids
{
    /** C XmlXsd10IdRef. */
    static final class IdRef
    {
        /** char *value */
        String value;
        /** XMQNode *attr */
        XmlNode attr;
    }

    /** id value -&gt; the attribute node (C HashMap *ids). */
    HashMap<String, XmlNode> ids = new HashMap<>();
    /** attribute node (identity) -&gt; atype (C HashMap *atypes keyed on "%p"). */
    IdentityHashMap<XmlNode, Integer> atypes = new IdentityHashMap<>();
    /** The references in the order added (C refs, num_refs, max_refs). */
    ArrayList<IdRef> refs = new ArrayList<>();
}
