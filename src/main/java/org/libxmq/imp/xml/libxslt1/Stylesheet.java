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

/* Portions of this file are ported from libxslt (version 1.1.45 as found in the
   libxmq fork in 3rdparty/libxslt). libxslt is covered by the following notice
   (from libxslt/Copyright):

 Copyright (C) 2001-2002 Daniel Veillard.  All Rights Reserved.

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
DANIEL VEILLARD BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CON-
NECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of Daniel Veillard shall not
be used in advertising or otherwise to promote the sale, use or other deal-
ings in this Software without prior written authorization from him.
*/

package org.libxmq.imp.xml.libxslt1;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import org.libxmq.imp.xml.XPath;
import org.libxmq.imp.xml.XPathEvalContext;
import org.libxmq.imp.xml.XPathResult;
import org.libxmq.imp.xml.XmlDocument;
import org.libxmq.imp.xml.XmlNode;
import org.libxmq.imp.xml.XmlNs;

/** A compiled stylesheet (XmlLibxslt1Stylesheet in the C version, libxslt xsltStylesheet). */
public final class Stylesheet
{
    Settings settings;
    // The stylesheet import relation is kept as a tree.
    Stylesheet parent;
    Stylesheet next;
    Stylesheet imports;
    XsltDocument docList;
    XmlDocument doc;
    HashMap<String, String> stripSpaces;      // "name\1uri" -> "strip" or "preserve".
    int stripAll;
    HashMap<String, String> cdataSection;     // "name\1uri" -> "cdata".
    StackElem variables;    // Global variables and parameters.
    Template templates;
    HashMap<String, Pattern> templatesHash;  // Template rules by the name of their last step, mode and mode uri.
    Pattern rootMatch;
    Pattern keyMatch;
    Pattern elemMatch;
    Pattern attrMatch;
    Pattern parentMatch;
    Pattern textMatch;
    Pattern piMatch;
    Pattern commentMatch;
    HashMap<String, String> nsAliases;        // literal namespace -> target namespace (or UNDEFINED_DEFAULT_NS).
    HashMap<String, AttrSet> attributeSets;   // "name\1uri" -> AttrSet.
    AttrSet attributeSetList; // The same, in declaration order.
    ExtDef nsDefs;
    KeyDef keys;
    // Output.
    String method;
    String methodUri;
    String version;
    String encoding;
    int omitXmlDeclaration;
    DecimalFormat decimalFormat;
    int standalone;
    String doctypePublic;
    String doctypeSystem;
    int indent;
    String mediaType;
    StylePreComp preComps;
    int warnings;
    int errors;
    String exclPrefix;
    List<String> exclPrefixTab = new ArrayList<>();
    int extrasNr;           // The number of runtime extras allocated by the patterns.
    XsltDocument includes;  // For keeping track of nested includes.
    AttrVT attVts;
    String defaultAlias;
    int nopreproc;
    int literalResult;
    Stylesheet principal;
    int forwardsCompatible;
    HashMap<String, Template> namedTemplates; // name and uri -> Template.
    long opCount;
    long opLimit;

    Stylesheet()
    {
    }
}
