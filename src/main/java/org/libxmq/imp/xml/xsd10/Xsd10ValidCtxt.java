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
import org.libxmq.imp.xml.XmlNode;
import org.libxmq.imp.xml.XmlRegExecCtxt;

import static org.libxmq.imp.xml.xsd10.Xsd10Const.*;

/** A Schemas validation context (libxml2 xmlSchemaValidCtxt). The streaming fields (input, enc, sax, parserCtxt, reader, locFunc, locCtxt) are not ported, ids is new. (C {@code XmlXsd10ValidCtxt}). */
public final class Xsd10ValidCtxt extends Xsd10AbstractCtxt
{
    /** user specific data block */
    Object errCtxt;
    /** the callback in case of errors */
    ValidityErrorFunc error;
    /** the callback in case of warning */
    ValidityWarningFunc warning;
    /** C {@code XmlXsd10StructuredErrorFunc serror}. */
    StructuredErrorFunc serror;
    /** The schema in use */
    Xsd10Schema schema;
    /** C {@code XmlDom * doc}. */
    XmlDocument doc;
    /** TODO: What is this for? */
    Object user_data;
    /** C {@code char * filename}. */
    String filename;
    /** C {@code int err}. */
    int err;
    /** C {@code int nberrors}. */
    int nberrors;
    /** C {@code XMQNode * node}. */
    XmlNode node;
    /** C {@code XMQNode * cur}. */
    XmlNode cur;
    /** C {@code XmlRegExecCtxt * regexp}. */
    XmlRegExecCtxt regexp;
    /** C {@code XmlXsd10Val * value}. */
    Xsd10Val value;
    /** C {@code int valueWS}. */
    int valueWS;
    /** C {@code int options}. */
    int options;
    /** C {@code XMQNode * validationRoot}. */
    XmlNode validationRoot;
    /** C {@code XmlXsd10ParserCtxt * pctxt}. */
    Xsd10ParserCtxt pctxt;
    /** C {@code int xsiAssemble}. */
    int xsiAssemble;
    /** C {@code int depth}. */
    int depth;
    /** array of element information */
    Xsd10NodeInfo[] elemInfos;
    /** C {@code int sizeElemInfos}. */
    int sizeElemInfos;
    /** the current element information */
    Xsd10NodeInfo inode;
    /** a list of augmented IDC information */
    Xsd10IDCAug aidcs;
    /** first active state object. */
    Xsd10IDCStateObj xpathStates;
    /** first stored state object. */
    Xsd10IDCStateObj xpathStatePool;
    /** Cache for IDC matcher objects. */
    Xsd10IDCMatcher idcMatcherCache;
    /** list of all IDC node-table entries */
    Xsd10PSVIIDCNode[] idcNodes;
    /** C {@code int nbIdcNodes}. */
    int nbIdcNodes;
    /** C {@code int sizeIdcNodes}. */
    int sizeIdcNodes;
    /** list of all IDC node-table entries */
    Xsd10PSVIIDCKey[] idcKeys;
    /** C {@code int nbIdcKeys}. */
    int nbIdcKeys;
    /** C {@code int sizeIdcKeys}. */
    int sizeIdcKeys;
    /** C {@code int flags}. */
    int flags;
    /** C {@code XmlXsd10Dict * dict}. */
    Xsd10Dict dict;
    /** C {@code XmlXsd10AttrInfo ** attrInfos}. */
    Xsd10AttrInfo[] attrInfos;
    /** C {@code int nbAttrInfos}. */
    int nbAttrInfos;
    /** C {@code int sizeAttrInfos}. */
    int sizeAttrInfos;
    /** C {@code int skipDepth}. */
    int skipDepth;
    /** C {@code XmlXsd10ItemList * nodeQNames}. */
    Xsd10ItemList nodeQNames;
    /** C {@code int hasKeyrefs}. */
    int hasKeyrefs;
    /** C {@code int createIDCNodeTables}. */
    int createIDCNodeTables;
    /** C {@code int psviExposeIDCNodeTables}. */
    int psviExposeIDCNodeTables;
    /** The ID attributes of the validated document (replaces libxml2 doc->ids). */
    Xsd10Ids ids;
}
