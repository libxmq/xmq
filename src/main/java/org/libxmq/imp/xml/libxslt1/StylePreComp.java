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

/**
 * A precompiled instruction (libxslt xsltStylePreComp), kept as the user data of its element.
 */
final class StylePreComp
{
    // The kind of a precompiled instruction (libxslt xsltStyleType).
    static final int FUNC_COPY = 1;
    static final int FUNC_SORT = 2;
    static final int FUNC_TEXT = 3;
    static final int FUNC_ELEMENT = 4;
    static final int FUNC_ATTRIBUTE = 5;
    static final int FUNC_COMMENT = 6;
    static final int FUNC_PI = 7;
    static final int FUNC_COPYOF = 8;
    static final int FUNC_VALUEOF = 9;
    static final int FUNC_NUMBER = 10;
    static final int FUNC_APPLYIMPORTS = 11;
    static final int FUNC_CALLTEMPLATE = 12;
    static final int FUNC_APPLYTEMPLATES = 13;
    static final int FUNC_CHOOSE = 14;
    static final int FUNC_IF = 15;
    static final int FUNC_FOREACH = 16;
    static final int FUNC_DOCUMENT = 17;
    static final int FUNC_WITHPARAM = 18;
    static final int FUNC_PARAM = 19;
    static final int FUNC_VARIABLE = 20;
    static final int FUNC_WHEN = 21;
    static final int FUNC_EXTENSION = 22;

    // The built in extension elements (extra.c): libxslt:debug and the document elements.
    static final int EXT_NONE = 0;
    static final int EXT_DEBUG = 1;
    static final int EXT_DOCUMENT = 2;

    /** The marker for elements of an unknown extension (libxslt xsltExtMarker). */
    static final Object EXT_MARKER = new Object();

    StylePreComp next;      // All of them, in the stylesheet.
    int type;
    int ext;                // For FUNC_EXTENSION and FUNC_DOCUMENT.
    XmlNode inst;
    String stype;
    int has_stype;
    int number;
    String order;
    int has_order;
    int descending;
    String lang;
    int has_lang;
    String case_order;
    int lower_first;
    String use;
    int has_use;
    int noescape;
    String name;
    int has_name;
    String ns;
    int has_ns;
    String mode;
    String mode_uri;
    String test;
    String select;
    int ver11;
    String filename;
    int has_filename;
    Template templ;         // The called template (call-template), found at runtime.
    NumberData numdata = new NumberData();
    XPath comp;
    Map<String, String> ns_list; // The namespaces in scope (xmlGetNsList), in order, the first of a prefix wins.
}
