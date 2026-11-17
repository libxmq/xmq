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

/** A transformation of a source document with a stylesheet (XmlLibxslt1Transform in the C version, libxslt xsltTransformContext). */
public final class TransformCtx
{
    static final int STATE_OK = 0;
    static final int STATE_ERROR = 1;
    static final int STATE_STOPPED = 2;

    Stylesheet style;
    XmlDocument source;
    XPathEvalContext xpath;
    int state;
    XmlNode node;           // The current node (ctxt->node).
    XmlNode inst;           // The current instruction (ctxt->inst).
    // The context position and size of the libxslt xpath context.
    long xp_position;
    long xp_size;
    String mode;
    String mode_uri;
    List<RuntimeExtra> extras = new ArrayList<>();
    int extras_nr;
    // The transformation (transform.c, variables.c).
    XmlDocument output;     // The tree being built (the result or a result tree fragment).
    XmlNode insert;         // Where new nodes are added.
    int type;               // OUTPUT_XML, OUTPUT_HTML, OUTPUT_TEXT.
    Template templ;         // The current template.
    List<Template> templ_tab = new ArrayList<>();
    int templ_nr;
    List<StackElem> vars_tab = new ArrayList<>(); // The stack of local variables and parameters.
    int vars_nr;
    int vars_base;
    StackElem vars;
    HashMap<String, StackElem> global_vars;      // "name\1uri" -> StackElem
    StackElem global_vars_list;
    Template current_template_rule;
    NodeList node_list;      // The node list being processed (for xsl:sort).
    int depth;
    int max_template_depth;
    int max_template_vars;
    XmlNode initial_context_node;
    XmlDocument initial_context_doc;
    StackElem context_variable;
    RVT tmp_rvt;             // Result tree fragments of the template (xsltRegisterTmpRVT).
    RVT local_rvt;           // Of the current instruction (xsltRegisterLocalRVT).
    RVT persist_rvt;         // Kept until the end (global variables).
    long op_count;
    long op_limit;
    DocInfo doc_list;        // Documents of the transformation (the source and document()), with their keys.
    DocInfo document;       // The current document.
    int has_templ_key_patterns;
    int nb_keys;
    int key_init_level;
    String[] params;        // name, value pairs (xpath expressions).
    java.util.IdentityHashMap<XmlNode, Boolean> keyed_nodes; // The source nodes with keys (XSLT_SOURCE_NODE_HAS_KEY).
    java.util.IdentityHashMap<XmlNode, String> generated_ids;  // node -> id of generate-id().
    String output_file;      // The file the result goes to (ctxt->output_file), for xsl:document.
    long current_id;

    TransformCtx()
    {
    }
}
