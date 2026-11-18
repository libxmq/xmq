/* This file is ported from libxslt (version 1.1.45 as found in the
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
    XsltDocument doc_list;
    XmlDocument doc;
    HashMap<String, String> strip_spaces;      // "name\1uri" -> "strip" or "preserve".
    int strip_all;
    HashMap<String, String> cdata_section;     // "name\1uri" -> "cdata".
    StackElem variables;    // Global variables and parameters.
    Template templates;
    HashMap<String, Pattern> templates_hash;  // Template rules by the name of their last step, mode and mode uri.
    Pattern root_match;
    Pattern key_match;
    Pattern elem_match;
    Pattern attr_match;
    Pattern parent_match;
    Pattern text_match;
    Pattern pi_match;
    Pattern comment_match;
    HashMap<String, String> ns_aliases;        // literal namespace -> target namespace (or UNDEFINED_DEFAULT_NS).
    HashMap<String, AttrSet> attribute_sets;   // "name\1uri" -> AttrSet.
    AttrSet attribute_set_list; // The same, in declaration order.
    ExtDef ns_defs;
    KeyDef keys;
    // Output.
    String method;
    String method_uri;
    String version;
    String encoding;
    int omit_xml_declaration;
    DecimalFormat decimal_format;
    int standalone;
    String doctype_public;
    String doctype_system;
    int indent;
    String media_type;
    StylePreComp pre_comps;
    int warnings;
    int errors;
    String excl_prefix;
    List<String> excl_prefix_tab = new ArrayList<>();
    int extras_nr;           // The number of runtime extras allocated by the patterns.
    XsltDocument includes;  // For keeping track of nested includes.
    AttrVT att_vts;
    String default_alias;
    int nopreproc;
    int literal_result;
    Stylesheet principal;
    int forwards_compatible;
    HashMap<String, Template> named_templates; // name and uri -> Template.
    long op_count;
    long op_limit;

    Stylesheet()
    {
    }
}
