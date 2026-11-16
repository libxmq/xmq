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

/**
 * The constants (every #define / enum value of xml_xsd10_schemas.h and xml_xsd10_types.h, same names,
 * enum typedefs are plain int) and the callback interfaces of the xsd 1.0 port.
 *
 * <p>DESIGN of the Java port (contract for all Xsd10* classes):
 * <ul>
 * <li>Namespace strings: the C arrays xml_xsd10_ns, xml_xsd10_instance_ns, xml_xsd10_namespace_ns are the
 *   String constants {@link #XML_XSD10_NS}, {@link #XML_XSD10_INSTANCE_NS}, {@link #XML_XSD10_NAMESPACE_NS}
 *   here. String valued #defines (XML_XSD10_SCHEMAS_NO_NAMESPACE ...) are String constants too.</li>
 * <li>Callback typedefs (XmlXsd10ValidityErrorFunc, ...WarningFunc, ...StructuredErrorFunc, XmlXsd10Loader,
 *   XmlXsd10HashDeallocator, ...HashScanner, ...HashScannerFull) are the nested interfaces below with the
 *   XmlXsd10 prefix dropped. {@code import static ...Xsd10Const.*} imports them. The regexp hooks struct
 *   is the class Xsd10RegexpHooks (nested interfaces Compile/Exec/Free).</li>
 * <li>Component casts. C casts between the structs sharing a leading prefix become Java casts in this
 *   hierarchy:
 *   <pre>
 *   Xsd10BasicItem {int type}                       (xmlSchemaBasicItem, "dummy" dropped)
 *     Xsd10AnnotItem {annot}
 *       Xsd10TreeItem {Xsd10TreeItem next; Xsd10BasicItem children}
 *         Xsd10Particle, Xsd10ModelGroup, Xsd10ModelGroupDef
 *       Xsd10AttributeUse {Xsd10AttributeUse next; Xsd10BasicItem attrDecl; ...}
 *       Xsd10IDC {Xsd10IDC next; ...}
 *     Xsd10Type, Xsd10Element, Xsd10Facet, Xsd10Attribute, Xsd10AttributeGroup, Xsd10Wildcard,
 *     Xsd10Notation, Xsd10QNameRef, Xsd10AttributeUseProhib
 *   Xsd10AbstractCtxt {int type}  -&gt; Xsd10ParserCtxt, Xsd10ValidCtxt
 *   Xsd10Bucket                   -&gt; Xsd10Import, Xsd10Include
 *   Xsd10NodeInfo                 -&gt; Xsd10AttrInfo (vctxt.inode = (Xsd10NodeInfo) iattr works)
 *   </pre>
 *   Fields that C declares with one type but fills with other components are typed with the common
 *   superclass and need a cast on use: {@code Xsd10TreeItem.children} (term of a particle: element,
 *   wildcard, model group, model group def, QName ref), {@code Xsd10Type.subtypes} (a type, or the content
 *   particle of a complex type), {@code Xsd10TypeLink.type} (a QName ref before fixup),
 *   {@code Xsd10AttributeUse.attrDecl} (an attribute or a QName ref). Everything else keeps the C type.
 *   A C cast to XmlXsd10TreeItem* / XmlXsd10AnnotItem* of an item that is not one in Java (e.g.
 *   pending items, "(XmlXsd10AnnotItem *) type") is a cast to Xsd10BasicItem when only -&gt;type is read;
 *   C type punning (reading a field through the wrong struct) must become an instanceof + cast to the
 *   real class (e.g. MODEL_GROUP_CAST(mgd)-&gt;children: ((Xsd10TreeItem) mgd).children). Never let a
 *   ClassCastException escape: guard with instanceof where the C cast is not guaranteed by the type field.
 *   Xsd10AttrInfo.decl (Xsd10Attribute) hides Xsd10NodeInfo.decl (Xsd10Element): through a NodeInfo
 *   reference to an AttrInfo you get the (null) element decl; cast to Xsd10AttrInfo to get the attribute.</li>
 * <li>Pointer out-params {@code T *x} / {@code T **x} are arrays {@code T[] x} of length 1 (caller
 *   allocates, null where C allows NULL); real C arrays (+count) are Java arrays. {@code void **items} of
 *   item lists is {@code Object[] items} (+ nbItems/sizeItems kept like C). C varargs {@code ...} is
 *   {@code Object... args}, a va_list is {@code Object[] ap}. size_t / unsigned long are long, other ints int.</li>
 * <li>Function like macros of the headers are static methods of Xsd10Schemas (XSD10_WXS_IS_COMPLEX -&gt;
 *   wxsIsComplex, XSD10_IS_SCHEMA -&gt; isSchema, XSD10_VERROR -&gt; verror(vctxt, ...), XSD10_PERROR_INT -&gt;
 *   perrorInt(pctxt, ...), XSD10_WXS_ADD_LOCAL(ctx, item) -&gt; {@code item = wxsAddLocal(ctx, item)} ...).
 *   Control flow macros (XSD10_HFAILURE, XSD10_HERROR, XSD10_HSTOP) and XSD10_FREE_AND_NULL are written
 *   out inline (if (res == -1) break/return ...; x = null). Cast macros are Java casts.</li>
 * </ul>
 */
public interface Xsd10Const
{
    /** The XML Schemas namespace (C xml_xsd10_ns, libxml2 xmlSchemaNs). */
    String XML_XSD10_NS = "http://www.w3.org/2001/XMLSchema";
    /** The XML Schemas instance namespace (C xml_xsd10_instance_ns). */
    String XML_XSD10_INSTANCE_NS = "http://www.w3.org/2001/XMLSchema-instance";
    /** The xmlns namespace (C xml_xsd10_namespace_ns). */
    String XML_XSD10_NAMESPACE_NS = "http://www.w3.org/2000/xmlns/";

    /** C XmlXsd10ValidityErrorFunc: called as error(ctx, "%s", message) like libxml2. */
    @FunctionalInterface
    interface ValidityErrorFunc
    {
        /** @param ctx user context @param msg printf format @param args the arguments */
        void error(Object ctx, String msg, Object... args);
    }
    /** C XmlXsd10ValidityWarningFunc. */
    @FunctionalInterface
    interface ValidityWarningFunc
    {
        /** @param ctx user context @param msg printf format @param args the arguments */
        void warning(Object ctx, String msg, Object... args);
    }
    /** C XmlXsd10StructuredErrorFunc (libxml2 xmlStructuredErrorFunc). */
    @FunctionalInterface
    interface StructuredErrorFunc
    {
        /** @param userData user data @param error the error, only valid during the call */
        void error(Object userData, Xsd10Error error);
    }
    /** C XmlXsd10Loader: load the schema document at uri, null if it cannot be loaded. */
    @FunctionalInterface
    interface Loader
    {
        /** @param userData user data @param uri the uri @return the document or null */
        XmlDocument load(Object userData, String uri);
    }
    /** C XmlXsd10HashDeallocator. */
    @FunctionalInterface
    interface HashDeallocator
    {
        /** @param payload the payload @param name the first key */
        void dealloc(Object payload, String name);
    }
    /** C XmlXsd10HashScanner. */
    @FunctionalInterface
    interface HashScanner
    {
        /** @param payload the payload @param data user data @param name the first key */
        void scan(Object payload, Object data, String name);
    }
    /** C XmlXsd10HashScannerFull. */
    @FunctionalInterface
    interface HashScannerFull
    {
        /** @param payload the payload @param data user data @param name key @param name2 key2 @param name3 key3 */
        void scan(Object payload, Object data, String name, String name2, String name3);
    }

