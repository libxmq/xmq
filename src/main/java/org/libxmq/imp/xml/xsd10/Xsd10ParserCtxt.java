/* This file is ported from libxml2 (xmlschemas.c, xmlschemas.h,
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
import org.libxmq.imp.xml.XmlAutomata;
import org.libxmq.imp.xml.XmlAutomataState;

import static org.libxmq.imp.xml.xsd10.Xsd10Const.*;

/** libxml2 xmlSchemaParserCtxt. (C {@code XmlXsd10ParserCtxt}). */
public final class Xsd10ParserCtxt extends Xsd10AbstractCtxt
{
    /** Construct with default field values. */
    public Xsd10ParserCtxt()
    {
    }

    /** user specific error context */
    Object err_ctxt;
    /** the callback in case of errors */
    ValidityErrorFunc error;
    /** the callback in case of warning */
    ValidityWarningFunc warning;
    /** C {@code int err}. */
    int err;
    /** C {@code int nberrors}. */
    int nberrors;
    /** C {@code XmlXsd10StructuredErrorFunc serror}. */
    StructuredErrorFunc serror;
    /** C {@code XmlXsd10ConstructionCtxt * constructor}. */
    Xsd10ConstructionCtxt constructor;
    /** TODO: Move this to parser *flags*. */
    int owns_constructor;
    /** The main schema in use */
    Xsd10Schema schema;
    /** C {@code int counter}. */
    int counter;
    /** C {@code const char * URL}. */
    String URL;
    /** C {@code XmlDom * doc}. */
    XmlDocument doc;
    /** Whether the doc should be freed */
    int preserve;
    /** C {@code const char * buffer}. */
    String buffer;
    /** C {@code int size}. */
    int size;
    /** C {@code XmlAutomata * am}. */
    XmlAutomata am;
    /** C {@code XmlAutomataState * start}. */
    XmlAutomataState start;
    /** C {@code XmlAutomataState * end}. */
    XmlAutomataState end;
    /** C {@code XmlAutomataState * state}. */
    XmlAutomataState state;
    /** dictionary for interned string names */
    Xsd10Dict dict;
    /** The current context simple/complex type */
    Xsd10Type ctxt_type;
    /** C {@code int options}. */
    int options;
    /** C {@code XmlXsd10ValidCtxt * vctxt}. */
    Xsd10ValidCtxt vctxt;
    /** C {@code int is_s4_s}. */
    int is_s4_s;
    /** C {@code int is_redefine}. */
    int is_redefine;
    /** C {@code int xsi_assemble}. */
    int xsi_assemble;
    /** If the parser should stop; i.e. a critical error. */
    int stop;
    /** C {@code const char * target_namespace}. */
    String target_namespace;
    /** The schema to be redefined. */
    Xsd10Bucket redefined;
    /** Used for redefinitions. */
    Xsd10Redef redef;
    /** Used for redefinitions. */
    int redef_counter;
    /** C {@code XmlXsd10ItemList * attr_prohibs}. */
    Xsd10ItemList attr_prohibs;
    /** NULL: xml_xsd10_default_loader */
    Loader resource_loader;
    /** C {@code void * resource_ctxt}. */
    Object resource_ctxt;
}