    /** C {@code XML_XSD10_SCHEMAS_UNKNOWN} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMAS_UNKNOWN = 0;
    /** C {@code XML_XSD10_SCHEMAS_STRING} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMAS_STRING = 1;
    /** C {@code XML_XSD10_SCHEMAS_NORMSTRING} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMAS_NORMSTRING = 2;
    /** C {@code XML_XSD10_SCHEMAS_DECIMAL} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMAS_DECIMAL = 3;
    /** C {@code XML_XSD10_SCHEMAS_TIME} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMAS_TIME = 4;
    /** C {@code XML_XSD10_SCHEMAS_GDAY} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMAS_GDAY = 5;
    /** C {@code XML_XSD10_SCHEMAS_GMONTH} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMAS_GMONTH = 6;
    /** C {@code XML_XSD10_SCHEMAS_GMONTHDAY} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMAS_GMONTHDAY = 7;
    /** C {@code XML_XSD10_SCHEMAS_GYEAR} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMAS_GYEAR = 8;
    /** C {@code XML_XSD10_SCHEMAS_GYEARMONTH} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMAS_GYEARMONTH = 9;
    /** C {@code XML_XSD10_SCHEMAS_DATE} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMAS_DATE = 10;
    /** C {@code XML_XSD10_SCHEMAS_DATETIME} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMAS_DATETIME = 11;
    /** C {@code XML_XSD10_SCHEMAS_DURATION} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMAS_DURATION = 12;
    /** C {@code XML_XSD10_SCHEMAS_FLOAT} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMAS_FLOAT = 13;
    /** C {@code XML_XSD10_SCHEMAS_DOUBLE} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMAS_DOUBLE = 14;
    /** C {@code XML_XSD10_SCHEMAS_BOOLEAN} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMAS_BOOLEAN = 15;
    /** C {@code XML_XSD10_SCHEMAS_TOKEN} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMAS_TOKEN = 16;
    /** C {@code XML_XSD10_SCHEMAS_LANGUAGE} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMAS_LANGUAGE = 17;
    /** C {@code XML_XSD10_SCHEMAS_NMTOKEN} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMAS_NMTOKEN = 18;
    /** C {@code XML_XSD10_SCHEMAS_NMTOKENS} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMAS_NMTOKENS = 19;
    /** C {@code XML_XSD10_SCHEMAS_NAME} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMAS_NAME = 20;
    /** C {@code XML_XSD10_SCHEMAS_QNAME} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMAS_QNAME = 21;
    /** C {@code XML_XSD10_SCHEMAS_NCNAME} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMAS_NCNAME = 22;
    /** C {@code XML_XSD10_SCHEMAS_ID} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMAS_ID = 23;
    /** C {@code XML_XSD10_SCHEMAS_IDREF} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMAS_IDREF = 24;
    /** C {@code XML_XSD10_SCHEMAS_IDREFS} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMAS_IDREFS = 25;
    /** C {@code XML_XSD10_SCHEMAS_ENTITY} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMAS_ENTITY = 26;
    /** C {@code XML_XSD10_SCHEMAS_ENTITIES} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMAS_ENTITIES = 27;
    /** C {@code XML_XSD10_SCHEMAS_NOTATION} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMAS_NOTATION = 28;
    /** C {@code XML_XSD10_SCHEMAS_ANYURI} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMAS_ANYURI = 29;
    /** C {@code XML_XSD10_SCHEMAS_INTEGER} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMAS_INTEGER = 30;
    /** C {@code XML_XSD10_SCHEMAS_NPINTEGER} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMAS_NPINTEGER = 31;
    /** C {@code XML_XSD10_SCHEMAS_NINTEGER} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMAS_NINTEGER = 32;
    /** C {@code XML_XSD10_SCHEMAS_NNINTEGER} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMAS_NNINTEGER = 33;
    /** C {@code XML_XSD10_SCHEMAS_PINTEGER} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMAS_PINTEGER = 34;
    /** C {@code XML_XSD10_SCHEMAS_INT} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMAS_INT = 35;
    /** C {@code XML_XSD10_SCHEMAS_UINT} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMAS_UINT = 36;
    /** C {@code XML_XSD10_SCHEMAS_LONG} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMAS_LONG = 37;
    /** C {@code XML_XSD10_SCHEMAS_ULONG} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMAS_ULONG = 38;
    /** C {@code XML_XSD10_SCHEMAS_SHORT} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMAS_SHORT = 39;
    /** C {@code XML_XSD10_SCHEMAS_USHORT} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMAS_USHORT = 40;
    /** C {@code XML_XSD10_SCHEMAS_BYTE} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMAS_BYTE = 41;
    /** C {@code XML_XSD10_SCHEMAS_UBYTE} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMAS_UBYTE = 42;
    /** C {@code XML_XSD10_SCHEMAS_HEXBINARY} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMAS_HEXBINARY = 43;
    /** C {@code XML_XSD10_SCHEMAS_BASE64BINARY} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMAS_BASE64BINARY = 44;
    /** C {@code XML_XSD10_SCHEMAS_ANYTYPE} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMAS_ANYTYPE = 45;
    /** C {@code XML_XSD10_SCHEMAS_ANYSIMPLETYPE} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMAS_ANYSIMPLETYPE = 46;
    /** C {@code XML_XSD10_SCHEMA_TYPE_BASIC} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMA_TYPE_BASIC = 1;
    /** C {@code XML_XSD10_SCHEMA_TYPE_ANY} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMA_TYPE_ANY = 2;
    /** C {@code XML_XSD10_SCHEMA_TYPE_FACET} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMA_TYPE_FACET = 3;
    /** C {@code XML_XSD10_SCHEMA_TYPE_SIMPLE} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMA_TYPE_SIMPLE = 4;
    /** C {@code XML_XSD10_SCHEMA_TYPE_COMPLEX} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMA_TYPE_COMPLEX = 5;
    /** C {@code XML_XSD10_SCHEMA_TYPE_SEQUENCE} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMA_TYPE_SEQUENCE = 6;
    /** C {@code XML_XSD10_SCHEMA_TYPE_CHOICE} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMA_TYPE_CHOICE = 7;
    /** C {@code XML_XSD10_SCHEMA_TYPE_ALL} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMA_TYPE_ALL = 8;
    /** C {@code XML_XSD10_SCHEMA_TYPE_SIMPLE_CONTENT} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMA_TYPE_SIMPLE_CONTENT = 9;
    /** C {@code XML_XSD10_SCHEMA_TYPE_COMPLEX_CONTENT} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMA_TYPE_COMPLEX_CONTENT = 10;
    /** C {@code XML_XSD10_SCHEMA_TYPE_UR} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMA_TYPE_UR = 11;
    /** C {@code XML_XSD10_SCHEMA_TYPE_RESTRICTION} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMA_TYPE_RESTRICTION = 12;
    /** C {@code XML_XSD10_SCHEMA_TYPE_EXTENSION} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMA_TYPE_EXTENSION = 13;
    /** C {@code XML_XSD10_SCHEMA_TYPE_ELEMENT} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMA_TYPE_ELEMENT = 14;
    /** C {@code XML_XSD10_SCHEMA_TYPE_ATTRIBUTE} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMA_TYPE_ATTRIBUTE = 15;
    /** C {@code XML_XSD10_SCHEMA_TYPE_ATTRIBUTEGROUP} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMA_TYPE_ATTRIBUTEGROUP = 16;
    /** C {@code XML_XSD10_SCHEMA_TYPE_GROUP} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMA_TYPE_GROUP = 17;
    /** C {@code XML_XSD10_SCHEMA_TYPE_NOTATION} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMA_TYPE_NOTATION = 18;
    /** C {@code XML_XSD10_SCHEMA_TYPE_LIST} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMA_TYPE_LIST = 19;
    /** C {@code XML_XSD10_SCHEMA_TYPE_UNION} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMA_TYPE_UNION = 20;
    /** C {@code XML_XSD10_SCHEMA_TYPE_ANY_ATTRIBUTE} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMA_TYPE_ANY_ATTRIBUTE = 21;
    /** C {@code XML_XSD10_SCHEMA_TYPE_IDC_UNIQUE} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMA_TYPE_IDC_UNIQUE = 22;
    /** C {@code XML_XSD10_SCHEMA_TYPE_IDC_KEY} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMA_TYPE_IDC_KEY = 23;
    /** C {@code XML_XSD10_SCHEMA_TYPE_IDC_KEYREF} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMA_TYPE_IDC_KEYREF = 24;
    /** C {@code XML_XSD10_SCHEMA_TYPE_PARTICLE} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMA_TYPE_PARTICLE = 25;
    /** C {@code XML_XSD10_SCHEMA_TYPE_ATTRIBUTE_USE} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMA_TYPE_ATTRIBUTE_USE = 26;
    /** C {@code XML_XSD10_SCHEMA_FACET_MININCLUSIVE} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMA_FACET_MININCLUSIVE = 1000;
    /** C {@code XML_XSD10_SCHEMA_FACET_MINEXCLUSIVE} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMA_FACET_MINEXCLUSIVE = 1001;
    /** C {@code XML_XSD10_SCHEMA_FACET_MAXINCLUSIVE} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMA_FACET_MAXINCLUSIVE = 1002;
    /** C {@code XML_XSD10_SCHEMA_FACET_MAXEXCLUSIVE} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMA_FACET_MAXEXCLUSIVE = 1003;
    /** C {@code XML_XSD10_SCHEMA_FACET_TOTALDIGITS} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMA_FACET_TOTALDIGITS = 1004;
    /** C {@code XML_XSD10_SCHEMA_FACET_FRACTIONDIGITS} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMA_FACET_FRACTIONDIGITS = 1005;
    /** C {@code XML_XSD10_SCHEMA_FACET_PATTERN} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMA_FACET_PATTERN = 1006;
    /** C {@code XML_XSD10_SCHEMA_FACET_ENUMERATION} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMA_FACET_ENUMERATION = 1007;
    /** C {@code XML_XSD10_SCHEMA_FACET_WHITESPACE} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMA_FACET_WHITESPACE = 1008;
    /** C {@code XML_XSD10_SCHEMA_FACET_LENGTH} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMA_FACET_LENGTH = 1009;
    /** C {@code XML_XSD10_SCHEMA_FACET_MAXLENGTH} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMA_FACET_MAXLENGTH = 1010;
    /** C {@code XML_XSD10_SCHEMA_FACET_MINLENGTH} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMA_FACET_MINLENGTH = 1011;
    /** C {@code XML_XSD10_SCHEMA_EXTRA_QNAMEREF} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMA_EXTRA_QNAMEREF = 2000;
    /** C {@code XML_XSD10_SCHEMA_EXTRA_ATTR_USE_PROHIB} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMA_EXTRA_ATTR_USE_PROHIB = 2001;
    /** C {@code XML_XSD10_SCHEMA_CONTENT_UNKNOWN} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMA_CONTENT_UNKNOWN = 0;
    /** C {@code XML_XSD10_SCHEMA_CONTENT_EMPTY} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMA_CONTENT_EMPTY = 1;
    /** C {@code XML_XSD10_SCHEMA_CONTENT_ELEMENTS} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMA_CONTENT_ELEMENTS = 2;
    /** C {@code XML_XSD10_SCHEMA_CONTENT_MIXED} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMA_CONTENT_MIXED = 3;
    /** C {@code XML_XSD10_SCHEMA_CONTENT_SIMPLE} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMA_CONTENT_SIMPLE = 4;
    /** C {@code XML_XSD10_SCHEMA_CONTENT_MIXED_OR_ELEMENTS} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMA_CONTENT_MIXED_OR_ELEMENTS = 5;
    /** C {@code XML_XSD10_SCHEMA_CONTENT_BASIC} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMA_CONTENT_BASIC = 6;
    /** C {@code XML_XSD10_SCHEMA_CONTENT_ANY} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMA_CONTENT_ANY = 7;
    /** C {@code XML_XSD10_SCHEMA_WHITESPACE_UNKNOWN} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMA_WHITESPACE_UNKNOWN = 0;
    /** C {@code XML_XSD10_SCHEMA_WHITESPACE_PRESERVE} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMA_WHITESPACE_PRESERVE = 1;
    /** C {@code XML_XSD10_SCHEMA_WHITESPACE_REPLACE} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMA_WHITESPACE_REPLACE = 2;
    /** C {@code XML_XSD10_SCHEMA_WHITESPACE_COLLAPSE} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMA_WHITESPACE_COLLAPSE = 3;
    /** C {@code XML_XSD10_SCHEMAP_INVALID_FACET_VALUE} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMAP_INVALID_FACET_VALUE = 1717;
    /** C {@code XML_XSD10_SCHEMAP_REGEXP_INVALID} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMAP_REGEXP_INVALID = 1756;
    /** C {@code XML_XSD10_SCHEMAV_CVC_DATATYPE_VALID_1_2_1} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMAV_CVC_DATATYPE_VALID_1_2_1 = 1824;
    /** C {@code XML_XSD10_SCHEMAV_CVC_DATATYPE_VALID_1_2_2} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMAV_CVC_DATATYPE_VALID_1_2_2 = 1825;
    /** C {@code XML_XSD10_SCHEMAV_CVC_LENGTH_VALID} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMAV_CVC_LENGTH_VALID = 1830;
    /** C {@code XML_XSD10_SCHEMAV_CVC_MINLENGTH_VALID} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMAV_CVC_MINLENGTH_VALID = 1831;
    /** C {@code XML_XSD10_SCHEMAV_CVC_MAXLENGTH_VALID} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMAV_CVC_MAXLENGTH_VALID = 1832;
    /** C {@code XML_XSD10_SCHEMAV_CVC_MININCLUSIVE_VALID} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMAV_CVC_MININCLUSIVE_VALID = 1833;
    /** C {@code XML_XSD10_SCHEMAV_CVC_MAXINCLUSIVE_VALID} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMAV_CVC_MAXINCLUSIVE_VALID = 1834;
    /** C {@code XML_XSD10_SCHEMAV_CVC_MINEXCLUSIVE_VALID} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMAV_CVC_MINEXCLUSIVE_VALID = 1835;
    /** C {@code XML_XSD10_SCHEMAV_CVC_MAXEXCLUSIVE_VALID} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMAV_CVC_MAXEXCLUSIVE_VALID = 1836;
    /** C {@code XML_XSD10_SCHEMAV_CVC_TOTALDIGITS_VALID} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMAV_CVC_TOTALDIGITS_VALID = 1837;
    /** C {@code XML_XSD10_SCHEMAV_CVC_FRACTIONDIGITS_VALID} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMAV_CVC_FRACTIONDIGITS_VALID = 1838;
    /** C {@code XML_XSD10_SCHEMAV_CVC_PATTERN_VALID} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMAV_CVC_PATTERN_VALID = 1839;
    /** C {@code XML_XSD10_SCHEMAV_CVC_ENUMERATION_VALID} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMAV_CVC_ENUMERATION_VALID = 1840;
    /** C {@code XML_XSD10_ATTRIBUTE_CDATA} (xml_xsd10_types.h). */
    int XML_XSD10_ATTRIBUTE_CDATA = 1;
    /** C {@code XML_XSD10_ATTRIBUTE_ID} (xml_xsd10_types.h). */
    int XML_XSD10_ATTRIBUTE_ID = 2;
    /** C {@code XML_XSD10_ATTRIBUTE_IDREF} (xml_xsd10_types.h). */
    int XML_XSD10_ATTRIBUTE_IDREF = 3;
    /** C {@code XML_XSD10_ATTRIBUTE_IDREFS} (xml_xsd10_types.h). */
    int XML_XSD10_ATTRIBUTE_IDREFS = 4;
    /** C {@code XML_XSD10_ATTRIBUTE_ENTITY} (xml_xsd10_types.h). */
    int XML_XSD10_ATTRIBUTE_ENTITY = 5;
    /** C {@code XML_XSD10_ATTRIBUTE_ENTITIES} (xml_xsd10_types.h). */
    int XML_XSD10_ATTRIBUTE_ENTITIES = 6;
    /** C {@code XML_XSD10_ATTRIBUTE_NMTOKEN} (xml_xsd10_types.h). */
    int XML_XSD10_ATTRIBUTE_NMTOKEN = 7;
    /** C {@code XML_XSD10_ATTRIBUTE_NMTOKENS} (xml_xsd10_types.h). */
    int XML_XSD10_ATTRIBUTE_NMTOKENS = 8;
    /** C {@code XML_XSD10_ATTRIBUTE_ENUMERATION} (xml_xsd10_types.h). */
    int XML_XSD10_ATTRIBUTE_ENUMERATION = 9;
    /** C {@code XML_XSD10_ATTRIBUTE_NOTATION} (xml_xsd10_types.h). */
    int XML_XSD10_ATTRIBUTE_NOTATION = 10;
    /** C {@code XML_XSD10_SCHEMAS_NAMESPACE_NAME} (xml_xsd10_types.h). */
    String XML_XSD10_SCHEMAS_NAMESPACE_NAME = "http://www.w3.org/2001/XMLSchema";
    /** C {@code XML_XSD10_SCHEMAS_ANYATTR_SKIP} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMAS_ANYATTR_SKIP = 1;
    /** C {@code XML_XSD10_SCHEMAS_ANYATTR_LAX} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMAS_ANYATTR_LAX = 2;
    /** C {@code XML_XSD10_SCHEMAS_ANYATTR_STRICT} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMAS_ANYATTR_STRICT = 3;
    /** C {@code XML_XSD10_SCHEMAS_ANY_SKIP} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMAS_ANY_SKIP = 1;
    /** C {@code XML_XSD10_SCHEMAS_ANY_LAX} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMAS_ANY_LAX = 2;
    /** C {@code XML_XSD10_SCHEMAS_ANY_STRICT} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMAS_ANY_STRICT = 3;
    /** C {@code XML_XSD10_SCHEMAS_ATTR_USE_PROHIBITED} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMAS_ATTR_USE_PROHIBITED = 0;
    /** C {@code XML_XSD10_SCHEMAS_ATTR_USE_REQUIRED} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMAS_ATTR_USE_REQUIRED = 1;
    /** C {@code XML_XSD10_SCHEMAS_ATTR_USE_OPTIONAL} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMAS_ATTR_USE_OPTIONAL = 2;
    /** C {@code XML_XSD10_SCHEMAS_ATTR_GLOBAL} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMAS_ATTR_GLOBAL = (1 << 0);
    /** C {@code XML_XSD10_SCHEMAS_ATTR_NSDEFAULT} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMAS_ATTR_NSDEFAULT = (1 << 7);
    /** C {@code XML_XSD10_SCHEMAS_ATTR_INTERNAL_RESOLVED} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMAS_ATTR_INTERNAL_RESOLVED = (1 << 8);
    /** C {@code XML_XSD10_SCHEMAS_ATTR_FIXED} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMAS_ATTR_FIXED = (1 << 9);
    /** C {@code XML_XSD10_SCHEMAS_WILDCARD_COMPLETE} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMAS_WILDCARD_COMPLETE = (1 << 0);
    /** C {@code XML_XSD10_SCHEMAS_ATTRGROUP_WILDCARD_BUILDED} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMAS_ATTRGROUP_WILDCARD_BUILDED = (1 << 0);
    /** C {@code XML_XSD10_SCHEMAS_ATTRGROUP_GLOBAL} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMAS_ATTRGROUP_GLOBAL = (1 << 1);
    /** C {@code XML_XSD10_SCHEMAS_ATTRGROUP_MARKED} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMAS_ATTRGROUP_MARKED = (1 << 2);
    /** C {@code XML_XSD10_SCHEMAS_ATTRGROUP_REDEFINED} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMAS_ATTRGROUP_REDEFINED = (1 << 3);
    /** C {@code XML_XSD10_SCHEMAS_ATTRGROUP_HAS_REFS} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMAS_ATTRGROUP_HAS_REFS = (1 << 4);
    /** C {@code XML_XSD10_SCHEMAS_TYPE_MIXED} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMAS_TYPE_MIXED = (1 << 0);
    /** C {@code XML_XSD10_SCHEMAS_TYPE_DERIVATION_METHOD_EXTENSION} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMAS_TYPE_DERIVATION_METHOD_EXTENSION = (1 << 1);
    /** C {@code XML_XSD10_SCHEMAS_TYPE_DERIVATION_METHOD_RESTRICTION} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMAS_TYPE_DERIVATION_METHOD_RESTRICTION = (1 << 2);
    /** C {@code XML_XSD10_SCHEMAS_TYPE_GLOBAL} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMAS_TYPE_GLOBAL = (1 << 3);
    /** C {@code XML_XSD10_SCHEMAS_TYPE_OWNED_ATTR_WILDCARD} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMAS_TYPE_OWNED_ATTR_WILDCARD = (1 << 4);
    /** C {@code XML_XSD10_SCHEMAS_TYPE_VARIETY_ABSENT} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMAS_TYPE_VARIETY_ABSENT = (1 << 5);
    /** C {@code XML_XSD10_SCHEMAS_TYPE_VARIETY_LIST} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMAS_TYPE_VARIETY_LIST = (1 << 6);
    /** C {@code XML_XSD10_SCHEMAS_TYPE_VARIETY_UNION} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMAS_TYPE_VARIETY_UNION = (1 << 7);
    /** C {@code XML_XSD10_SCHEMAS_TYPE_VARIETY_ATOMIC} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMAS_TYPE_VARIETY_ATOMIC = (1 << 8);
    /** C {@code XML_XSD10_SCHEMAS_TYPE_FINAL_EXTENSION} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMAS_TYPE_FINAL_EXTENSION = (1 << 9);
    /** C {@code XML_XSD10_SCHEMAS_TYPE_FINAL_RESTRICTION} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMAS_TYPE_FINAL_RESTRICTION = (1 << 10);
    /** C {@code XML_XSD10_SCHEMAS_TYPE_FINAL_LIST} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMAS_TYPE_FINAL_LIST = (1 << 11);
    /** C {@code XML_XSD10_SCHEMAS_TYPE_FINAL_UNION} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMAS_TYPE_FINAL_UNION = (1 << 12);
    /** C {@code XML_XSD10_SCHEMAS_TYPE_FINAL_DEFAULT} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMAS_TYPE_FINAL_DEFAULT = (1 << 13);
    /** C {@code XML_XSD10_SCHEMAS_TYPE_BUILTIN_PRIMITIVE} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMAS_TYPE_BUILTIN_PRIMITIVE = (1 << 14);
    /** C {@code XML_XSD10_SCHEMAS_TYPE_MARKED} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMAS_TYPE_MARKED = (1 << 16);
    /** C {@code XML_XSD10_SCHEMAS_TYPE_BLOCK_DEFAULT} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMAS_TYPE_BLOCK_DEFAULT = (1 << 17);
    /** C {@code XML_XSD10_SCHEMAS_TYPE_BLOCK_EXTENSION} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMAS_TYPE_BLOCK_EXTENSION = (1 << 18);
    /** C {@code XML_XSD10_SCHEMAS_TYPE_BLOCK_RESTRICTION} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMAS_TYPE_BLOCK_RESTRICTION = (1 << 19);
    /** C {@code XML_XSD10_SCHEMAS_TYPE_ABSTRACT} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMAS_TYPE_ABSTRACT = (1 << 20);
    /** C {@code XML_XSD10_SCHEMAS_TYPE_FACETSNEEDVALUE} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMAS_TYPE_FACETSNEEDVALUE = (1 << 21);
    /** C {@code XML_XSD10_SCHEMAS_TYPE_INTERNAL_RESOLVED} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMAS_TYPE_INTERNAL_RESOLVED = (1 << 22);
    /** C {@code XML_XSD10_SCHEMAS_TYPE_INTERNAL_INVALID} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMAS_TYPE_INTERNAL_INVALID = (1 << 23);
    /** C {@code XML_XSD10_SCHEMAS_TYPE_WHITESPACE_PRESERVE} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMAS_TYPE_WHITESPACE_PRESERVE = (1 << 24);
    /** C {@code XML_XSD10_SCHEMAS_TYPE_WHITESPACE_REPLACE} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMAS_TYPE_WHITESPACE_REPLACE = (1 << 25);
    /** C {@code XML_XSD10_SCHEMAS_TYPE_WHITESPACE_COLLAPSE} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMAS_TYPE_WHITESPACE_COLLAPSE = (1 << 26);
    /** C {@code XML_XSD10_SCHEMAS_TYPE_HAS_FACETS} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMAS_TYPE_HAS_FACETS = (1 << 27);
    /** C {@code XML_XSD10_SCHEMAS_TYPE_NORMVALUENEEDED} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMAS_TYPE_NORMVALUENEEDED = (1 << 28);
    /** C {@code XML_XSD10_SCHEMAS_TYPE_FIXUP_1} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMAS_TYPE_FIXUP_1 = (1 << 29);
    /** C {@code XML_XSD10_SCHEMAS_TYPE_REDEFINED} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMAS_TYPE_REDEFINED = (1 << 30);
    /** C {@code XML_XSD10_SCHEMAS_ELEM_NILLABLE} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMAS_ELEM_NILLABLE = (1 << 0);
    /** C {@code XML_XSD10_SCHEMAS_ELEM_GLOBAL} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMAS_ELEM_GLOBAL = (1 << 1);
    /** C {@code XML_XSD10_SCHEMAS_ELEM_DEFAULT} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMAS_ELEM_DEFAULT = (1 << 2);
    /** C {@code XML_XSD10_SCHEMAS_ELEM_FIXED} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMAS_ELEM_FIXED = (1 << 3);
    /** C {@code XML_XSD10_SCHEMAS_ELEM_ABSTRACT} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMAS_ELEM_ABSTRACT = (1 << 4);
    /** C {@code XML_XSD10_SCHEMAS_ELEM_TOPLEVEL} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMAS_ELEM_TOPLEVEL = (1 << 5);
    /** C {@code XML_XSD10_SCHEMAS_ELEM_REF} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMAS_ELEM_REF = (1 << 6);
    /** C {@code XML_XSD10_SCHEMAS_ELEM_NSDEFAULT} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMAS_ELEM_NSDEFAULT = (1 << 7);
    /** C {@code XML_XSD10_SCHEMAS_ELEM_INTERNAL_RESOLVED} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMAS_ELEM_INTERNAL_RESOLVED = (1 << 8);
    /** C {@code XML_XSD10_SCHEMAS_ELEM_CIRCULAR} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMAS_ELEM_CIRCULAR = (1 << 9);
    /** C {@code XML_XSD10_SCHEMAS_ELEM_BLOCK_ABSENT} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMAS_ELEM_BLOCK_ABSENT = (1 << 10);
    /** C {@code XML_XSD10_SCHEMAS_ELEM_BLOCK_EXTENSION} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMAS_ELEM_BLOCK_EXTENSION = (1 << 11);
    /** C {@code XML_XSD10_SCHEMAS_ELEM_BLOCK_RESTRICTION} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMAS_ELEM_BLOCK_RESTRICTION = (1 << 12);
    /** C {@code XML_XSD10_SCHEMAS_ELEM_BLOCK_SUBSTITUTION} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMAS_ELEM_BLOCK_SUBSTITUTION = (1 << 13);
    /** C {@code XML_XSD10_SCHEMAS_ELEM_FINAL_ABSENT} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMAS_ELEM_FINAL_ABSENT = (1 << 14);
    /** C {@code XML_XSD10_SCHEMAS_ELEM_FINAL_EXTENSION} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMAS_ELEM_FINAL_EXTENSION = (1 << 15);
    /** C {@code XML_XSD10_SCHEMAS_ELEM_FINAL_RESTRICTION} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMAS_ELEM_FINAL_RESTRICTION = (1 << 16);
    /** C {@code XML_XSD10_SCHEMAS_ELEM_SUBST_GROUP_HEAD} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMAS_ELEM_SUBST_GROUP_HEAD = (1 << 17);
    /** C {@code XML_XSD10_SCHEMAS_ELEM_INTERNAL_CHECKED} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMAS_ELEM_INTERNAL_CHECKED = (1 << 18);
    /** C {@code XML_XSD10_SCHEMAS_FACET_UNKNOWN} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMAS_FACET_UNKNOWN = 0;
    /** C {@code XML_XSD10_SCHEMAS_FACET_PRESERVE} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMAS_FACET_PRESERVE = 1;
    /** C {@code XML_XSD10_SCHEMAS_FACET_REPLACE} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMAS_FACET_REPLACE = 2;
    /** C {@code XML_XSD10_SCHEMAS_FACET_COLLAPSE} (xml_xsd10_types.h). */
    int XML_XSD10_SCHEMAS_FACET_COLLAPSE = 3;
    /** C {@code XML_XSD10_UNBOUNDED} (xml_xsd10_types.h). */
    int XML_XSD10_UNBOUNDED = (1 << 30);
    /** C {@code XML_XSD10_ERR_NONE} (xml_xsd10_schemas.h). */
    int XML_XSD10_ERR_NONE = 0;
    /** C {@code XML_XSD10_ERR_WARNING} (xml_xsd10_schemas.h). */
    int XML_XSD10_ERR_WARNING = 1;
    /** C {@code XML_XSD10_ERR_ERROR} (xml_xsd10_schemas.h). */
    int XML_XSD10_ERR_ERROR = 2;
    /** C {@code XML_XSD10_ERR_FATAL} (xml_xsd10_schemas.h). */
    int XML_XSD10_ERR_FATAL = 3;
    /** C {@code XML_XSD10_SCHEMA_VAL_VC_I_CREATE} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMA_VAL_VC_I_CREATE = (1<<0);
    /** C {@code XML_XSD10_SCHEMAP_PREFIX_UNDEFINED} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_PREFIX_UNDEFINED = 1700;
    /** C {@code XML_XSD10_SCHEMAP_ATTRFORMDEFAULT_VALUE} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_ATTRFORMDEFAULT_VALUE = 1701;
    /** C {@code XML_XSD10_SCHEMAP_ATTRGRP_NONAME_NOREF} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_ATTRGRP_NONAME_NOREF = 1702;
    /** C {@code XML_XSD10_SCHEMAP_ATTR_NONAME_NOREF} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_ATTR_NONAME_NOREF = 1703;
    /** C {@code XML_XSD10_SCHEMAP_COMPLEXTYPE_NONAME_NOREF} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_COMPLEXTYPE_NONAME_NOREF = 1704;
    /** C {@code XML_XSD10_SCHEMAP_ELEMFORMDEFAULT_VALUE} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_ELEMFORMDEFAULT_VALUE = 1705;
    /** C {@code XML_XSD10_SCHEMAP_ELEM_NONAME_NOREF} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_ELEM_NONAME_NOREF = 1706;
    /** C {@code XML_XSD10_SCHEMAP_EXTENSION_NO_BASE} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_EXTENSION_NO_BASE = 1707;
    /** C {@code XML_XSD10_SCHEMAP_FACET_NO_VALUE} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_FACET_NO_VALUE = 1708;
    /** C {@code XML_XSD10_SCHEMAP_FAILED_BUILD_IMPORT} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_FAILED_BUILD_IMPORT = 1709;
    /** C {@code XML_XSD10_SCHEMAP_GROUP_NONAME_NOREF} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_GROUP_NONAME_NOREF = 1710;
    /** C {@code XML_XSD10_SCHEMAP_IMPORT_NAMESPACE_NOT_URI} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_IMPORT_NAMESPACE_NOT_URI = 1711;
    /** C {@code XML_XSD10_SCHEMAP_IMPORT_REDEFINE_NSNAME} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_IMPORT_REDEFINE_NSNAME = 1712;
    /** C {@code XML_XSD10_SCHEMAP_IMPORT_SCHEMA_NOT_URI} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_IMPORT_SCHEMA_NOT_URI = 1713;
    /** C {@code XML_XSD10_SCHEMAP_INVALID_BOOLEAN} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_INVALID_BOOLEAN = 1714;
    /** C {@code XML_XSD10_SCHEMAP_INVALID_ENUM} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_INVALID_ENUM = 1715;
    /** C {@code XML_XSD10_SCHEMAP_INVALID_FACET} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_INVALID_FACET = 1716;
    /** C {@code XML_XSD10_SCHEMAP_INVALID_MAXOCCURS} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_INVALID_MAXOCCURS = 1718;
    /** C {@code XML_XSD10_SCHEMAP_INVALID_MINOCCURS} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_INVALID_MINOCCURS = 1719;
    /** C {@code XML_XSD10_SCHEMAP_INVALID_REF_AND_SUBTYPE} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_INVALID_REF_AND_SUBTYPE = 1720;
    /** C {@code XML_XSD10_SCHEMAP_INVALID_WHITE_SPACE} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_INVALID_WHITE_SPACE = 1721;
    /** C {@code XML_XSD10_SCHEMAP_NOATTR_NOREF} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_NOATTR_NOREF = 1722;
    /** C {@code XML_XSD10_SCHEMAP_NOTATION_NO_NAME} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_NOTATION_NO_NAME = 1723;
    /** C {@code XML_XSD10_SCHEMAP_NOTYPE_NOREF} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_NOTYPE_NOREF = 1724;
    /** C {@code XML_XSD10_SCHEMAP_REF_AND_SUBTYPE} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_REF_AND_SUBTYPE = 1725;
    /** C {@code XML_XSD10_SCHEMAP_RESTRICTION_NONAME_NOREF} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_RESTRICTION_NONAME_NOREF = 1726;
    /** C {@code XML_XSD10_SCHEMAP_SIMPLETYPE_NONAME} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_SIMPLETYPE_NONAME = 1727;
    /** C {@code XML_XSD10_SCHEMAP_TYPE_AND_SUBTYPE} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_TYPE_AND_SUBTYPE = 1728;
    /** C {@code XML_XSD10_SCHEMAP_UNKNOWN_ALL_CHILD} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_UNKNOWN_ALL_CHILD = 1729;
    /** C {@code XML_XSD10_SCHEMAP_UNKNOWN_ANYATTRIBUTE_CHILD} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_UNKNOWN_ANYATTRIBUTE_CHILD = 1730;
    /** C {@code XML_XSD10_SCHEMAP_UNKNOWN_ATTR_CHILD} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_UNKNOWN_ATTR_CHILD = 1731;
    /** C {@code XML_XSD10_SCHEMAP_UNKNOWN_ATTRGRP_CHILD} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_UNKNOWN_ATTRGRP_CHILD = 1732;
    /** C {@code XML_XSD10_SCHEMAP_UNKNOWN_ATTRIBUTE_GROUP} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_UNKNOWN_ATTRIBUTE_GROUP = 1733;
    /** C {@code XML_XSD10_SCHEMAP_UNKNOWN_BASE_TYPE} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_UNKNOWN_BASE_TYPE = 1734;
    /** C {@code XML_XSD10_SCHEMAP_UNKNOWN_CHOICE_CHILD} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_UNKNOWN_CHOICE_CHILD = 1735;
    /** C {@code XML_XSD10_SCHEMAP_UNKNOWN_COMPLEXCONTENT_CHILD} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_UNKNOWN_COMPLEXCONTENT_CHILD = 1736;
    /** C {@code XML_XSD10_SCHEMAP_UNKNOWN_COMPLEXTYPE_CHILD} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_UNKNOWN_COMPLEXTYPE_CHILD = 1737;
    /** C {@code XML_XSD10_SCHEMAP_UNKNOWN_ELEM_CHILD} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_UNKNOWN_ELEM_CHILD = 1738;
    /** C {@code XML_XSD10_SCHEMAP_UNKNOWN_EXTENSION_CHILD} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_UNKNOWN_EXTENSION_CHILD = 1739;
    /** C {@code XML_XSD10_SCHEMAP_UNKNOWN_FACET_CHILD} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_UNKNOWN_FACET_CHILD = 1740;
    /** C {@code XML_XSD10_SCHEMAP_UNKNOWN_FACET_TYPE} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_UNKNOWN_FACET_TYPE = 1741;
    /** C {@code XML_XSD10_SCHEMAP_UNKNOWN_GROUP_CHILD} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_UNKNOWN_GROUP_CHILD = 1742;
    /** C {@code XML_XSD10_SCHEMAP_UNKNOWN_IMPORT_CHILD} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_UNKNOWN_IMPORT_CHILD = 1743;
    /** C {@code XML_XSD10_SCHEMAP_UNKNOWN_LIST_CHILD} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_UNKNOWN_LIST_CHILD = 1744;
    /** C {@code XML_XSD10_SCHEMAP_UNKNOWN_NOTATION_CHILD} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_UNKNOWN_NOTATION_CHILD = 1745;
    /** C {@code XML_XSD10_SCHEMAP_UNKNOWN_PROCESSCONTENT_CHILD} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_UNKNOWN_PROCESSCONTENT_CHILD = 1746;
    /** C {@code XML_XSD10_SCHEMAP_UNKNOWN_REF} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_UNKNOWN_REF = 1747;
    /** C {@code XML_XSD10_SCHEMAP_UNKNOWN_RESTRICTION_CHILD} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_UNKNOWN_RESTRICTION_CHILD = 1748;
    /** C {@code XML_XSD10_SCHEMAP_UNKNOWN_SCHEMAS_CHILD} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_UNKNOWN_SCHEMAS_CHILD = 1749;
    /** C {@code XML_XSD10_SCHEMAP_UNKNOWN_SEQUENCE_CHILD} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_UNKNOWN_SEQUENCE_CHILD = 1750;
    /** C {@code XML_XSD10_SCHEMAP_UNKNOWN_SIMPLECONTENT_CHILD} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_UNKNOWN_SIMPLECONTENT_CHILD = 1751;
    /** C {@code XML_XSD10_SCHEMAP_UNKNOWN_SIMPLETYPE_CHILD} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_UNKNOWN_SIMPLETYPE_CHILD = 1752;
    /** C {@code XML_XSD10_SCHEMAP_UNKNOWN_TYPE} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_UNKNOWN_TYPE = 1753;
    /** C {@code XML_XSD10_SCHEMAP_UNKNOWN_UNION_CHILD} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_UNKNOWN_UNION_CHILD = 1754;
    /** C {@code XML_XSD10_SCHEMAP_ELEM_DEFAULT_FIXED} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_ELEM_DEFAULT_FIXED = 1755;
    /** C {@code XML_XSD10_SCHEMAP_FAILED_LOAD} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_FAILED_LOAD = 1757;
    /** C {@code XML_XSD10_SCHEMAP_NOTHING_TO_PARSE} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_NOTHING_TO_PARSE = 1758;
    /** C {@code XML_XSD10_SCHEMAP_NOROOT} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_NOROOT = 1759;
    /** C {@code XML_XSD10_SCHEMAP_REDEFINED_GROUP} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_REDEFINED_GROUP = 1760;
    /** C {@code XML_XSD10_SCHEMAP_REDEFINED_TYPE} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_REDEFINED_TYPE = 1761;
    /** C {@code XML_XSD10_SCHEMAP_REDEFINED_ELEMENT} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_REDEFINED_ELEMENT = 1762;
    /** C {@code XML_XSD10_SCHEMAP_REDEFINED_ATTRGROUP} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_REDEFINED_ATTRGROUP = 1763;
    /** C {@code XML_XSD10_SCHEMAP_REDEFINED_ATTR} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_REDEFINED_ATTR = 1764;
    /** C {@code XML_XSD10_SCHEMAP_REDEFINED_NOTATION} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_REDEFINED_NOTATION = 1765;
    /** C {@code XML_XSD10_SCHEMAP_FAILED_PARSE} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_FAILED_PARSE = 1766;
    /** C {@code XML_XSD10_SCHEMAP_UNKNOWN_PREFIX} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_UNKNOWN_PREFIX = 1767;
    /** C {@code XML_XSD10_SCHEMAP_DEF_AND_PREFIX} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_DEF_AND_PREFIX = 1768;
    /** C {@code XML_XSD10_SCHEMAP_UNKNOWN_INCLUDE_CHILD} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_UNKNOWN_INCLUDE_CHILD = 1769;
    /** C {@code XML_XSD10_SCHEMAP_INCLUDE_SCHEMA_NOT_URI} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_INCLUDE_SCHEMA_NOT_URI = 1770;
    /** C {@code XML_XSD10_SCHEMAP_INCLUDE_SCHEMA_NO_URI} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_INCLUDE_SCHEMA_NO_URI = 1771;
    /** C {@code XML_XSD10_SCHEMAP_NOT_SCHEMA} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_NOT_SCHEMA = 1772;
    /** C {@code XML_XSD10_SCHEMAP_UNKNOWN_MEMBER_TYPE} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_UNKNOWN_MEMBER_TYPE = 1773;
    /** C {@code XML_XSD10_SCHEMAP_INVALID_ATTR_USE} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_INVALID_ATTR_USE = 1774;
    /** C {@code XML_XSD10_SCHEMAP_RECURSIVE} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_RECURSIVE = 1775;
    /** C {@code XML_XSD10_SCHEMAP_SUPERNUMEROUS_LIST_ITEM_TYPE} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_SUPERNUMEROUS_LIST_ITEM_TYPE = 1776;
    /** C {@code XML_XSD10_SCHEMAP_INVALID_ATTR_COMBINATION} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_INVALID_ATTR_COMBINATION = 1777;
    /** C {@code XML_XSD10_SCHEMAP_INVALID_ATTR_INLINE_COMBINATION} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_INVALID_ATTR_INLINE_COMBINATION = 1778;
    /** C {@code XML_XSD10_SCHEMAP_MISSING_SIMPLETYPE_CHILD} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_MISSING_SIMPLETYPE_CHILD = 1779;
    /** C {@code XML_XSD10_SCHEMAP_INVALID_ATTR_NAME} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_INVALID_ATTR_NAME = 1780;
    /** C {@code XML_XSD10_SCHEMAP_REF_AND_CONTENT} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_REF_AND_CONTENT = 1781;
    /** C {@code XML_XSD10_SCHEMAP_CT_PROPS_CORRECT_1} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_CT_PROPS_CORRECT_1 = 1782;
    /** C {@code XML_XSD10_SCHEMAP_CT_PROPS_CORRECT_2} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_CT_PROPS_CORRECT_2 = 1783;
    /** C {@code XML_XSD10_SCHEMAP_CT_PROPS_CORRECT_3} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_CT_PROPS_CORRECT_3 = 1784;
    /** C {@code XML_XSD10_SCHEMAP_CT_PROPS_CORRECT_4} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_CT_PROPS_CORRECT_4 = 1785;
    /** C {@code XML_XSD10_SCHEMAP_CT_PROPS_CORRECT_5} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_CT_PROPS_CORRECT_5 = 1786;
    /** C {@code XML_XSD10_SCHEMAP_DERIVATION_OK_RESTRICTION_1} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_DERIVATION_OK_RESTRICTION_1 = 1787;
    /** C {@code XML_XSD10_SCHEMAP_DERIVATION_OK_RESTRICTION_2_1_1} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_DERIVATION_OK_RESTRICTION_2_1_1 = 1788;
    /** C {@code XML_XSD10_SCHEMAP_DERIVATION_OK_RESTRICTION_2_1_2} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_DERIVATION_OK_RESTRICTION_2_1_2 = 1789;
    /** C {@code XML_XSD10_SCHEMAP_DERIVATION_OK_RESTRICTION_2_2} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_DERIVATION_OK_RESTRICTION_2_2 = 1790;
    /** C {@code XML_XSD10_SCHEMAP_DERIVATION_OK_RESTRICTION_3} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_DERIVATION_OK_RESTRICTION_3 = 1791;
    /** C {@code XML_XSD10_SCHEMAP_WILDCARD_INVALID_NS_MEMBER} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_WILDCARD_INVALID_NS_MEMBER = 1792;
    /** C {@code XML_XSD10_SCHEMAP_INTERSECTION_NOT_EXPRESSIBLE} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_INTERSECTION_NOT_EXPRESSIBLE = 1793;
    /** C {@code XML_XSD10_SCHEMAP_UNION_NOT_EXPRESSIBLE} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_UNION_NOT_EXPRESSIBLE = 1794;
    /** C {@code XML_XSD10_SCHEMAP_SRC_IMPORT_3_1} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_SRC_IMPORT_3_1 = 1795;
    /** C {@code XML_XSD10_SCHEMAP_SRC_IMPORT_3_2} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_SRC_IMPORT_3_2 = 1796;
    /** C {@code XML_XSD10_SCHEMAP_DERIVATION_OK_RESTRICTION_4_1} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_DERIVATION_OK_RESTRICTION_4_1 = 1797;
    /** C {@code XML_XSD10_SCHEMAP_DERIVATION_OK_RESTRICTION_4_2} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_DERIVATION_OK_RESTRICTION_4_2 = 1798;
    /** C {@code XML_XSD10_SCHEMAP_DERIVATION_OK_RESTRICTION_4_3} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_DERIVATION_OK_RESTRICTION_4_3 = 1799;
    /** C {@code XML_XSD10_SCHEMAP_COS_CT_EXTENDS_1_3} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_COS_CT_EXTENDS_1_3 = 1800;
    /** C {@code XML_XSD10_SCHEMAV_NOROOT} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAV_NOROOT = 1801;
    /** C {@code XML_XSD10_SCHEMAV_UNDECLAREDELEM} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAV_UNDECLAREDELEM = 1802;
    /** C {@code XML_XSD10_SCHEMAV_NOTTOPLEVEL} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAV_NOTTOPLEVEL = 1803;
    /** C {@code XML_XSD10_SCHEMAV_MISSING} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAV_MISSING = 1804;
    /** C {@code XML_XSD10_SCHEMAV_WRONGELEM} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAV_WRONGELEM = 1805;
    /** C {@code XML_XSD10_SCHEMAV_NOTYPE} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAV_NOTYPE = 1806;
    /** C {@code XML_XSD10_SCHEMAV_NOROLLBACK} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAV_NOROLLBACK = 1807;
    /** C {@code XML_XSD10_SCHEMAV_ISABSTRACT} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAV_ISABSTRACT = 1808;
    /** C {@code XML_XSD10_SCHEMAV_NOTEMPTY} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAV_NOTEMPTY = 1809;
    /** C {@code XML_XSD10_SCHEMAV_ELEMCONT} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAV_ELEMCONT = 1810;
    /** C {@code XML_XSD10_SCHEMAV_HAVEDEFAULT} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAV_HAVEDEFAULT = 1811;
    /** C {@code XML_XSD10_SCHEMAV_NOTNILLABLE} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAV_NOTNILLABLE = 1812;
    /** C {@code XML_XSD10_SCHEMAV_EXTRACONTENT} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAV_EXTRACONTENT = 1813;
    /** C {@code XML_XSD10_SCHEMAV_INVALIDATTR} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAV_INVALIDATTR = 1814;
    /** C {@code XML_XSD10_SCHEMAV_INVALIDELEM} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAV_INVALIDELEM = 1815;
    /** C {@code XML_XSD10_SCHEMAV_NOTDETERMINIST} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAV_NOTDETERMINIST = 1816;
    /** C {@code XML_XSD10_SCHEMAV_CONSTRUCT} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAV_CONSTRUCT = 1817;
    /** C {@code XML_XSD10_SCHEMAV_INTERNAL} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAV_INTERNAL = 1818;
    /** C {@code XML_XSD10_SCHEMAV_NOTSIMPLE} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAV_NOTSIMPLE = 1819;
    /** C {@code XML_XSD10_SCHEMAV_ATTRUNKNOWN} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAV_ATTRUNKNOWN = 1820;
    /** C {@code XML_XSD10_SCHEMAV_ATTRINVALID} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAV_ATTRINVALID = 1821;
    /** C {@code XML_XSD10_SCHEMAV_VALUE} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAV_VALUE = 1822;
    /** C {@code XML_XSD10_SCHEMAV_FACET} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAV_FACET = 1823;
    /** C {@code XML_XSD10_SCHEMAV_CVC_DATATYPE_VALID_1_2_3} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAV_CVC_DATATYPE_VALID_1_2_3 = 1826;
    /** C {@code XML_XSD10_SCHEMAV_CVC_TYPE_3_1_1} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAV_CVC_TYPE_3_1_1 = 1827;
    /** C {@code XML_XSD10_SCHEMAV_CVC_TYPE_3_1_2} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAV_CVC_TYPE_3_1_2 = 1828;
    /** C {@code XML_XSD10_SCHEMAV_CVC_FACET_VALID} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAV_CVC_FACET_VALID = 1829;
    /** C {@code XML_XSD10_SCHEMAV_CVC_COMPLEX_TYPE_2_1} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAV_CVC_COMPLEX_TYPE_2_1 = 1841;
    /** C {@code XML_XSD10_SCHEMAV_CVC_COMPLEX_TYPE_2_2} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAV_CVC_COMPLEX_TYPE_2_2 = 1842;
    /** C {@code XML_XSD10_SCHEMAV_CVC_COMPLEX_TYPE_2_3} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAV_CVC_COMPLEX_TYPE_2_3 = 1843;
    /** C {@code XML_XSD10_SCHEMAV_CVC_COMPLEX_TYPE_2_4} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAV_CVC_COMPLEX_TYPE_2_4 = 1844;
    /** C {@code XML_XSD10_SCHEMAV_CVC_ELT_1} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAV_CVC_ELT_1 = 1845;
    /** C {@code XML_XSD10_SCHEMAV_CVC_ELT_2} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAV_CVC_ELT_2 = 1846;
    /** C {@code XML_XSD10_SCHEMAV_CVC_ELT_3_1} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAV_CVC_ELT_3_1 = 1847;
    /** C {@code XML_XSD10_SCHEMAV_CVC_ELT_3_2_1} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAV_CVC_ELT_3_2_1 = 1848;
    /** C {@code XML_XSD10_SCHEMAV_CVC_ELT_3_2_2} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAV_CVC_ELT_3_2_2 = 1849;
    /** C {@code XML_XSD10_SCHEMAV_CVC_ELT_4_1} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAV_CVC_ELT_4_1 = 1850;
    /** C {@code XML_XSD10_SCHEMAV_CVC_ELT_4_2} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAV_CVC_ELT_4_2 = 1851;
    /** C {@code XML_XSD10_SCHEMAV_CVC_ELT_4_3} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAV_CVC_ELT_4_3 = 1852;
    /** C {@code XML_XSD10_SCHEMAV_CVC_ELT_5_1_1} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAV_CVC_ELT_5_1_1 = 1853;
    /** C {@code XML_XSD10_SCHEMAV_CVC_ELT_5_1_2} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAV_CVC_ELT_5_1_2 = 1854;
    /** C {@code XML_XSD10_SCHEMAV_CVC_ELT_5_2_1} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAV_CVC_ELT_5_2_1 = 1855;
    /** C {@code XML_XSD10_SCHEMAV_CVC_ELT_5_2_2_1} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAV_CVC_ELT_5_2_2_1 = 1856;
    /** C {@code XML_XSD10_SCHEMAV_CVC_ELT_5_2_2_2_1} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAV_CVC_ELT_5_2_2_2_1 = 1857;
    /** C {@code XML_XSD10_SCHEMAV_CVC_ELT_5_2_2_2_2} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAV_CVC_ELT_5_2_2_2_2 = 1858;
    /** C {@code XML_XSD10_SCHEMAV_CVC_ELT_6} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAV_CVC_ELT_6 = 1859;
    /** C {@code XML_XSD10_SCHEMAV_CVC_ELT_7} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAV_CVC_ELT_7 = 1860;
    /** C {@code XML_XSD10_SCHEMAV_CVC_ATTRIBUTE_1} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAV_CVC_ATTRIBUTE_1 = 1861;
    /** C {@code XML_XSD10_SCHEMAV_CVC_ATTRIBUTE_2} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAV_CVC_ATTRIBUTE_2 = 1862;
    /** C {@code XML_XSD10_SCHEMAV_CVC_ATTRIBUTE_3} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAV_CVC_ATTRIBUTE_3 = 1863;
    /** C {@code XML_XSD10_SCHEMAV_CVC_ATTRIBUTE_4} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAV_CVC_ATTRIBUTE_4 = 1864;
    /** C {@code XML_XSD10_SCHEMAV_CVC_COMPLEX_TYPE_3_1} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAV_CVC_COMPLEX_TYPE_3_1 = 1865;
    /** C {@code XML_XSD10_SCHEMAV_CVC_COMPLEX_TYPE_3_2_1} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAV_CVC_COMPLEX_TYPE_3_2_1 = 1866;
    /** C {@code XML_XSD10_SCHEMAV_CVC_COMPLEX_TYPE_3_2_2} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAV_CVC_COMPLEX_TYPE_3_2_2 = 1867;
    /** C {@code XML_XSD10_SCHEMAV_CVC_COMPLEX_TYPE_4} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAV_CVC_COMPLEX_TYPE_4 = 1868;
    /** C {@code XML_XSD10_SCHEMAV_CVC_COMPLEX_TYPE_5_1} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAV_CVC_COMPLEX_TYPE_5_1 = 1869;
    /** C {@code XML_XSD10_SCHEMAV_CVC_COMPLEX_TYPE_5_2} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAV_CVC_COMPLEX_TYPE_5_2 = 1870;
    /** C {@code XML_XSD10_SCHEMAV_ELEMENT_CONTENT} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAV_ELEMENT_CONTENT = 1871;
    /** C {@code XML_XSD10_SCHEMAV_DOCUMENT_ELEMENT_MISSING} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAV_DOCUMENT_ELEMENT_MISSING = 1872;
    /** C {@code XML_XSD10_SCHEMAV_CVC_COMPLEX_TYPE_1} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAV_CVC_COMPLEX_TYPE_1 = 1873;
    /** C {@code XML_XSD10_SCHEMAV_CVC_AU} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAV_CVC_AU = 1874;
    /** C {@code XML_XSD10_SCHEMAV_CVC_TYPE_1} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAV_CVC_TYPE_1 = 1875;
    /** C {@code XML_XSD10_SCHEMAV_CVC_TYPE_2} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAV_CVC_TYPE_2 = 1876;
    /** C {@code XML_XSD10_SCHEMAV_CVC_IDC} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAV_CVC_IDC = 1877;
    /** C {@code XML_XSD10_SCHEMAV_CVC_WILDCARD} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAV_CVC_WILDCARD = 1878;
    /** C {@code XML_XSD10_SCHEMAV_MISC} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAV_MISC = 1879;
    /** C {@code XML_XSD10_SCHEMAP_SRC_SIMPLE_TYPE_1} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_SRC_SIMPLE_TYPE_1 = 3000;
    /** C {@code XML_XSD10_SCHEMAP_SRC_SIMPLE_TYPE_2} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_SRC_SIMPLE_TYPE_2 = 3001;
    /** C {@code XML_XSD10_SCHEMAP_SRC_SIMPLE_TYPE_3} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_SRC_SIMPLE_TYPE_3 = 3002;
    /** C {@code XML_XSD10_SCHEMAP_SRC_SIMPLE_TYPE_4} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_SRC_SIMPLE_TYPE_4 = 3003;
    /** C {@code XML_XSD10_SCHEMAP_SRC_RESOLVE} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_SRC_RESOLVE = 3004;
    /** C {@code XML_XSD10_SCHEMAP_SRC_RESTRICTION_BASE_OR_SIMPLETYPE} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_SRC_RESTRICTION_BASE_OR_SIMPLETYPE = 3005;
    /** C {@code XML_XSD10_SCHEMAP_SRC_LIST_ITEMTYPE_OR_SIMPLETYPE} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_SRC_LIST_ITEMTYPE_OR_SIMPLETYPE = 3006;
    /** C {@code XML_XSD10_SCHEMAP_SRC_UNION_MEMBERTYPES_OR_SIMPLETYPES} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_SRC_UNION_MEMBERTYPES_OR_SIMPLETYPES = 3007;
    /** C {@code XML_XSD10_SCHEMAP_ST_PROPS_CORRECT_1} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_ST_PROPS_CORRECT_1 = 3008;
    /** C {@code XML_XSD10_SCHEMAP_ST_PROPS_CORRECT_2} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_ST_PROPS_CORRECT_2 = 3009;
    /** C {@code XML_XSD10_SCHEMAP_ST_PROPS_CORRECT_3} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_ST_PROPS_CORRECT_3 = 3010;
    /** C {@code XML_XSD10_SCHEMAP_COS_ST_RESTRICTS_1_1} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_COS_ST_RESTRICTS_1_1 = 3011;
    /** C {@code XML_XSD10_SCHEMAP_COS_ST_RESTRICTS_1_2} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_COS_ST_RESTRICTS_1_2 = 3012;
    /** C {@code XML_XSD10_SCHEMAP_COS_ST_RESTRICTS_1_3_1} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_COS_ST_RESTRICTS_1_3_1 = 3013;
    /** C {@code XML_XSD10_SCHEMAP_COS_ST_RESTRICTS_1_3_2} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_COS_ST_RESTRICTS_1_3_2 = 3014;
    /** C {@code XML_XSD10_SCHEMAP_COS_ST_RESTRICTS_2_1} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_COS_ST_RESTRICTS_2_1 = 3015;
    /** C {@code XML_XSD10_SCHEMAP_COS_ST_RESTRICTS_2_3_1_1} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_COS_ST_RESTRICTS_2_3_1_1 = 3016;
    /** C {@code XML_XSD10_SCHEMAP_COS_ST_RESTRICTS_2_3_1_2} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_COS_ST_RESTRICTS_2_3_1_2 = 3017;
    /** C {@code XML_XSD10_SCHEMAP_COS_ST_RESTRICTS_2_3_2_1} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_COS_ST_RESTRICTS_2_3_2_1 = 3018;
    /** C {@code XML_XSD10_SCHEMAP_COS_ST_RESTRICTS_2_3_2_2} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_COS_ST_RESTRICTS_2_3_2_2 = 3019;
    /** C {@code XML_XSD10_SCHEMAP_COS_ST_RESTRICTS_2_3_2_3} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_COS_ST_RESTRICTS_2_3_2_3 = 3020;
    /** C {@code XML_XSD10_SCHEMAP_COS_ST_RESTRICTS_2_3_2_4} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_COS_ST_RESTRICTS_2_3_2_4 = 3021;
    /** C {@code XML_XSD10_SCHEMAP_COS_ST_RESTRICTS_2_3_2_5} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_COS_ST_RESTRICTS_2_3_2_5 = 3022;
    /** C {@code XML_XSD10_SCHEMAP_COS_ST_RESTRICTS_3_1} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_COS_ST_RESTRICTS_3_1 = 3023;
    /** C {@code XML_XSD10_SCHEMAP_COS_ST_RESTRICTS_3_3_1} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_COS_ST_RESTRICTS_3_3_1 = 3024;
    /** C {@code XML_XSD10_SCHEMAP_COS_ST_RESTRICTS_3_3_1_2} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_COS_ST_RESTRICTS_3_3_1_2 = 3025;
    /** C {@code XML_XSD10_SCHEMAP_COS_ST_RESTRICTS_3_3_2_2} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_COS_ST_RESTRICTS_3_3_2_2 = 3026;
    /** C {@code XML_XSD10_SCHEMAP_COS_ST_RESTRICTS_3_3_2_1} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_COS_ST_RESTRICTS_3_3_2_1 = 3027;
    /** C {@code XML_XSD10_SCHEMAP_COS_ST_RESTRICTS_3_3_2_3} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_COS_ST_RESTRICTS_3_3_2_3 = 3028;
    /** C {@code XML_XSD10_SCHEMAP_COS_ST_RESTRICTS_3_3_2_4} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_COS_ST_RESTRICTS_3_3_2_4 = 3029;
    /** C {@code XML_XSD10_SCHEMAP_COS_ST_RESTRICTS_3_3_2_5} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_COS_ST_RESTRICTS_3_3_2_5 = 3030;
    /** C {@code XML_XSD10_SCHEMAP_COS_ST_DERIVED_OK_2_1} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_COS_ST_DERIVED_OK_2_1 = 3031;
    /** C {@code XML_XSD10_SCHEMAP_COS_ST_DERIVED_OK_2_2} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_COS_ST_DERIVED_OK_2_2 = 3032;
    /** C {@code XML_XSD10_SCHEMAP_S4S_ELEM_NOT_ALLOWED} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_S4S_ELEM_NOT_ALLOWED = 3033;
    /** C {@code XML_XSD10_SCHEMAP_S4S_ELEM_MISSING} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_S4S_ELEM_MISSING = 3034;
    /** C {@code XML_XSD10_SCHEMAP_S4S_ATTR_NOT_ALLOWED} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_S4S_ATTR_NOT_ALLOWED = 3035;
    /** C {@code XML_XSD10_SCHEMAP_S4S_ATTR_MISSING} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_S4S_ATTR_MISSING = 3036;
    /** C {@code XML_XSD10_SCHEMAP_S4S_ATTR_INVALID_VALUE} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_S4S_ATTR_INVALID_VALUE = 3037;
    /** C {@code XML_XSD10_SCHEMAP_SRC_ELEMENT_1} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_SRC_ELEMENT_1 = 3038;
    /** C {@code XML_XSD10_SCHEMAP_SRC_ELEMENT_2_1} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_SRC_ELEMENT_2_1 = 3039;
    /** C {@code XML_XSD10_SCHEMAP_SRC_ELEMENT_2_2} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_SRC_ELEMENT_2_2 = 3040;
    /** C {@code XML_XSD10_SCHEMAP_SRC_ELEMENT_3} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_SRC_ELEMENT_3 = 3041;
    /** C {@code XML_XSD10_SCHEMAP_P_PROPS_CORRECT_1} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_P_PROPS_CORRECT_1 = 3042;
    /** C {@code XML_XSD10_SCHEMAP_P_PROPS_CORRECT_2_1} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_P_PROPS_CORRECT_2_1 = 3043;
    /** C {@code XML_XSD10_SCHEMAP_P_PROPS_CORRECT_2_2} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_P_PROPS_CORRECT_2_2 = 3044;
    /** C {@code XML_XSD10_SCHEMAP_E_PROPS_CORRECT_2} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_E_PROPS_CORRECT_2 = 3045;
    /** C {@code XML_XSD10_SCHEMAP_E_PROPS_CORRECT_3} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_E_PROPS_CORRECT_3 = 3046;
    /** C {@code XML_XSD10_SCHEMAP_E_PROPS_CORRECT_4} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_E_PROPS_CORRECT_4 = 3047;
    /** C {@code XML_XSD10_SCHEMAP_E_PROPS_CORRECT_5} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_E_PROPS_CORRECT_5 = 3048;
    /** C {@code XML_XSD10_SCHEMAP_E_PROPS_CORRECT_6} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_E_PROPS_CORRECT_6 = 3049;
    /** C {@code XML_XSD10_SCHEMAP_SRC_INCLUDE} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_SRC_INCLUDE = 3050;
    /** C {@code XML_XSD10_SCHEMAP_SRC_ATTRIBUTE_1} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_SRC_ATTRIBUTE_1 = 3051;
    /** C {@code XML_XSD10_SCHEMAP_SRC_ATTRIBUTE_2} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_SRC_ATTRIBUTE_2 = 3052;
    /** C {@code XML_XSD10_SCHEMAP_SRC_ATTRIBUTE_3_1} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_SRC_ATTRIBUTE_3_1 = 3053;
    /** C {@code XML_XSD10_SCHEMAP_SRC_ATTRIBUTE_3_2} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_SRC_ATTRIBUTE_3_2 = 3054;
    /** C {@code XML_XSD10_SCHEMAP_SRC_ATTRIBUTE_4} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_SRC_ATTRIBUTE_4 = 3055;
    /** C {@code XML_XSD10_SCHEMAP_NO_XMLNS} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_NO_XMLNS = 3056;
    /** C {@code XML_XSD10_SCHEMAP_NO_XSI} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_NO_XSI = 3057;
    /** C {@code XML_XSD10_SCHEMAP_COS_VALID_DEFAULT_1} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_COS_VALID_DEFAULT_1 = 3058;
    /** C {@code XML_XSD10_SCHEMAP_COS_VALID_DEFAULT_2_1} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_COS_VALID_DEFAULT_2_1 = 3059;
    /** C {@code XML_XSD10_SCHEMAP_COS_VALID_DEFAULT_2_2_1} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_COS_VALID_DEFAULT_2_2_1 = 3060;
    /** C {@code XML_XSD10_SCHEMAP_COS_VALID_DEFAULT_2_2_2} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_COS_VALID_DEFAULT_2_2_2 = 3061;
    /** C {@code XML_XSD10_SCHEMAP_CVC_SIMPLE_TYPE} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_CVC_SIMPLE_TYPE = 3062;
    /** C {@code XML_XSD10_SCHEMAP_COS_CT_EXTENDS_1_1} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_COS_CT_EXTENDS_1_1 = 3063;
    /** C {@code XML_XSD10_SCHEMAP_SRC_IMPORT_1_1} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_SRC_IMPORT_1_1 = 3064;
    /** C {@code XML_XSD10_SCHEMAP_SRC_IMPORT_1_2} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_SRC_IMPORT_1_2 = 3065;
    /** C {@code XML_XSD10_SCHEMAP_SRC_IMPORT_2} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_SRC_IMPORT_2 = 3066;
    /** C {@code XML_XSD10_SCHEMAP_SRC_IMPORT_2_1} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_SRC_IMPORT_2_1 = 3067;
    /** C {@code XML_XSD10_SCHEMAP_SRC_IMPORT_2_2} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_SRC_IMPORT_2_2 = 3068;
    /** C {@code XML_XSD10_SCHEMAP_INTERNAL} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_INTERNAL = 3069;
    /** C {@code XML_XSD10_SCHEMAP_NOT_DETERMINISTIC} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_NOT_DETERMINISTIC = 3070;
    /** C {@code XML_XSD10_SCHEMAP_SRC_ATTRIBUTE_GROUP_1} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_SRC_ATTRIBUTE_GROUP_1 = 3071;
    /** C {@code XML_XSD10_SCHEMAP_SRC_ATTRIBUTE_GROUP_2} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_SRC_ATTRIBUTE_GROUP_2 = 3072;
    /** C {@code XML_XSD10_SCHEMAP_SRC_ATTRIBUTE_GROUP_3} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_SRC_ATTRIBUTE_GROUP_3 = 3073;
    /** C {@code XML_XSD10_SCHEMAP_MG_PROPS_CORRECT_1} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_MG_PROPS_CORRECT_1 = 3074;
    /** C {@code XML_XSD10_SCHEMAP_MG_PROPS_CORRECT_2} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_MG_PROPS_CORRECT_2 = 3075;
    /** C {@code XML_XSD10_SCHEMAP_SRC_CT_1} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_SRC_CT_1 = 3076;
    /** C {@code XML_XSD10_SCHEMAP_DERIVATION_OK_RESTRICTION_2_1_3} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_DERIVATION_OK_RESTRICTION_2_1_3 = 3077;
    /** C {@code XML_XSD10_SCHEMAP_AU_PROPS_CORRECT_2} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_AU_PROPS_CORRECT_2 = 3078;
    /** C {@code XML_XSD10_SCHEMAP_A_PROPS_CORRECT_2} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_A_PROPS_CORRECT_2 = 3079;
    /** C {@code XML_XSD10_SCHEMAP_C_PROPS_CORRECT} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_C_PROPS_CORRECT = 3080;
    /** C {@code XML_XSD10_SCHEMAP_SRC_REDEFINE} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_SRC_REDEFINE = 3081;
    /** C {@code XML_XSD10_SCHEMAP_SRC_IMPORT} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_SRC_IMPORT = 3082;
    /** C {@code XML_XSD10_SCHEMAP_WARN_SKIP_SCHEMA} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_WARN_SKIP_SCHEMA = 3083;
    /** C {@code XML_XSD10_SCHEMAP_WARN_UNLOCATED_SCHEMA} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_WARN_UNLOCATED_SCHEMA = 3084;
    /** C {@code XML_XSD10_SCHEMAP_WARN_ATTR_REDECL_PROH} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_WARN_ATTR_REDECL_PROH = 3085;
    /** C {@code XML_XSD10_SCHEMAP_WARN_ATTR_POINTLESS_PROH} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_WARN_ATTR_POINTLESS_PROH = 3086;
    /** C {@code XML_XSD10_SCHEMAP_AG_PROPS_CORRECT} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_AG_PROPS_CORRECT = 3087;
    /** C {@code XML_XSD10_SCHEMAP_COS_CT_EXTENDS_1_2} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_COS_CT_EXTENDS_1_2 = 3088;
    /** C {@code XML_XSD10_SCHEMAP_AU_PROPS_CORRECT} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_AU_PROPS_CORRECT = 3089;
    /** C {@code XML_XSD10_SCHEMAP_A_PROPS_CORRECT_3} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_A_PROPS_CORRECT_3 = 3090;
    /** C {@code XML_XSD10_SCHEMAP_COS_ALL_LIMITED} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAP_COS_ALL_LIMITED = 3091;
    /** C {@code XML_XSD10_SCHEMAS_ERR_OK} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAS_ERR_OK = 0;
    /** C {@code XML_XSD10_SCHEMAS_ERR_NOROOT} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAS_ERR_NOROOT = 1;
    /** C {@code XML_XSD10_SCHEMAS_ERR_UNDECLAREDELEM} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAS_ERR_UNDECLAREDELEM = 2;
    /** C {@code XML_XSD10_SCHEMAS_ERR_NOTTOPLEVEL} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAS_ERR_NOTTOPLEVEL = 3;
    /** C {@code XML_XSD10_SCHEMAS_ERR_MISSING} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAS_ERR_MISSING = 4;
    /** C {@code XML_XSD10_SCHEMAS_ERR_WRONGELEM} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAS_ERR_WRONGELEM = 5;
    /** C {@code XML_XSD10_SCHEMAS_ERR_NOTYPE} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAS_ERR_NOTYPE = 6;
    /** C {@code XML_XSD10_SCHEMAS_ERR_NOROLLBACK} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAS_ERR_NOROLLBACK = 7;
    /** C {@code XML_XSD10_SCHEMAS_ERR_ISABSTRACT} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAS_ERR_ISABSTRACT = 8;
    /** C {@code XML_XSD10_SCHEMAS_ERR_NOTEMPTY} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAS_ERR_NOTEMPTY = 9;
    /** C {@code XML_XSD10_SCHEMAS_ERR_ELEMCONT} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAS_ERR_ELEMCONT = 10;
    /** C {@code XML_XSD10_SCHEMAS_ERR_HAVEDEFAULT} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAS_ERR_HAVEDEFAULT = 11;
    /** C {@code XML_XSD10_SCHEMAS_ERR_NOTNILLABLE} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAS_ERR_NOTNILLABLE = 12;
    /** C {@code XML_XSD10_SCHEMAS_ERR_EXTRACONTENT} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAS_ERR_EXTRACONTENT = 13;
    /** C {@code XML_XSD10_SCHEMAS_ERR_INVALIDATTR} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAS_ERR_INVALIDATTR = 14;
    /** C {@code XML_XSD10_SCHEMAS_ERR_INVALIDELEM} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAS_ERR_INVALIDELEM = 15;
    /** C {@code XML_XSD10_SCHEMAS_ERR_NOTDETERMINIST} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAS_ERR_NOTDETERMINIST = 16;
    /** C {@code XML_XSD10_SCHEMAS_ERR_CONSTRUCT} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAS_ERR_CONSTRUCT = 17;
    /** C {@code XML_XSD10_SCHEMAS_ERR_INTERNAL} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAS_ERR_INTERNAL = 18;
    /** C {@code XML_XSD10_SCHEMAS_ERR_NOTSIMPLE} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAS_ERR_NOTSIMPLE = 19;
    /** C {@code XML_XSD10_SCHEMAS_ERR_ATTRUNKNOWN} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAS_ERR_ATTRUNKNOWN = 20;
    /** C {@code XML_XSD10_SCHEMAS_ERR_ATTRINVALID} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAS_ERR_ATTRINVALID = 21;
    /** C {@code XML_XSD10_SCHEMAS_ERR_VALUE} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAS_ERR_VALUE = 22;
    /** C {@code XML_XSD10_SCHEMAS_ERR_FACET} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAS_ERR_FACET = 23;
    /** C {@code XML_XSD10_SCHEMAS_ERR_} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAS_ERR_ = 24;
    /** C {@code XML_XSD10_SCHEMAS_ERR_XXX} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAS_ERR_XXX = 25;
    /** C {@code XML_XSD10_FROM_PARSER} (xml_xsd10_schemas.h). */
    int XML_XSD10_FROM_PARSER = 1;
    /** C {@code XML_XSD10_FROM_MEMORY} (xml_xsd10_schemas.h). */
    int XML_XSD10_FROM_MEMORY = 6;
    /** C {@code XML_XSD10_FROM_IO} (xml_xsd10_schemas.h). */
    int XML_XSD10_FROM_IO = 8;
    /** C {@code XML_XSD10_FROM_SCHEMASP} (xml_xsd10_schemas.h). */
    int XML_XSD10_FROM_SCHEMASP = 16;
    /** C {@code XML_XSD10_FROM_SCHEMASV} (xml_xsd10_schemas.h). */
    int XML_XSD10_FROM_SCHEMASV = 17;
    /** C {@code XML_XSD10_ERR_OK} (xml_xsd10_schemas.h). */
    int XML_XSD10_ERR_OK = 0;
    /** C {@code XML_XSD10_ERR_INTERNAL_ERROR} (xml_xsd10_schemas.h). */
    int XML_XSD10_ERR_INTERNAL_ERROR = 1;
    /** C {@code XML_XSD10_ERR_NO_MEMORY} (xml_xsd10_schemas.h). */
    int XML_XSD10_ERR_NO_MEMORY = 2;
    /** C {@code XML_XSD10_IO_UNKNOWN} (xml_xsd10_schemas.h). */
    int XML_XSD10_IO_UNKNOWN = 1500;
    /** C {@code XML_XSD10_IO_EACCES} (xml_xsd10_schemas.h). */
    int XML_XSD10_IO_EACCES = 1501;
    /** C {@code XML_XSD10_IO_ENOENT} (xml_xsd10_schemas.h). */
    int XML_XSD10_IO_ENOENT = 1524;
    /** C {@code XML_XSD10_IO_LOAD_ERROR} (xml_xsd10_schemas.h). */
    int XML_XSD10_IO_LOAD_ERROR = 1549;
    /** C {@code XML_XSD10_MAX_ITEMS} (xml_xsd10_schemas.h). */
    int XML_XSD10_MAX_ITEMS = 1000000000;
    /** C {@code XML_XSD10_MAX_ERR_MSG_SIZE} (xml_xsd10_schemas.h). */
    int XML_XSD10_MAX_ERR_MSG_SIZE = 64000;
    /** C {@code XML_XSD10_XML_XML_NAMESPACE} (xml_xsd10_schemas.h). */
    String XML_XSD10_XML_XML_NAMESPACE = "http://www.w3.org/XML/1998/namespace";
    /** C {@code XML_XSD10_SCHEMA_VAL_XSI_ASSEMBLE} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMA_VAL_XSI_ASSEMBLE = (1<<1);
    /** C {@code XML_XSD10_SCHEMAS_QUALIF_ELEM} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAS_QUALIF_ELEM = (1 << 0);
    /** C {@code XML_XSD10_SCHEMAS_QUALIF_ATTR} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAS_QUALIF_ATTR = (1 << 1);
    /** C {@code XML_XSD10_SCHEMAS_FINAL_DEFAULT_EXTENSION} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAS_FINAL_DEFAULT_EXTENSION = (1 << 2);
    /** C {@code XML_XSD10_SCHEMAS_FINAL_DEFAULT_RESTRICTION} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAS_FINAL_DEFAULT_RESTRICTION = (1 << 3);
    /** C {@code XML_XSD10_SCHEMAS_FINAL_DEFAULT_LIST} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAS_FINAL_DEFAULT_LIST = (1 << 4);
    /** C {@code XML_XSD10_SCHEMAS_FINAL_DEFAULT_UNION} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAS_FINAL_DEFAULT_UNION = (1 << 5);
    /** C {@code XML_XSD10_SCHEMAS_BLOCK_DEFAULT_EXTENSION} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAS_BLOCK_DEFAULT_EXTENSION = (1 << 6);
    /** C {@code XML_XSD10_SCHEMAS_BLOCK_DEFAULT_RESTRICTION} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAS_BLOCK_DEFAULT_RESTRICTION = (1 << 7);
    /** C {@code XML_XSD10_SCHEMAS_BLOCK_DEFAULT_SUBSTITUTION} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAS_BLOCK_DEFAULT_SUBSTITUTION = (1 << 8);
    /** C {@code XML_XSD10_SCHEMAS_INCLUDING_CONVERT_NS} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAS_INCLUDING_CONVERT_NS = (1 << 9);
    /** C {@code XML_XSD10_SCHEMAS_TYPE_REDEFINING} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAS_TYPE_REDEFINING = (1 << 31);
    /** C {@code XML_XSD10_SCHEMAS_NO_NAMESPACE} (xml_xsd10_schemas.h). */
    String XML_XSD10_SCHEMAS_NO_NAMESPACE = "##";
    /** C {@code XSD10_SUBSET_RESTRICTION} (xml_xsd10_schemas.h). */
    int XSD10_SUBSET_RESTRICTION = (1<<0);
    /** C {@code XSD10_SUBSET_EXTENSION} (xml_xsd10_schemas.h). */
    int XSD10_SUBSET_EXTENSION = (1<<1);
    /** C {@code XSD10_SUBSET_SUBSTITUTION} (xml_xsd10_schemas.h). */
    int XSD10_SUBSET_SUBSTITUTION = (1<<2);
    /** C {@code XSD10_SUBSET_LIST} (xml_xsd10_schemas.h). */
    int XSD10_SUBSET_LIST = (1<<3);
    /** C {@code XSD10_SUBSET_UNION} (xml_xsd10_schemas.h). */
    int XSD10_SUBSET_UNION = (1<<4);
    /** C {@code XSD10_WXS_ERROR_TYPE_ERROR} (xml_xsd10_schemas.h). */
    int XSD10_WXS_ERROR_TYPE_ERROR = 1;
    /** C {@code XSD10_WXS_ERROR_TYPE_WARNING} (xml_xsd10_schemas.h). */
    int XSD10_WXS_ERROR_TYPE_WARNING = 2;
    /** C {@code XML_XSD10_SCHEMA_CTXT_PARSER} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMA_CTXT_PARSER = 1;
    /** C {@code XML_XSD10_SCHEMA_CTXT_VALIDATOR} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMA_CTXT_VALIDATOR = 2;
    /** C {@code XML_XSD10_SCHEMA_SCHEMA_MAIN} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMA_SCHEMA_MAIN = 0;
    /** C {@code XML_XSD10_SCHEMA_SCHEMA_IMPORT} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMA_SCHEMA_IMPORT = 1;
    /** C {@code XML_XSD10_SCHEMA_SCHEMA_INCLUDE} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMA_SCHEMA_INCLUDE = 2;
    /** C {@code XML_XSD10_SCHEMA_SCHEMA_REDEFINE} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMA_SCHEMA_REDEFINE = 3;
    /** C {@code XML_XSD10_SCHEMA_BUCKET_MARKED} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMA_BUCKET_MARKED = (1<<0);
    /** C {@code XML_XSD10_SCHEMA_BUCKET_COMPS_ADDED} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMA_BUCKET_COMPS_ADDED = (1<<1);
    /** C {@code XML_XSD10_SCHEMA_ATTR_USE_FIXED} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMA_ATTR_USE_FIXED = (1<<0);
    /** C {@code XML_XSD10_SCHEMAS_PARSE_ERROR} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAS_PARSE_ERROR = 1;
    /** C {@code XML_XSD10_SCHEMA_MODEL_GROUP_DEF_MARKED} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMA_MODEL_GROUP_DEF_MARKED = (1<<0);
    /** C {@code XML_XSD10_SCHEMA_MODEL_GROUP_DEF_REDEFINED} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMA_MODEL_GROUP_DEF_REDEFINED = (1<<1);
    /** C {@code XSD10_XPATH_STATE_OBJ_TYPE_IDC_SELECTOR} (xml_xsd10_schemas.h). */
    int XSD10_XPATH_STATE_OBJ_TYPE_IDC_SELECTOR = 1;
    /** C {@code XSD10_XPATH_STATE_OBJ_TYPE_IDC_FIELD} (xml_xsd10_schemas.h). */
    int XSD10_XPATH_STATE_OBJ_TYPE_IDC_FIELD = 2;
    /** C {@code XSD10_XPATH_STATE_OBJ_MATCHES} (xml_xsd10_schemas.h). */
    int XSD10_XPATH_STATE_OBJ_MATCHES = -2;
    /** C {@code XSD10_XPATH_STATE_OBJ_BLOCKED} (xml_xsd10_schemas.h). */
    int XSD10_XPATH_STATE_OBJ_BLOCKED = -3;
    /** C {@code XSD10_IDC_MATCHER} (xml_xsd10_schemas.h). */
    int XSD10_IDC_MATCHER = 0;
    /** C {@code XML_XSD10_SCHEMA_NODE_INFO_FLAG_OWNED_NAMES} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMA_NODE_INFO_FLAG_OWNED_NAMES = (1<<0);
    /** C {@code XML_XSD10_SCHEMA_NODE_INFO_FLAG_OWNED_VALUES} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMA_NODE_INFO_FLAG_OWNED_VALUES = (1<<1);
    /** C {@code XML_XSD10_SCHEMA_ELEM_INFO_NILLED} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMA_ELEM_INFO_NILLED = (1<<2);
    /** C {@code XML_XSD10_SCHEMA_ELEM_INFO_LOCAL_TYPE} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMA_ELEM_INFO_LOCAL_TYPE = (1<<3);
    /** C {@code XML_XSD10_SCHEMA_NODE_INFO_VALUE_NEEDED} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMA_NODE_INFO_VALUE_NEEDED = (1<<4);
    /** C {@code XML_XSD10_SCHEMA_ELEM_INFO_EMPTY} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMA_ELEM_INFO_EMPTY = (1<<5);
    /** C {@code XML_XSD10_SCHEMA_ELEM_INFO_HAS_CONTENT} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMA_ELEM_INFO_HAS_CONTENT = (1<<6);
    /** C {@code XML_XSD10_SCHEMA_ELEM_INFO_HAS_ELEM_CONTENT} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMA_ELEM_INFO_HAS_ELEM_CONTENT = (1<<7);
    /** C {@code XML_XSD10_SCHEMA_ELEM_INFO_ERR_BAD_CONTENT} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMA_ELEM_INFO_ERR_BAD_CONTENT = (1<<8);
    /** C {@code XML_XSD10_SCHEMA_NODE_INFO_ERR_NOT_EXPECTED} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMA_NODE_INFO_ERR_NOT_EXPECTED = (1<<9);
    /** C {@code XML_XSD10_SCHEMA_NODE_INFO_ERR_BAD_TYPE} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMA_NODE_INFO_ERR_BAD_TYPE = (1<<10);
    /** C {@code XML_XSD10_SCHEMAS_ATTR_UNKNOWN} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAS_ATTR_UNKNOWN = 1;
    /** C {@code XML_XSD10_SCHEMAS_ATTR_ASSESSED} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAS_ATTR_ASSESSED = 2;
    /** C {@code XML_XSD10_SCHEMAS_ATTR_PROHIBITED} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAS_ATTR_PROHIBITED = 3;
    /** C {@code XML_XSD10_SCHEMAS_ATTR_ERR_MISSING} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAS_ATTR_ERR_MISSING = 4;
    /** C {@code XML_XSD10_SCHEMAS_ATTR_INVALID_VALUE} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAS_ATTR_INVALID_VALUE = 5;
    /** C {@code XML_XSD10_SCHEMAS_ATTR_ERR_NO_TYPE} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAS_ATTR_ERR_NO_TYPE = 6;
    /** C {@code XML_XSD10_SCHEMAS_ATTR_ERR_FIXED_VALUE} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAS_ATTR_ERR_FIXED_VALUE = 7;
    /** C {@code XML_XSD10_SCHEMAS_ATTR_DEFAULT} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAS_ATTR_DEFAULT = 8;
    /** C {@code XML_XSD10_SCHEMAS_ATTR_VALIDATE_VALUE} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAS_ATTR_VALIDATE_VALUE = 9;
    /** C {@code XML_XSD10_SCHEMAS_ATTR_ERR_WILD_STRICT_NO_DECL} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAS_ATTR_ERR_WILD_STRICT_NO_DECL = 10;
    /** C {@code XML_XSD10_SCHEMAS_ATTR_HAS_ATTR_USE} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAS_ATTR_HAS_ATTR_USE = 11;
    /** C {@code XML_XSD10_SCHEMAS_ATTR_HAS_ATTR_DECL} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAS_ATTR_HAS_ATTR_DECL = 12;
    /** C {@code XML_XSD10_SCHEMAS_ATTR_WILD_SKIP} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAS_ATTR_WILD_SKIP = 13;
    /** C {@code XML_XSD10_SCHEMAS_ATTR_WILD_LAX_NO_DECL} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAS_ATTR_WILD_LAX_NO_DECL = 14;
    /** C {@code XML_XSD10_SCHEMAS_ATTR_ERR_WILD_DUPLICATE_ID} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAS_ATTR_ERR_WILD_DUPLICATE_ID = 15;
    /** C {@code XML_XSD10_SCHEMAS_ATTR_ERR_WILD_AND_USE_ID} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAS_ATTR_ERR_WILD_AND_USE_ID = 16;
    /** C {@code XML_XSD10_SCHEMAS_ATTR_META} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMAS_ATTR_META = 17;
    /** C {@code XML_XSD10_SCHEMA_ATTR_INFO_META_XSI_TYPE} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMA_ATTR_INFO_META_XSI_TYPE = 1;
    /** C {@code XML_XSD10_SCHEMA_ATTR_INFO_META_XSI_NIL} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMA_ATTR_INFO_META_XSI_NIL = 2;
    /** C {@code XML_XSD10_SCHEMA_ATTR_INFO_META_XSI_SCHEMA_LOC} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMA_ATTR_INFO_META_XSI_SCHEMA_LOC = 3;
    /** C {@code XML_XSD10_SCHEMA_ATTR_INFO_META_XSI_NO_NS_SCHEMA_LOC} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMA_ATTR_INFO_META_XSI_NO_NS_SCHEMA_LOC = 4;
    /** C {@code XML_XSD10_SCHEMA_ATTR_INFO_META_XMLNS} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMA_ATTR_INFO_META_XMLNS = 5;
    /** C {@code XML_XSD10_SCHEMA_VALID_CTXT_FLAG_STREAM} (xml_xsd10_schemas.h). */
    int XML_XSD10_SCHEMA_VALID_CTXT_FLAG_STREAM = 1;
}
