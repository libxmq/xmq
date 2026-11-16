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

/* Portions of this file are ported from libxml2 (xmlschemas.c, and the helpers
   it uses from error.c, xmlstring.c, tree.c, dict.c and hash.c, version 2.16.0
   as found in the libxmq fork in 3rdparty/libxml2).
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

import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.AccessDeniedException;
import java.nio.file.FileSystemException;
import java.nio.file.Files;
import java.nio.file.NoSuchFileException;
import java.nio.file.NotDirectoryException;
import java.nio.file.Path;
import java.util.Arrays;
import java.util.List;
import java.util.Objects;

import org.libxmq.imp.xml.XmlAttr;
import org.libxmq.imp.xml.XmlDocument;
import org.libxmq.imp.xml.XmlEntityExpander;
import org.libxmq.imp.xml.XmlNode;
import org.libxmq.imp.xml.XmlNs;
import org.libxmq.imp.xml.XmlParseException;
import org.libxmq.imp.xml.XmlParser;
import org.libxmq.imp.xml.XmlPattern;
import org.libxmq.imp.xml.XmlRegexp;
import org.libxmq.imp.xml.XmlStreamCtxt;
import org.libxmq.imp.xml.XmlUri;

import static org.libxmq.imp.xml.XmlDomApi.*;
import static org.libxmq.imp.xml.xsd10.Xsd10Const.*;
import static org.libxmq.imp.xml.xsd10.Xsd10Parse.*;
import static org.libxmq.imp.xml.xsd10.Xsd10Build.*;
import static org.libxmq.imp.xml.xsd10.Xsd10Valid.*;
import static org.libxmq.imp.xml.xsd10.Xsd10Types.*;

/** Port of xml_xsd10_schemas.c: helpers, error reporting, allocation, the context lifecycle, the dict and hash tables and the public setters. */
public final class Xsd10Schemas
{
    private Xsd10Schemas()
    {
    }

    // Function like macros of xml_xsd10_schemas.h (implemented, not stubs) ///////////////////

    // XSD10_WXS_ITEM_NODE
    /** XSD10_WXS_ITEM_NODE(i). @param i item @return node */
    static XmlNode wxsItemNode(Xsd10BasicItem i) { return getComponentNode(i); }
    // XSD10_WXS_ITEM_TYPE_NAME
    /** XSD10_WXS_ITEM_TYPE_NAME(i). @param i item @return type name */
    static String wxsItemTypeName(Xsd10BasicItem i) { return getComponentTypeStr(i); }
    // XSD10_WXS_ELEM_TYPEDEF
    /** XSD10_WXS_ELEM_TYPEDEF(e). @param e element @return subtypes */
    static Xsd10Type wxsElemTypedef(Xsd10Element e) { return e.subtypes; }
    // XSD10_WXS_SUBST_HEAD
    /** XSD10_WXS_SUBST_HEAD(item). @param item element @return refDecl */
    static Xsd10Element wxsSubstHead(Xsd10Element item) { return item.refDecl; }
    // XSD10_WXS_ATTR_TYPEDEF
    /** XSD10_WXS_ATTR_TYPEDEF(a). @param a attribute @return subtypes */
    static Xsd10Type wxsAttrTypedef(Xsd10Attribute a) { return a.subtypes; }
    // XSD10_WXS_ATTRUSE_DECL
    /** XSD10_WXS_ATTRUSE_DECL(au): the attribute decl (null if attrDecl is not an Xsd10Attribute).
        @param au attribute use @return decl */
    static Xsd10Attribute wxsAttruseDecl(Xsd10BasicItem au)
    {
        Xsd10BasicItem d = ((Xsd10AttributeUse) au).attrDecl;
        return (d instanceof Xsd10Attribute) ? (Xsd10Attribute) d : null;
    }
    // XSD10_WXS_ATTRUSE_TYPEDEF
    /** XSD10_WXS_ATTRUSE_TYPEDEF(au). @param au attribute use @return type */
    static Xsd10Type wxsAttruseTypedef(Xsd10BasicItem au) { return wxsAttrTypedef(wxsAttruseDecl(au)); }
    // XSD10_WXS_ATTRUSE_DECL_NAME
    /** XSD10_WXS_ATTRUSE_DECL_NAME(au). @param au attribute use @return name */
    static String wxsAttruseDeclName(Xsd10BasicItem au) { return wxsAttruseDecl(au).name; }
    // XSD10_WXS_ATTRUSE_DECL_TNS
    /** XSD10_WXS_ATTRUSE_DECL_TNS(au). @param au attribute use @return namespace */
    static String wxsAttruseDeclTns(Xsd10BasicItem au) { return wxsAttruseDecl(au).targetNamespace; }
    // XSD10_WXS_ATTR_GROUP_HAS_REFS
    /** XSD10_WXS_ATTR_GROUP_HAS_REFS(ag). @param ag group @return flag bits */
    static int wxsAttrGroupHasRefs(Xsd10BasicItem ag) { return ((Xsd10AttributeGroup) ag).flags & XML_XSD10_SCHEMAS_ATTRGROUP_HAS_REFS; }
    // XSD10_WXS_ATTR_GROUP_EXPANDED
    /** XSD10_WXS_ATTR_GROUP_EXPANDED(ag). @param ag group @return flag bits */
    static int wxsAttrGroupExpanded(Xsd10BasicItem ag) { return ((Xsd10AttributeGroup) ag).flags & XML_XSD10_SCHEMAS_ATTRGROUP_WILDCARD_BUILDED; }
    // XSD10_WXS_PARTICLE
    /** XSD10_WXS_PARTICLE(p). @param p item @return the particle */
    static Xsd10Particle wxsParticle(Xsd10BasicItem p) { return (Xsd10Particle) p; }
    // XSD10_WXS_PARTICLE_TERM
    /** XSD10_WXS_PARTICLE_TERM(p). @param p particle @return children */
    static Xsd10BasicItem wxsParticleTerm(Xsd10BasicItem p) { return ((Xsd10Particle) p).children; }
    // XSD10_WXS_PARTICLE_TERM_AS_ELEM
    /** XSD10_WXS_PARTICLE_TERM_AS_ELEM(p). @param p particle @return the term as element */
    static Xsd10Element wxsParticleTermAsElem(Xsd10BasicItem p) { return (Xsd10Element) wxsParticleTerm(p); }
    // XSD10_WXS_PARTICLE_MODEL
    /** XSD10_WXS_PARTICLE_MODEL(p). @param p particle @return the term as model group */
    static Xsd10ModelGroup wxsParticleModel(Xsd10BasicItem p) { return (Xsd10ModelGroup) wxsParticleTerm(p); }
    // XSD10_WXS_MODELGROUPDEF_MODEL
    /** XSD10_WXS_MODELGROUPDEF_MODEL(mgd). @param mgd model group def @return children */
    static Xsd10BasicItem wxsModelgroupdefModel(Xsd10BasicItem mgd) { return ((Xsd10TreeItem) mgd).children; }
    // XSD10_WXS_IS_MODEL_GROUP
    /** XSD10_WXS_IS_MODEL_GROUP(i). @param i item @return 1 or 0 */
    static int wxsIsModelGroup(Xsd10BasicItem i)
    {
        return ((i.type == XML_XSD10_SCHEMA_TYPE_SEQUENCE) || (i.type == XML_XSD10_SCHEMA_TYPE_CHOICE)
                || (i.type == XML_XSD10_SCHEMA_TYPE_ALL)) ? 1 : 0;
    }
    // XSD10_WXS_MODELGROUP_PARTICLE
    /** XSD10_WXS_MODELGROUP_PARTICLE(mg). @param mg model group @return the first particle */
    static Xsd10Particle wxsModelgroupParticle(Xsd10BasicItem mg) { return (Xsd10Particle) ((Xsd10TreeItem) mg).children; }
    // XSD10_WXS_IS_BUCKET_INCREDEF
    /** XSD10_WXS_IS_BUCKET_INCREDEF(t). @param t bucket type @return 1 or 0 */
    static int wxsIsBucketIncredef(int t) { return ((t == XML_XSD10_SCHEMA_SCHEMA_INCLUDE) || (t == XML_XSD10_SCHEMA_SCHEMA_REDEFINE)) ? 1 : 0; }
    // XSD10_WXS_IS_BUCKET_IMPMAIN
    /** XSD10_WXS_IS_BUCKET_IMPMAIN(t). @param t bucket type @return 1 or 0 */
    static int wxsIsBucketImpmain(int t) { return ((t == XML_XSD10_SCHEMA_SCHEMA_MAIN) || (t == XML_XSD10_SCHEMA_SCHEMA_IMPORT)) ? 1 : 0; }
    // XSD10_WXS_IMPBUCKET
    /** XSD10_WXS_IMPBUCKET(b). @param b bucket @return import bucket */
    static Xsd10Import wxsImpbucket(Xsd10Bucket b) { return (Xsd10Import) b; }
    // XSD10_WXS_INCBUCKET
    /** XSD10_WXS_INCBUCKET(b). @param b bucket @return include bucket */
    static Xsd10Include wxsIncbucket(Xsd10Bucket b) { return (Xsd10Include) b; }
    // XSD10_WXS_IS_ANYTYPE
    /** XSD10_WXS_IS_ANYTYPE(i). @param i item @return 1 or 0 */
    static int wxsIsAnytype(Xsd10BasicItem i)
    {
        return ((i.type == XML_XSD10_SCHEMA_TYPE_BASIC) && (i instanceof Xsd10Type)
                && (((Xsd10Type) i).builtInType == XML_XSD10_SCHEMAS_ANYTYPE)) ? 1 : 0;
    }
    // XSD10_WXS_IS_COMPLEX
    /** XSD10_WXS_IS_COMPLEX(i). @param i type @return 1 or 0 */
    static int wxsIsComplex(Xsd10Type i) { return ((i.type == XML_XSD10_SCHEMA_TYPE_COMPLEX) || (i.builtInType == XML_XSD10_SCHEMAS_ANYTYPE)) ? 1 : 0; }
    // XSD10_WXS_IS_SIMPLE
    /** XSD10_WXS_IS_SIMPLE(item). @param item type @return 1 or 0 */
    static int wxsIsSimple(Xsd10Type item)
    {
        return ((item.type == XML_XSD10_SCHEMA_TYPE_SIMPLE)
                || ((item.type == XML_XSD10_SCHEMA_TYPE_BASIC) && (item.builtInType != XML_XSD10_SCHEMAS_ANYTYPE))) ? 1 : 0;
    }
    // XSD10_WXS_IS_ANY_SIMPLE_TYPE
    /** XSD10_WXS_IS_ANY_SIMPLE_TYPE(i). @param i type @return 1 or 0 */
    static int wxsIsAnySimpleType(Xsd10Type i) { return ((i.type == XML_XSD10_SCHEMA_TYPE_BASIC) && (i.builtInType == XML_XSD10_SCHEMAS_ANYSIMPLETYPE)) ? 1 : 0; }
    // XSD10_WXS_IS_RESTRICTION
    /** XSD10_WXS_IS_RESTRICTION(t). @param t type @return flag bits */
    static int wxsIsRestriction(Xsd10Type t) { return t.flags & XML_XSD10_SCHEMAS_TYPE_DERIVATION_METHOD_RESTRICTION; }
    // XSD10_WXS_IS_EXTENSION
    /** XSD10_WXS_IS_EXTENSION(t). @param t type @return flag bits */
    static int wxsIsExtension(Xsd10Type t) { return t.flags & XML_XSD10_SCHEMAS_TYPE_DERIVATION_METHOD_EXTENSION; }
    // XSD10_WXS_IS_TYPE_NOT_FIXED
    /** XSD10_WXS_IS_TYPE_NOT_FIXED(i). @param i type @return 1 or 0 */
    static int wxsIsTypeNotFixed(Xsd10Type i) { return ((i.type != XML_XSD10_SCHEMA_TYPE_BASIC) && ((i.flags & XML_XSD10_SCHEMAS_TYPE_INTERNAL_RESOLVED) == 0)) ? 1 : 0; }
    // XSD10_WXS_IS_TYPE_NOT_FIXED_1
    /** XSD10_WXS_IS_TYPE_NOT_FIXED_1(item). @param item type @return 1 or 0 */
    static int wxsIsTypeNotFixed1(Xsd10Type item) { return ((item.type != XML_XSD10_SCHEMA_TYPE_BASIC) && ((item.flags & XML_XSD10_SCHEMAS_TYPE_FIXUP_1) == 0)) ? 1 : 0; }
    // XSD10_WXS_TYPE_IS_GLOBAL
    /** XSD10_WXS_TYPE_IS_GLOBAL(t). @param t type @return flag bits */
    static int wxsTypeIsGlobal(Xsd10Type t) { return t.flags & XML_XSD10_SCHEMAS_TYPE_GLOBAL; }
    // XSD10_WXS_TYPE_IS_LOCAL
    /** XSD10_WXS_TYPE_IS_LOCAL(t). @param t type @return 1 or 0 */
    static int wxsTypeIsLocal(Xsd10Type t) { return ((t.flags & XML_XSD10_SCHEMAS_TYPE_GLOBAL) == 0) ? 1 : 0; }
    // XSD10_WXS_HAS_COMPLEX_CONTENT
    /** XSD10_WXS_HAS_COMPLEX_CONTENT(item). @param item type @return 1 or 0 */
    static int wxsHasComplexContent(Xsd10Type item)
    {
        return ((item.contentType == XML_XSD10_SCHEMA_CONTENT_MIXED) || (item.contentType == XML_XSD10_SCHEMA_CONTENT_EMPTY)
                || (item.contentType == XML_XSD10_SCHEMA_CONTENT_ELEMENTS)) ? 1 : 0;
    }
    // XSD10_WXS_HAS_SIMPLE_CONTENT
    /** XSD10_WXS_HAS_SIMPLE_CONTENT(item). @param item type @return 1 or 0 */
    static int wxsHasSimpleContent(Xsd10Type item)
    {
        return ((item.contentType == XML_XSD10_SCHEMA_CONTENT_SIMPLE) || (item.contentType == XML_XSD10_SCHEMA_CONTENT_BASIC)) ? 1 : 0;
    }
    // XSD10_WXS_HAS_MIXED_CONTENT
    /** XSD10_WXS_HAS_MIXED_CONTENT(item). @param item type @return 1 or 0 */
    static int wxsHasMixedContent(Xsd10Type item) { return (item.contentType == XML_XSD10_SCHEMA_CONTENT_MIXED) ? 1 : 0; }
    // XSD10_WXS_EMPTIABLE
    /** XSD10_WXS_EMPTIABLE(t). @param t type @return isParticleEmptiable of its content particle */
    static int wxsEmptiable(Xsd10Type t) { return isParticleEmptiable((Xsd10Particle) t.subtypes); }
    // XSD10_WXS_TYPE_CONTENTTYPE
    /** XSD10_WXS_TYPE_CONTENTTYPE(t) as an rvalue (as an lvalue write t.subtypes). @param t type @return subtypes */
    static Xsd10BasicItem wxsTypeContenttype(Xsd10Type t) { return t.subtypes; }
    // XSD10_WXS_TYPE_PARTICLE
    /** XSD10_WXS_TYPE_PARTICLE(t). @param t type @return the content particle */
    static Xsd10Particle wxsTypeParticle(Xsd10Type t) { return (Xsd10Particle) t.subtypes; }
    // XSD10_WXS_TYPE_PARTICLE_TERM
    /** XSD10_WXS_TYPE_PARTICLE_TERM(t). @param t type @return the term of the content particle */
    static Xsd10BasicItem wxsTypeParticleTerm(Xsd10Type t) { return wxsParticleTerm(wxsTypeParticle(t)); }
    // XSD10_WXS_LIST_ITEMTYPE
    /** XSD10_WXS_LIST_ITEMTYPE(t). @param t list type @return item type */
    static Xsd10Type wxsListItemtype(Xsd10Type t) { return (Xsd10Type) t.subtypes; }
    // XSD10_WXS_IS_ATOMIC
    /** XSD10_WXS_IS_ATOMIC(t). @param t type @return flag bits */
    static int wxsIsAtomic(Xsd10Type t) { return t.flags & XML_XSD10_SCHEMAS_TYPE_VARIETY_ATOMIC; }
    // XSD10_WXS_IS_LIST
    /** XSD10_WXS_IS_LIST(t). @param t type @return flag bits */
    static int wxsIsList(Xsd10Type t) { return t.flags & XML_XSD10_SCHEMAS_TYPE_VARIETY_LIST; }
    // XSD10_WXS_IS_UNION
    /** XSD10_WXS_IS_UNION(t). @param t type @return flag bits */
    static int wxsIsUnion(Xsd10Type t) { return t.flags & XML_XSD10_SCHEMAS_TYPE_VARIETY_UNION; }
    // XSD10_WXS_CONSTRUCTOR
    /** XSD10_WXS_CONSTRUCTOR(ctx). @param ctx parser context @return constructor */
    static Xsd10ConstructionCtxt wxsConstructor(Xsd10ParserCtxt ctx) { return ctx.constructor; }
    // XSD10_WXS_HAS_BUCKETS
    /** XSD10_WXS_HAS_BUCKETS(ctx). @param ctx parser context @return 1 or 0 */
    static int wxsHasBuckets(Xsd10ParserCtxt ctx)
    {
        return ((wxsConstructor(ctx).buckets != null) && (wxsConstructor(ctx).buckets.nbItems > 0)) ? 1 : 0;
    }
    // XSD10_WXS_SUBST_GROUPS
    /** XSD10_WXS_SUBST_GROUPS(ctx). @param ctx parser context @return substGroups */
    static Xsd10Hash wxsSubstGroups(Xsd10ParserCtxt ctx) { return wxsConstructor(ctx).substGroups; }
    // XSD10_WXS_BUCKET
    /** XSD10_WXS_BUCKET(ctx). @param ctx parser context @return the current bucket */
    static Xsd10Bucket wxsBucket(Xsd10ParserCtxt ctx) { return wxsConstructor(ctx).bucket; }
    // XSD10_WXS_SCHEMA
    /** XSD10_WXS_SCHEMA(ctx). @param ctx parser context @return schema */
    static Xsd10Schema wxsSchema(Xsd10ParserCtxt ctx) { return ctx.schema; }
    // XSD10_WXS_ADD_LOCAL
    /** XSD10_WXS_ADD_LOCAL(ctx, item): use as {@code item = wxsAddLocal(ctx, item);} (null on failure).
        @param <T> item class @param ctx parser context @param item item @return item or null */
    static <T> T wxsAddLocal(Xsd10ParserCtxt ctx, T item)
    {
        if (item != null)
        {
            Xsd10ItemList[] l = { wxsBucket(ctx).locals };
            int r = addItemSize(l, 10, item);
            wxsBucket(ctx).locals = l[0];
            if (r < 0)
            {
                pErrMemory(ctx);
                return null;
            }
        }
        return item;
    }
    // XSD10_WXS_ADD_GLOBAL
    /** XSD10_WXS_ADD_GLOBAL(ctx, item): use as {@code item = wxsAddGlobal(ctx, item);} (null on failure).
        @param <T> item class @param ctx parser context @param item item @return item or null */
    static <T> T wxsAddGlobal(Xsd10ParserCtxt ctx, T item)
    {
        if (item != null)
        {
            Xsd10ItemList[] l = { wxsBucket(ctx).globals };
            int r = addItemSize(l, 5, item);
            wxsBucket(ctx).globals = l[0];
            if (r < 0)
            {
                pErrMemory(ctx);
                return null;
            }
        }
        return item;
    }
    // XSD10_WXS_ADD_PENDING
    /** XSD10_WXS_ADD_PENDING(ctx, item). @param ctx parser context @param item item */
    static void wxsAddPending(Xsd10ParserCtxt ctx, Object item)
    {
        if (item != null)
        {
            Xsd10ItemList[] l = { ctx.constructor.pending };
            int r = addItemSize(l, 10, item);
            ctx.constructor.pending = l[0];
            if (r < 0) pErrMemory(ctx);
        }
    }
    // XSD10_WXS_ILIST_IS_EMPTY
    /** XSD10_WXS_ILIST_IS_EMPTY(l). @param l list @return 1 or 0 */
    static int wxsIlistIsEmpty(Xsd10ItemList l) { return ((l == null) || (l.nbItems == 0)) ? 1 : 0; }
    // XSD10_IS_SCHEMA
    /** XSD10_IS_SCHEMA(node, type): an element named type in the xsd namespace. @param node node
        @param type local name @return 1 or 0 */
    static int isSchema(XmlNode node, String type)
    {
        return ((node != null) && (nodeKind(node) == ELEMENT) && (nodeNs(node) != null)
                && (strEqual(nodeName(node), type) != 0)
                && (strEqual(nsHref(nodeNs(node)), XML_XSD10_NS) != 0)) ? 1 : 0;
    }
    // XSD10_WXS_ARE_DEFAULT_STR_EQUAL
    /** XSD10_WXS_ARE_DEFAULT_STR_EQUAL(v1, v2) (dict pointer equality: Objects.equals). @param v1 a
        @param v2 b @return 1 or 0 */
    static int wxsAreDefaultStrEqual(String v1, String v2) { return Objects.equals(v1, v2) ? 1 : 0; }
    // XSD10_INODE_NILLED
    /** XSD10_INODE_NILLED(item). @param item node info @return flag bits */
    static int inodeNilled(Xsd10NodeInfo item) { return item.flags & XML_XSD10_SCHEMA_ELEM_INFO_NILLED; }
    // XSD10_CAN_PARSE_SCHEMA
    /** XSD10_CAN_PARSE_SCHEMA(b). @param b bucket @return 1 or 0 */
    static int canParseSchema(Xsd10Bucket b) { return ((b.doc != null) && (b.parsed == 0)) ? 1 : 0; }
    // xml_xsd10_get_q_name_ref_name
    /** xml_xsd10_get_q_name_ref_name(r) (a macro). @param r QName ref @return name */
    static String getQNameRefName(Object r) { return ((Xsd10QNameRef) r).name; }
    // xml_xsd10_get_q_name_ref_target_ns
    /** xml_xsd10_get_q_name_ref_target_ns(r) (a macro). @param r QName ref @return namespace */
    static String getQNameRefTargetNs(Object r) { return ((Xsd10QNameRef) r).targetNamespace; }
    // XSD10_VERROR
    /** XSD10_VERROR(err, type, msg) with the local vctxt passed explicitly. @param vctxt context
        @param err code @param type item @param msg message */
    static void verror(Xsd10ValidCtxt vctxt, int err, Xsd10BasicItem type, String msg) { customErr(vctxt, err, null, type, msg, null, null); }
    // XSD10_VERROR_INT
    /** XSD10_VERROR_INT(func, msg). @param vctxt context @param func function name @param msg message */
    static void verrorInt(Xsd10ValidCtxt vctxt, String func, String msg) { internalErr(vctxt, func, msg); }
    // XSD10_PERROR_INT
    /** XSD10_PERROR_INT(func, msg). @param pctxt context @param func function name @param msg message */
    static void perrorInt(Xsd10ParserCtxt pctxt, String func, String msg) { internalErr(pctxt, func, msg); }
    // XSD10_PERROR_INT2
    /** XSD10_PERROR_INT2(func, msg). @param ctxt context @param func function name @param msg message */
    static void perrorInt2(Xsd10ParserCtxt ctxt, String func, String msg) { internalErr(ctxt, func, msg); }
    // XSD10_AERROR_INT
    /** XSD10_AERROR_INT(func, msg). @param actxt context @param func function name @param msg message */
    static void aerrorInt(Xsd10AbstractCtxt actxt, String func, String msg) { internalErr(actxt, func, msg); }

    // XSD10_SCHEMAS_IS_BLANK_CH
    private static boolean isBlankCh(int c)
    {
        return (c == 0x20) || ((0x9 <= c) && (c <= 0xa)) || (c == 0xd);
    }

    /* ***********************************************************************
     *                                                                      *
     *                  C printf formatting                                 *
     *                                                                      *
     ************************************************************************/

    /**
     * A C (glibc) printf for the messages of the xsd 1.0 port, usable by all its parts. Supports the
     * flags {@code - + space # 0}, a width and a precision (digits or {@code *}), the length modifiers
     * {@code hh h l ll z j t L q} (ignored, the Java argument type decides) and the conversions
     * {@code d i u o x X c s p e E f F g G %}. Arguments: Integer/Long/Short/Byte (an Integer is a C int,
     * so %u/%x of a negative Integer prints its 32 bit unsigned value, a Long its 64 bit unsigned value),
     * Boolean (1/0), Character, Double/Float, String (null prints "(null)" like glibc, or "" if the
     * precision is below 6), any Object for %p ("(nil)" for null). Width and precision of %s count UTF-8
     * bytes like C. Missing arguments are taken as null/0.
     * @param fmt the C format
     * @param args the arguments
     * @return the formatted text
     */
    public static String format(String fmt, Object... args)
    {
        if (fmt == null) return null;
        if (args == null) args = new Object[0];
        StringBuilder sb = new StringBuilder();
        int ai = 0;
        int n = fmt.length();
        for (int i = 0; i < n; i++)
        {
            char c = fmt.charAt(i);
            if (c != '%')
            {
                sb.append(c);
                continue;
            }
            int start = i;
            i++;
            if (i >= n)
            {
                sb.append('%');
                break;
            }
            boolean left = false, plus = false, space = false, alt = false, zero = false;
            for (; i < n; i++)
            {
                char f = fmt.charAt(i);
                if (f == '-') left = true;
                else if (f == '+') plus = true;
                else if (f == ' ') space = true;
                else if (f == '#') alt = true;
                else if (f == '0') zero = true;
                else break;
            }
            int width = 0;
            if (i < n && fmt.charAt(i) == '*')
            {
                width = argInt(args, ai++);
                if (width < 0)
                {
                    left = true;
                    width = -width;
                }
                i++;
            }
            else
            {
                while (i < n && fmt.charAt(i) >= '0' && fmt.charAt(i) <= '9') width = width * 10 + (fmt.charAt(i++) - '0');
            }
            int prec = -1;
            if (i < n && fmt.charAt(i) == '.')
            {
                i++;
                if (i < n && fmt.charAt(i) == '*')
                {
                    prec = argInt(args, ai++);
                    if (prec < 0) prec = -1;
                    i++;
                }
                else
                {
                    prec = 0;
                    while (i < n && fmt.charAt(i) >= '0' && fmt.charAt(i) <= '9') prec = prec * 10 + (fmt.charAt(i++) - '0');
                }
            }
            while (i < n && "hlzjtLq".indexOf(fmt.charAt(i)) >= 0) i++;
            if (i >= n)
            {
                sb.append(fmt, start, n);
                break;
            }
            char conv = fmt.charAt(i);
            String body;
            String prefix = "";
            boolean numeric = false;
            switch (conv)
            {
            case '%':
                sb.append('%');
                continue;
            case 's':
            {
                Object o = ai < args.length ? args[ai] : null;
                ai++;
                String s;
                if (o == null) s = (prec >= 0 && prec < 6) ? "" : "(null)";
                else s = o.toString();
                if (prec >= 0) s = utf8Prefix(s, prec);
                int len = utf8Length(s);
                StringBuilder pad = new StringBuilder();
                for (int k = len; k < width; k++) pad.append(' ');
                if (left) sb.append(s).append(pad);
                else sb.append(pad).append(s);
                continue;
            }
            case 'c':
            {
                Object o = ai < args.length ? args[ai] : null;
                ai++;
                String s;
                if (o instanceof Character ch) s = ch.toString();
                else if (o instanceof Number num) s = new String(Character.toChars(num.intValue() & 0xff));
                else s = "\0";
                StringBuilder pad = new StringBuilder();
                for (int k = 1; k < width; k++) pad.append(' ');
                if (left) sb.append(s).append(pad);
                else sb.append(pad).append(s);
                continue;
            }
            case 'p':
            {
                Object o = ai < args.length ? args[ai] : null;
                ai++;
                body = o == null ? "(nil)" : "0x" + Integer.toHexString(System.identityHashCode(o));
                break;
            }
            case 'd':
            case 'i':
            {
                long v = argLong(args, ai++);
                numeric = true;
                String digits = (v < 0) ? (v == Long.MIN_VALUE ? "9223372036854775808" : Long.toString(-v)) : Long.toString(v);
                if (prec == 0 && v == 0) digits = "";
                while (prec >= 0 && digits.length() < prec) digits = "0" + digits;
                prefix = (v < 0) ? "-" : plus ? "+" : space ? " " : "";
                body = digits;
                break;
            }
            case 'u':
            case 'o':
            case 'x':
            case 'X':
            {
                Object o = ai < args.length ? args[ai] : null;
                ai++;
                long v = argLong(args, ai - 1);
                boolean wide = !(o instanceof Integer || o instanceof Short || o instanceof Byte
                                 || o instanceof Character || o instanceof Boolean);
                if (!wide) v &= 0xffffffffL;
                numeric = true;
                String digits;
                if (conv == 'u') digits = Long.toUnsignedString(v);
                else if (conv == 'o') digits = Long.toOctalString(v);
                else digits = Long.toHexString(v);
                if (conv == 'X') digits = digits.toUpperCase(java.util.Locale.ROOT);
                if (prec == 0 && v == 0) digits = "";
                while (prec >= 0 && digits.length() < prec) digits = "0" + digits;
                if (alt && conv == 'o' && !digits.startsWith("0")) digits = "0" + digits;
                if (alt && v != 0 && conv == 'x') prefix = "0x";
                if (alt && v != 0 && conv == 'X') prefix = "0X";
                body = digits;
                break;
            }
            case 'e':
            case 'E':
            case 'f':
            case 'F':
            case 'g':
            case 'G':
            {
                double d = argDouble(args, ai++);
                boolean neg = (Double.doubleToRawLongBits(d) & Long.MIN_VALUE) != 0;
                double a = Math.abs(d);
                boolean upper = Character.isUpperCase(conv);
                if (Double.isNaN(d) || Double.isInfinite(d))
                {
                    body = Double.isNaN(d) ? "nan" : "inf";
                    if (upper) body = body.toUpperCase(java.util.Locale.ROOT);
                    prefix = neg ? "-" : plus ? "+" : space ? " " : "";
                    zero = false;
                    numeric = true;
                    break;
                }
                int p = prec < 0 ? 6 : prec;
                char lc = Character.toLowerCase(conv);
                if (lc == 'e') body = fmtE(a, p, alt);
                else if (lc == 'f') body = fmtF(a, p, alt);
                else body = fmtG(a, p, alt);
                if (upper) body = body.toUpperCase(java.util.Locale.ROOT);
                prefix = neg ? "-" : plus ? "+" : space ? " " : "";
                numeric = true;
                // The 0 flag applies to floating point conversions even with a precision.
                prec = -1;
                break;
            }
            default:
                // Unknown conversion: printed as it is (glibc prints the directive).
                sb.append(fmt, start, i + 1);
                continue;
            }
            int len = prefix.length() + body.length();
            if (len >= width)
            {
                sb.append(prefix).append(body);
            }
            else if (left)
            {
                sb.append(prefix).append(body);
                for (int k = len; k < width; k++) sb.append(' ');
            }
            else if (zero && numeric && prec < 0)
            {
                sb.append(prefix);
                for (int k = len; k < width; k++) sb.append('0');
                sb.append(body);
            }
            else
            {
                for (int k = len; k < width; k++) sb.append(' ');
                sb.append(prefix).append(body);
            }
        }
        return sb.toString();
    }

    private static int argInt(Object[] args, int i)
    {
        return (int) argLong(args, i);
    }

    private static long argLong(Object[] args, int i)
    {
        Object o = i < args.length ? args[i] : null;
        if (o instanceof Number num) return num.longValue();
        if (o instanceof Boolean b) return b ? 1 : 0;
        if (o instanceof Character ch) return ch;
        return 0;
    }

    private static double argDouble(Object[] args, int i)
    {
        Object o = i < args.length ? args[i] : null;
        if (o instanceof Number num) return num.doubleValue();
        return 0;
    }

    /** The digits of a (positive or zero) double rounded to n significant digits and the decimal
        exponent of the first digit: { digits, exponent }. */
    private static Object[] roundSignificant(double a, int n)
    {
        if (a == 0)
        {
            return new Object[] { "0".repeat(n), 0 };
        }
        java.math.BigDecimal bd = new java.math.BigDecimal(a).round(new java.math.MathContext(n, java.math.RoundingMode.HALF_EVEN));
        String digits = bd.unscaledValue().toString();
        int exp = digits.length() - 1 - bd.scale();
        if (digits.length() > n)
        {
            // Trailing zeros of the unscaled value.
            digits = digits.substring(0, n);
        }
        while (digits.length() < n) digits += "0";
        return new Object[] { digits, exp };
    }

    private static String fmtE(double a, int p, boolean alt)
    {
        Object[] r = roundSignificant(a, p + 1);
        String digits = (String) r[0];
        int exp = (Integer) r[1];
        StringBuilder sb = new StringBuilder();
        sb.append(digits.charAt(0));
        if (p > 0 || alt) sb.append('.');
        sb.append(digits, 1, digits.length());
        sb.append('e').append(exp < 0 ? '-' : '+');
        int ae = Math.abs(exp);
        if (ae < 10) sb.append('0');
        sb.append(ae);
        return sb.toString();
    }

    private static String fmtF(double a, int p, boolean alt)
    {
        String s = new java.math.BigDecimal(a).setScale(p, java.math.RoundingMode.HALF_EVEN).toPlainString();
        if (p == 0 && alt) s += ".";
        return s;
    }

    private static String fmtG(double a, int p, boolean alt)
    {
        int P = (p == 0) ? 1 : p;
        int x = (a == 0) ? 0 : (Integer) roundSignificant(a, P)[1];
        String s;
        if (P > x && x >= -4) s = fmtF(a, P - 1 - x, alt);
        else s = fmtE(a, P - 1, alt);
        if (!alt)
        {
            int e = s.indexOf('e');
            String mant = e >= 0 ? s.substring(0, e) : s;
            String rest = e >= 0 ? s.substring(e) : "";
            if (mant.indexOf('.') >= 0)
            {
                int k = mant.length();
                while (mant.charAt(k - 1) == '0') k--;
                if (mant.charAt(k - 1) == '.') k--;
                mant = mant.substring(0, k);
            }
            s = mant + rest;
        }
        return s;
    }

    /** The UTF-8 byte length of s (0 for null). */
    private static int utf8Length(String s)
    {
        if (s == null) return 0;
        int n = 0;
        for (int i = 0; i < s.length(); i++)
        {
            char c = s.charAt(i);
            if (c < 0x80) n++;
            else if (c < 0x800) n += 2;
            else if (Character.isHighSurrogate(c) && i + 1 < s.length() && Character.isLowSurrogate(s.charAt(i + 1)))
            {
                n += 4;
                i++;
            }
            else n += 3;
        }
        return n;
    }

    /** The first len UTF-8 bytes of s (C memcpy of len bytes). A character split by the cut is
        dropped (Java strings cannot hold partial UTF-8 sequences). */
    private static String utf8Prefix(String s, int len)
    {
        if (len <= 0) return "";
        int n = 0;
        for (int i = 0; i < s.length(); i++)
        {
            char c = s.charAt(i);
            int w;
            int chars = 1;
            if (c < 0x80) w = 1;
            else if (c < 0x800) w = 2;
            else if (Character.isHighSurrogate(c) && i + 1 < s.length() && Character.isLowSurrogate(s.charAt(i + 1)))
            {
                w = 4;
                chars = 2;
            }
            else w = 3;
            if (n + w > len) return s.substring(0, i);
            n += w;
            i += chars - 1;
        }
        return s;
    }

    // xsd10_schemas_vasprintf
    /** Port of xmlStrVASPrintf with maxSize: the formatted message, truncated to maxSize - 1 bytes (and
        then without a trailing incomplete UTF-8 sequence) if longer. null if msg is null.
        @param maxSize the maximum size incl. the C terminator @param msg the format @param ap the args
        @return the message or null */
    private static String vasprintf(int maxSize, String msg, Object[] ap)
    {
        if (msg == null) return null;
        if (maxSize < 32) maxSize = 32;
        String s = format(msg, ap);
        int len = utf8Length(s);
        if (len < maxSize) return s;
        byte[] buf = s.getBytes(StandardCharsets.UTF_8);
        int size = maxSize;
        int i = size - 1;
        while (i > 0)
        {
            /* Break after ASCII */
            if ((buf[i - 1] & 0xff) < 0x80) break;
            i -= 1;
            /* Break before non-ASCII */
            if ((buf[i] & 0xff) >= 0xc0) break;
        }
        return new String(buf, 0, i, StandardCharsets.UTF_8);
    }

    /* ***********************************************************************
     *                                                                      *
     *                  String helpers (libxml2 xmlstring.c)                *
     *                                                                      *
     ************************************************************************/

    // xml_xsd10_str_equal
    /** Port of xml_xsd10_str_equal. @param a a @param b b @return 1 if equal (both null too) */
    static int strEqual(String a, String b)
    {
        if (a == b) return 1;
        if (a == null || b == null) return 0;
        return a.equals(b) ? 1 : 0;
    }

    // xml_xsd10_strdup
    /** Port of xml_xsd10_strdup. @param s s @return the copy (s itself) */
    static String strdup(String s)
    {
        if (s == null) return null;
        return s;
    }

    // xml_xsd10_strndup
    /** Port of xml_xsd10_strndup: the first len UTF-8 bytes of s. @param s s @param len byte length
        @return the copy or null */
    static String strndup(String s, int len)
    {
        if ((s == null) || (len < 0)) return null;
        return utf8Prefix(s, len);
    }

    // xml_xsd10_char_strdup
    /** Port of xml_xsd10_char_strdup. @param s s @return the copy */
    static String charStrdup(String s)
    {
        return strdup(s);
    }

    // xml_xsd10_strlen
    /** Port of xml_xsd10_strlen: the UTF-8 byte length. @param s s @return the length */
    static int strlen(String s)
    {
        if (s == null) return 0;
        return utf8Length(s);
    }

    // xml_xsd10_strcat
    /** Port of xml_xsd10_strcat. @param cur cur @param add add @return the concatenation */
    static String strcat(String cur, String add)
    {
        if (add == null)
            return cur;
        if (cur == null)
            return strdup(add);
        return strncat(cur, add, strlen(add));
    }

    // xml_xsd10_strncat
    /** Port of xml_xsd10_strncat (len in UTF-8 bytes of add). @param cur cur @param add add
        @param len length @return the concatenation */
    static String strncat(String cur, String add, int len)
    {
        if ((add == null) || (len == 0))
            return cur;
        if (len < 0)
            return null;
        if (cur == null)
            return strndup(add, len);
        return cur + utf8Prefix(add, len);
    }

    // xml_xsd10_strncat_new
    /** Port of xml_xsd10_strncat_new. @param str1 str1 @param str2 str2 @param len byte length of str2
        (negative for all) @return the new string */
    static String strncatNew(String str1, String str2, int len)
    {
        if (len < 0)
        {
            len = strlen(str2);
            if (len < 0)
                return null;
        }
        if (str1 == null)
            return strndup(str2, len);
        if ((str2 == null) || (len == 0))
            return strdup(str1);
        return str1 + utf8Prefix(str2, len);
    }

    // xml_xsd10_escape_format_string
    /** Port of xmlEscapeFormatString: replaces msg[0] with an escaped string, all '%' characters are
        doubled. @param msg the string (in/out) @return msg[0] or null */
    static String escapeFormatString(String[] msg)
    {
        if (msg == null || msg[0] == null)
            return null;
        if (msg[0].indexOf('%') < 0)
            return msg[0];
        msg[0] = msg[0].replace("%", "%%");
        return msg[0];
    }

    // xml_xsd10_grow_capacity
    /** Port of xmlGrowCapacity. @param capacity current @param elemSize element size @param min initial
        @param max maximum @return the new capacity or -1 */
    static int growCapacity(int capacity, long elemSize, int min, int max)
    {
        int extra;

        if (capacity <= 0)
            return min;

        if ((capacity >= max) ||
            ((elemSize > 0) && ((long) capacity > Long.MAX_VALUE / elemSize)))
            return -1;

        /* Grow by 50% */
        extra = (capacity + 1) / 2;

        if (capacity > max - extra)
            return max;

        return capacity + extra;
    }

    /* ***********************************************************************
     *                                                                      *
     *                  The dict (libxml2 dict.c)                           *
     *                                                                      *
     ************************************************************************/

    // xml_xsd10_dict_create
    /** Port of xml_xsd10_dict_create. @return the dict */
    static Xsd10Dict dictCreate()
    {
        Xsd10Dict dict = new Xsd10Dict();
        dict.ref_counter = 1;
        return dict;
    }

    // xml_xsd10_dict_reference
    /** Port of xml_xsd10_dict_reference. @param dict dict @return 0 or -1 */
    static int dictReference(Xsd10Dict dict)
    {
        if (dict == null) return -1;
        dict.ref_counter++;
        return 0;
    }

    // xml_xsd10_dict_free
    /** Port of xml_xsd10_dict_free. @param dict dict */
    static void dictFree(Xsd10Dict dict)
    {
        if (dict == null) return;
        dict.ref_counter--;
        if (dict.ref_counter > 0) return;
        dict.strings.clear();
    }

    // xml_xsd10_dict_lookup
    /** Port of xml_xsd10_dict_lookup: the interned string of the first len UTF-8 bytes of name (all of
        it if len is negative). @param dict dict @param name name @param len length @return the string */
    static String dictLookup(Xsd10Dict dict, String name, int len)
    {
        if ((dict == null) || (name == null))
            return null;
        String key = (len < 0) ? name : utf8Prefix(name, len);
        String ret = dict.strings.get(key);
        if (ret != null) return ret;
        dict.strings.put(key, key);
        return key;
    }

    // xml_xsd10_dict_qlookup
    /** Port of xml_xsd10_dict_qlookup. @param dict dict @param prefix prefix @param name name
        @return the interned "prefix:name" */
    static String dictQlookup(Xsd10Dict dict, String prefix, String name)
    {
        if (name == null) return null;
        if (prefix == null) return dictLookup(dict, name, -1);
        return dictLookup(dict, prefix + ":" + name, -1);
    }

    // xml_xsd10_dict_owns
    /** Port of xml_xsd10_dict_owns (C pointer identity: the interned instance). @param dict dict
        @param str string @return 1, 0 or -1 */
    static int dictOwns(Xsd10Dict dict, String str)
    {
        if ((dict == null) || (str == null)) return -1;
        return (dict.strings.get(str) == str) ? 1 : 0;
    }

    // xml_xsd10_dict_size
    /** Port of xml_xsd10_dict_size. @param dict dict @return the number of strings or -1 */
    static int dictSize(Xsd10Dict dict)
    {
        if (dict == null) return -1;
        return dict.strings.size();
    }

    /* ***********************************************************************
     *                                                                      *
     *                  Hash tables (libxml2 hash.c api)                    *
     *                                                                      *
     ************************************************************************/

    // xml_xsd10_hash_create
    /** Port of xml_xsd10_hash_create. @param size the initial size (unused) @return the hash */
    static Xsd10Hash hashCreate(int size)
    {
        return new Xsd10Hash();
    }

    // xml_xsd10_hash_create_dict
    /** Port of xml_xsd10_hash_create_dict. @param size size @param dict dict (unused) @return the hash */
    static Xsd10Hash hashCreateDict(int size, Xsd10Dict dict)
    {
        return hashCreate(size);
    }

    // xml_xsd10_hash_free
    /** Port of xml_xsd10_hash_free. @param hash hash @param dealloc deallocator or null */
    static void hashFree(Xsd10Hash hash, HashDeallocator dealloc)
    {
        if (hash == null) return;
        hash.freeAll(dealloc);
    }

    // xml_xsd10_hash_add_entry
    /** Port of xml_xsd10_hash_add_entry. @param hash hash @param name key @param payload payload
        @return 0 or -1 */
    static int hashAddEntry(Xsd10Hash hash, String name, Object payload)
    {
        return hashAddEntry3(hash, name, null, null, payload);
    }

    // xml_xsd10_hash_add_entry2
    /** Port of xml_xsd10_hash_add_entry2. @param hash hash @param name key @param name2 key2
        @param payload payload @return 0 or -1 */
    static int hashAddEntry2(Xsd10Hash hash, String name, String name2, Object payload)
    {
        return hashAddEntry3(hash, name, name2, null, payload);
    }

    // xml_xsd10_hash_add_entry3
    /** Port of xml_xsd10_hash_add_entry3. @param hash hash @param name key @param name2 key2
        @param name3 key3 @param payload payload @return 0 or -1 (also if the key exists) */
    static int hashAddEntry3(Xsd10Hash hash, String name, String name2, String name3, Object payload)
    {
        if (hash == null) return -1;
        int res = hash.updateInternal(name, name2, name3, payload, null, 0);

        if (res == 0)
            res = -1;
        else if (res == 1)
            res = 0;

        return res;
    }

    // xml_xsd10_hash_update_entry
    /** Port of xml_xsd10_hash_update_entry. @param hash hash @param name key @param payload payload
        @param dealloc deallocator of a replaced payload @return 0 or -1 */
    static int hashUpdateEntry(Xsd10Hash hash, String name, Object payload, HashDeallocator dealloc)
    {
        if (hash == null) return -1;
        int res = hash.updateInternal(name, null, null, payload, dealloc, 1);

        if (res == 1)
            res = 0;

        return res;
    }

    // xml_xsd10_hash_lookup
    /** Port of xml_xsd10_hash_lookup. @param hash hash @param name key @return the payload or null */
    static Object hashLookup(Xsd10Hash hash, String name)
    {
        return hashLookup3(hash, name, null, null);
    }

    // xml_xsd10_hash_lookup2
    /** Port of xml_xsd10_hash_lookup2. @param hash hash @param name key @param name2 key2
        @return the payload or null */
    static Object hashLookup2(Xsd10Hash hash, String name, String name2)
    {
        return hashLookup3(hash, name, name2, null);
    }

    // xml_xsd10_hash_lookup3
    /** Port of xml_xsd10_hash_lookup3. @param hash hash @param name key @param name2 key2
        @param name3 key3 @return the payload or null */
    static Object hashLookup3(Xsd10Hash hash, String name, String name2, String name3)
    {
        if ((hash == null) || (name == null)) return null;
        Xsd10Hash.Entry e = hash.find(name, name2, name3);
        if (e == null) return null;
        return e.payload;
    }

    // xml_xsd10_hash_remove_entry
    /** Port of xml_xsd10_hash_remove_entry. @param hash hash @param name key @param dealloc deallocator
        @return 0 or -1 */
    static int hashRemoveEntry(Xsd10Hash hash, String name, HashDeallocator dealloc)
    {
        return hashRemoveEntry3(hash, name, null, null, dealloc);
    }

    // xml_xsd10_hash_remove_entry2
    /** Port of xml_xsd10_hash_remove_entry2. @param hash hash @param name key @param name2 key2
        @param dealloc deallocator @return 0 or -1 */
    static int hashRemoveEntry2(Xsd10Hash hash, String name, String name2, HashDeallocator dealloc)
    {
        return hashRemoveEntry3(hash, name, name2, null, dealloc);
    }

    // xml_xsd10_hash_remove_entry3
    /** Port of xml_xsd10_hash_remove_entry3. @param hash hash @param name key @param name2 key2
        @param name3 key3 @param dealloc deallocator @return 0 or -1 */
    static int hashRemoveEntry3(Xsd10Hash hash, String name, String name2, String name3, HashDeallocator dealloc)
    {
        if ((hash == null) || (name == null)) return -1;
        return hash.remove(name, name2, name3, dealloc);
    }

    // xml_xsd10_hash_size
    /** Port of xml_xsd10_hash_size. @param hash hash @return the number of entries or -1 */
    static int hashSize(Xsd10Hash hash)
    {
        if (hash == null) return -1;
        return hash.nb_elems;
    }

    // xml_xsd10_hash_scan
    /** Port of xml_xsd10_hash_scan (through xsd10_schemas_stub_hash_scanner_full). @param hash hash
        @param scan scanner @param data user data */
    static void hashScan(Xsd10Hash hash, HashScanner scan, Object data)
    {
        if (scan == null) return;
        hashScanFull(hash, (payload, d, key, key2, key3) -> scan.scan(payload, data, key), null);
    }

    // xml_xsd10_hash_scan_full
    /** Port of xml_xsd10_hash_scan_full: entries added by the callback are scanned too, removed ones are
        skipped. @param hash hash @param scan scanner @param data user data */
    static void hashScanFull(Xsd10Hash hash, HashScannerFull scan, Object data)
    {
        if ((hash == null) || (scan == null)) return;
        hash.scanFull(scan, data);
    }

    /* ***********************************************************************
     *                                                                      *
     *          Tree helpers (libxml2 tree.c, parser.c)                     *
     *                                                                      *
     ************************************************************************/

    // xsd10_schemas_is_name_start_char
    /* Port of xmlIsNameStartCharNew (XML 1.0 fifth edition). */
    private static int isNameStartChar(int c)
    {
        if ((c != ' ') && (c != '>') && (c != '/') && /* accelerators */
            (((c >= 'a') && (c <= 'z')) ||
             ((c >= 'A') && (c <= 'Z')) ||
             (c == '_') || (c == ':') ||
             ((c >= 0xC0) && (c <= 0xD6)) ||
             ((c >= 0xD8) && (c <= 0xF6)) ||
             ((c >= 0xF8) && (c <= 0x2FF)) ||
             ((c >= 0x370) && (c <= 0x37D)) ||
             ((c >= 0x37F) && (c <= 0x1FFF)) ||
             ((c >= 0x200C) && (c <= 0x200D)) ||
             ((c >= 0x2070) && (c <= 0x218F)) ||
             ((c >= 0x2C00) && (c <= 0x2FEF)) ||
             ((c >= 0x3001) && (c <= 0xD7FF)) ||
             ((c >= 0xF900) && (c <= 0xFDCF)) ||
             ((c >= 0xFDF0) && (c <= 0xFFFD)) ||
             ((c >= 0x10000) && (c <= 0xEFFFF))))
            return 1;
        return 0;
    }

    // xsd10_schemas_is_name_char
    /* Port of xmlIsNameCharNew (XML 1.0 fifth edition). */
    private static int isNameChar(int c)
    {
        if ((c != ' ') && (c != '>') && (c != '/') && /* accelerators */
            (((c >= 'a') && (c <= 'z')) ||
             ((c >= 'A') && (c <= 'Z')) ||
             ((c >= '0') && (c <= '9')) || /* !start */
             (c == '_') || (c == ':') ||
             (c == '-') || (c == '.') || (c == 0xB7) || /* !start */
             ((c >= 0xC0) && (c <= 0xD6)) ||
             ((c >= 0xD8) && (c <= 0xF6)) ||
             ((c >= 0xF8) && (c <= 0x2FF)) ||
             ((c >= 0x300) && (c <= 0x36F)) || /* !start */
             ((c >= 0x370) && (c <= 0x37D)) ||
             ((c >= 0x37F) && (c <= 0x1FFF)) ||
             ((c >= 0x200C) && (c <= 0x200D)) ||
             ((c >= 0x203F) && (c <= 0x2040)) || /* !start */
             ((c >= 0x2070) && (c <= 0x218F)) ||
             ((c >= 0x2C00) && (c <= 0x2FEF)) ||
             ((c >= 0x3001) && (c <= 0xD7FF)) ||
             ((c >= 0xF900) && (c <= 0xFDCF)) ||
             ((c >= 0xFDF0) && (c <= 0xFFFD)) ||
             ((c >= 0x10000) && (c <= 0xEFFFF))))
            return 1;
        return 0;
    }

    // xsd10_schemas_scan_ncname
    /* Port of xmlScanName with XML_SCAN_NC (no size limit): the index of the end of the name, ptr if
       the name is invalid. A lone surrogate is an invalid UTF-8 sequence in C. */
    private static int scanNcname(String s, int ptr)
    {
        boolean first = true;

        while (true)
        {
            int c, len;

            if (ptr >= s.length())
                break;
            c = s.charAt(ptr);
            if (c < 0x80)
            {
                if (c == ':')
                    break;
                len = 1;
            }
            else
            {
                c = s.codePointAt(ptr);
                if (c >= 0xd800 && c < 0xe000)
                    break;
                len = Character.charCount(c);
            }

            if (first ? (isNameStartChar(c) == 0) : (isNameChar(c) == 0))
                break;

            ptr += len;
            first = false;
        }

        return ptr;
    }

    // xml_xsd10_tree_validate_ncname
    /** Port of xmlValidateNCName. @param v value @param space allow surrounding blanks
        @return 0 if valid, 1 if not, -1 for null */
    static int treeValidateNcname(String v, int space)
    {
        int value, cur;

        if (v == null)
            return -1;

        cur = 0;

        if (space != 0)
        {
            while (cur < v.length() && isBlankCh(v.charAt(cur)))
                cur++;
        }

        value = cur;
        cur = scanNcname(v, value);
        if (cur == value)
            return 1;

        if (space != 0)
        {
            while (cur < v.length() && isBlankCh(v.charAt(cur)))
                cur++;
        }

        return (cur < v.length() && v.charAt(cur) != 0) ? 1 : 0;
    }

    // xml_xsd10_tree_validate_qname
    /** Port of xmlValidateQName. @param v value @param space allow surrounding blanks
        @return 0 if valid, 1 if not, -1 for null */
    static int treeValidateQname(String v, int space)
    {
        int value, cur;

        if (v == null)
            return -1;

        cur = 0;

        if (space != 0)
        {
            while (cur < v.length() && isBlankCh(v.charAt(cur)))
                cur++;
        }

        value = cur;
        cur = scanNcname(v, value);
        if (cur == value)
            return 1;

        if (cur < v.length() && v.charAt(cur) == ':')
        {
            cur += 1;
            value = cur;
            cur = scanNcname(v, value);
            if (cur == value)
                return 1;
        }

        if (space != 0)
        {
            while (cur < v.length() && isBlankCh(v.charAt(cur)))
                cur++;
        }

        return (cur < v.length() && v.charAt(cur) != 0) ? 1 : 0;
    }

    // xml_xsd10_tree_split_qname2
    /** Port of xmlSplitQName2. @param name the qname @param prefix out: the prefix
        @return the local name or null if name is not a prefixed name */
    static String treeSplitQname2(String name, String[] prefix)
    {
        int len = 0;

        if (prefix == null) return null;
        prefix[0] = null;
        if (name == null) return null;

        /* nasty but valid */
        if (name.startsWith(":"))
            return null;

        /*
         * we are not trying to validate but just to cut, and yes it will
         * work even if this is as set of UTF-8 encoded chars
         */
        while ((len < name.length()) && (name.charAt(len) != ':'))
            len++;

        if ((len >= name.length()) || (len + 1 >= name.length()))
            return null;

        prefix[0] = name.substring(0, len);
        return name.substring(len + 1);
    }

    // xml_xsd10_tree_split_qname3
    /** Port of xmlSplitQName3. @param name the qname @param len out: the length of the prefix in UTF-8
        bytes (use strndup(name, len[0]) for the prefix) @return the local name or null */
    static String treeSplitQname3(String name, int[] len)
    {
        int l = 0;

        if (name == null) return null;
        if (len == null) return null;

        /* nasty but valid */
        if (name.startsWith(":"))
            return null;

        /*
         * we are not trying to validate but just to cut, and yes it will
         * work even if this is as set of UTF-8 encoded chars
         */
        while ((l < name.length()) && (name.charAt(l) != ':'))
            l++;

        if ((l >= name.length()) || (l + 1 >= name.length()))
            return null;

        len[0] = utf8Length(name.substring(0, l));

        return name.substring(l + 1);
    }

    // xsd10_schemas_element_of
    /* The closest element of node (node itself, the owner of an attribute, the parent of a text). */
    private static XmlNode elementOf(XmlNode node)
    {
        while ((node != null) && (nodeKind(node) != ELEMENT))
            node = nodeParent(node);
        return node;
    }

    // xml_xsd10_tree_search_ns
    /** Port of xmlSearchNs. @param node node @param prefix prefix @return the ns or null */
    static XmlNs treeSearchNs(XmlNode node, String prefix)
    {
        XmlNs ns;

        if (node == null)
            return null;
        node = elementOf(node);
        if (node == null)
            return null;
        ns = nsLookupPrefix(node, prefix);
        if ((ns == null) || (nsHref(ns) == null))
            return null;
        return ns;
    }

    // xml_xsd10_tree_search_ns_by_href
    /** Port of xmlSearchNsByHref: a declaration of href in scope of node whose prefix is not shadowed,
        with a prefix if node is an attribute. @param node node @param href href @return the ns or null */
    static XmlNs treeSearchNsByHref(XmlNode node, String href)
    {
        XmlNode elem, cur;
        boolean is_attr;

        if ((node == null) || (href == null))
            return null;
        is_attr = (nodeKind(node) == ATTRIBUTE);
        elem = elementOf(node);
        if (elem == null)
            return null;
        if (strEqual(href, XML_XSD10_XML_XML_NAMESPACE) != 0)
            return nsLookupPrefix(elem, "xml");
        for (cur = elem; (cur != null) && (nodeKind(cur) == ELEMENT); cur = nodeParent(cur))
        {
            for (XmlNs ns : nsDecls(cur))
            {
                if (strEqual(nsHref(ns), href) == 0)
                    continue;
                if (is_attr && (nsPrefix(ns) == null))
                    continue;
                if (nsLookupPrefix(elem, nsPrefix(ns)) == ns)
                    return ns;
            }
        }
        return null;
    }

    /* The namespace declarations of an element (xml_node_first_ns_decl / xml_ns_next_decl). */
    private static List<XmlNs> nsDecls(XmlNode e)
    {
        List<XmlNs> l = nodeNsDecls(e);
        return l == null ? java.util.Collections.emptyList() : l;
    }

    // xml_xsd10_tree_get_ns_list
    /** Port of xmlGetNsList. @param node node @return the in scope namespaces, terminated by a null
        entry (like C), or null if none */
    static XmlNs[] treeGetNsList(XmlNode node)
    {
        XmlNs[] namespaces = null;
        int nbns = 0;
        int maxns = 0;
        int i;

        if (node == null)
            return null;

        while (node != null)
        {
            if (nodeKind(node) == ELEMENT)
            {
                for (XmlNs cur : nsDecls(node))
                {
                    for (i = 0; i < nbns; i++)
                    {
                        if (strEqual(nsPrefix(cur), nsPrefix(namespaces[i])) != 0)
                            break;
                    }
                    if (i >= nbns)
                    {
                        if (nbns >= maxns)
                        {
                            int newSize;

                            newSize = growCapacity(maxns, 8, 10, XML_XSD10_MAX_ITEMS);
                            if (newSize < 0)
                                return null;
                            namespaces = (namespaces == null) ? new XmlNs[newSize + 1] : Arrays.copyOf(namespaces, newSize + 1);
                            maxns = newSize;
                        }
                        namespaces[nbns++] = cur;
                        namespaces[nbns] = null;
                    }
                }
            }
            node = nodeParent(node);
        }

        return namespaces;
    }

    // xml_xsd10_tree_get_no_ns_prop
    /** Port of xmlGetNoNsProp. @param node element @param name name @return the value or null */
    static String treeGetNoNsProp(XmlNode node, String name)
    {
        if ((node == null) || (name == null) || (nodeKind(node) != ELEMENT))
            return null;
        for (XmlAttr a = nodeFirstAttr(node); a != null; a = attrNext(a))
        {
            XmlNode an = attrNode(a);
            if ((nodeNsUri(an) == null) && (strEqual(nodeName(an), name) != 0))
                return nodeStringValue(an);
        }
        return null;
    }

    // xml_xsd10_tree_has_prop
    /** Port of xmlHasProp (without the dtd defaults). @param node element @param name name
        @return the attribute node or null */
    static XmlNode treeHasProp(XmlNode node, String name)
    {
        if ((node == null) || (name == null) || (nodeKind(node) != ELEMENT))
            return null;
        for (XmlAttr a = nodeFirstAttr(node); a != null; a = attrNext(a))
        {
            XmlNode an = attrNode(a);
            if (strEqual(nodeName(an), name) != 0)
                return an;
        }
        return null;
    }

    // xml_xsd10_tree_node_get_content
    /** Port of xmlNodeGetContent. @param node node @return the content or null */
    static String treeNodeGetContent(XmlNode node)
    {
        if (node == null)
            return null;
        switch (nodeKind(node))
        {
        case ELEMENT:
        case ATTRIBUTE:
        case DOCUMENT:
            return nodeStringValue(node);
        case ENTITY_REF:
            return strdup("");
        default:
            break;
        }
        String content = nodeContent(node);
        return strdup(content != null ? content : "");
    }

    // xsd10_schemas_get_ns_prop
    /* The value of the attribute name in namespace ns_uri, null if absent. */
    private static String getNsProp(XmlNode node, String name, String ns_uri)
    {
        XmlAttr a = nodeGetAttr(node, name, ns_uri);
        if (a == null)
            return null;
        return nodeStringValue(attrNode(a));
    }

    // xml_xsd10_tree_node_get_base
    /** Port of xmlNodeGetBase (xmlNodeGetBaseSafe). @param doc document or null @param cur node
        @return the base uri or null */
    static String treeNodeGetBase(XmlDocument doc, XmlNode cur)
    {
        String ret = null;
        String base, newbase;
        int[] res = new int[1];

        if ((cur == null) && (doc == null))
            return null;
        if (doc == null)
            doc = nodeDom(cur);

        while (cur != null)
        {
            if (nodeKind(cur) == ELEMENT)
            {
                base = getNsProp(cur, "base", XML_XSD10_XML_XML_NAMESPACE);
                if (base != null)
                {
                    if (ret != null)
                    {
                        res[0] = 0;
                        newbase = XmlUri.buildSafe(ret, base, res);
                        if (res[0] != 0)
                            return null;
                        ret = newbase;
                    }
                    else
                    {
                        ret = base;
                    }
                    if (ret.startsWith("http://") ||
                        ret.startsWith("ftp://") ||
                        ret.startsWith("urn:"))
                        return ret;
                }
            }
            cur = nodeParent(cur);
        }

        if ((doc != null) && (domUrl(doc) != null))
        {
            if (ret == null)
            {
                ret = strdup(domUrl(doc));
            }
            else
            {
                res[0] = 0;
                newbase = XmlUri.buildSafe(ret, domUrl(doc), res);
                if (res[0] != 0)
                    return null;
                ret = newbase;
            }
        }

        return ret;
    }

    // xml_xsd10_tree_node_get_space_preserve
    /** Port of xmlNodeGetSpacePreserve. @param cur node @return 1 preserve, 0 default, -1 unspecified */
    static int treeNodeGetSpacePreserve(XmlNode cur)
    {
        String space;

        if ((cur == null) || (nodeKind(cur) != ELEMENT))
            return -1;

        while ((cur != null) && (nodeKind(cur) == ELEMENT))
        {
            space = getNsProp(cur, "space", XML_XSD10_XML_XML_NAMESPACE);
            if (space != null)
            {
                if (strEqual(space, "preserve") != 0)
                    return 1;
                if (strEqual(space, "default") != 0)
                    return 0;
            }

            cur = nodeParent(cur);
        }

        return -1;
    }

    // xsd10_schemas_get_line_no_internal
    private static int getLineNoInternal(XmlNode node, int depth)
    {
        int result = -1;

        if (depth >= 5)
            return -1;

        if (node == null)
            return result;
        int kind = nodeKind(node);
        if ((kind == ELEMENT) ||
            (kind == TEXT) ||
            (kind == COMMENT) ||
            (kind == PI))
        {
            result = nodeLine(node);
        }
        else
        {
            XmlNode prev = nodePrev(node);
            XmlNode parent = nodeParent(node);
            int pk = prev != null ? nodeKind(prev) : 0;
            if ((prev != null) &&
                ((pk == ELEMENT) ||
                 (pk == TEXT) ||
                 (pk == COMMENT) ||
                 (pk == PI)))
                result = getLineNoInternal(prev, depth + 1);
            else if ((parent != null) &&
                     (nodeKind(parent) == ELEMENT))
                result = getLineNoInternal(parent, depth + 1);
        }

        return result;
    }

    // xml_xsd10_tree_get_line_no
    /** Port of xmlGetLineNo. @param node node @return the line or -1 */
    static int treeGetLineNo(XmlNode node)
    {
        return getLineNoInternal(node, 0);
    }

    /* ***********************************************************************
     *                                                                      *
     *          Error raising (libxml2 error.c)                             *
     *                                                                      *
     ************************************************************************/

    /** The function xml_xsd10_generic_error as a ValidityErrorFunc. vRaiseError compares the channel
        with this instance (C compares the function pointer), so pass this one, not a new method
        reference. */
    public static final ValidityErrorFunc GENERIC_ERROR = Xsd10Schemas::genericError;

    // xml_xsd10_generic_error
    /** Port of xml_xsd10_generic_error: printf to stderr. @param ctx unused @param msg format
        @param args arguments */
    public static void genericError(Object ctx, String msg, Object... args)
    {
        String s = format(msg, args);
        if (s == null) return;
        byte[] b = s.getBytes(StandardCharsets.UTF_8);
        System.err.write(b, 0, b.length);
    }

    // xml_xsd10_format_error
    /** Port of xml_xsd10_format_error (libxml2 xmlFormatError). @param err the error
        @param channel where to print @param data the channel data */
    public static void formatError(Xsd10Error err, ValidityErrorFunc channel, Object data)
    {
        String message;
        String file;
        int line;
        int code;
        int domain;
        String name = null;
        XmlNode node;
        int level;

        if ((err == null) || (channel == null))
            return;

        message = err.message;
        file = err.file;
        line = err.line;
        code = err.code;
        domain = err.domain;
        level = err.level;
        node = err.node;

        if (code == XML_XSD10_ERR_OK)
            return;

        if ((node != null) && (nodeKind(node) == ELEMENT) &&
            (domain != XML_XSD10_FROM_SCHEMASV))
            name = nodeName(node);

        /* There is never a parser context (libxml2 prints its input position here). */
        if (file != null)
            channel.error(data, "%s:%d: ", file, line);
        else if ((line != 0) &&
                 ((domain == XML_XSD10_FROM_PARSER) || (domain == XML_XSD10_FROM_SCHEMASV) ||
                  (domain == XML_XSD10_FROM_SCHEMASP)))
            channel.error(data, "Entity: line %d: ", line);
        if (name != null)
        {
            channel.error(data, "element %s: ", name);
        }
        switch (domain)
        {
        case XML_XSD10_FROM_PARSER:
            channel.error(data, "parser ");
            break;
        case XML_XSD10_FROM_MEMORY:
            channel.error(data, "memory ");
            break;
        case XML_XSD10_FROM_IO:
            channel.error(data, "I/O ");
            break;
        case XML_XSD10_FROM_SCHEMASV:
            channel.error(data, "Schemas validity ");
            break;
        case XML_XSD10_FROM_SCHEMASP:
            channel.error(data, "Schemas parser ");
            break;
        default:
            break;
        }
        switch (level)
        {
        case XML_XSD10_ERR_NONE:
            channel.error(data, ": ");
            break;
        case XML_XSD10_ERR_WARNING:
            channel.error(data, "warning : ");
            break;
        case XML_XSD10_ERR_ERROR:
            channel.error(data, "error : ");
            break;
        case XML_XSD10_ERR_FATAL:
            channel.error(data, "error : ");
            break;
        default:
            break;
        }
        if (message != null)
        {
            int len;
            len = message.length();
            if ((len > 0) && (message.charAt(len - 1) != '\n'))
                channel.error(data, "%s\n", message);
            else
                channel.error(data, "%s", message);
        }
        else
        {
            channel.error(data, "%s\n", "No error message provided");
        }
        /* libxml2 prints the context of its parser input here (xmlParserPrintFileContextInternal),
           the native errors carry it as text in ctxt when they come from a parser input. */
        if (err.ctxt != null) channel.error(data, "%s", err.ctxt.toString());
    }

    // xml_xsd10_format_error_string
    /** Port of xml_xsd10_format_error_string: formatError into a string. @param err the error
        @return the text ("" if nothing) */
    public static String formatErrorString(Xsd10Error err)
    {
        StringBuilder sc = new StringBuilder();
        // xsd10_schemas_string_channel
        formatError(err, (ctx, msg, args) ->
        {
            String part = vasprintf(Integer.MAX_VALUE, msg, args);
            if (part != null) sc.append(part);
        }, sc);
        return sc.toString();
    }

    // xml_xsd10_raise_memory_error
    /** Port of xml_xsd10_raise_memory_error. @param schannel structured channel @param channel channel
        @param data data @param domain domain */
    static void raiseMemoryError(StructuredErrorFunc schannel, ValidityErrorFunc channel, Object data, int domain)
    {
        Xsd10Error err = new Xsd10Error();

        err.domain = domain;
        err.code = XML_XSD10_ERR_NO_MEMORY;
        err.level = XML_XSD10_ERR_FATAL;

        if (schannel != null)
        {
            schannel.error(data, err);
        }
        else if (channel != null)
        {
            channel.error(data, "libxml2: out of memory\n");
        }
    }

    // xml_xsd10_v_raise_error
    /** Port of xml_xsd10_v_raise_error (libxml2 xmlVRaiseError). ctx is the context of the parser input
        as text (eg "\n^\n") for FROM_IO errors, else ignored. @param schannel structured channel
        @param channel channel @param data data @param ctx context @param node node @param domain domain
        @param code code @param level level @param file file @param line line @param str1 str1
        @param str2 str2 @param str3 str3 @param int1 int1 @param col column @param msg format
        @param ap arguments @return 0 or -1 */
    static int vRaiseError(StructuredErrorFunc schannel, ValidityErrorFunc channel, Object data, Object ctx, XmlNode node, int domain, int code, int level, String file, int line, String str1, String str2, String str3, int int1, int col, String msg, Object[] ap)
    {
        Xsd10Error err;
        String message = null;

        if (code == XML_XSD10_ERR_OK)
            return 0;

        /*
         * Find first element parent (xmlVUpdateError).
         */
        if (node != null)
        {
            int i;

            for (i = 0; i < 10; i++)
            {
                if ((nodeKind(node) == ELEMENT) ||
                    (nodeParent(node) == null))
                    break;
                node = nodeParent(node);
            }
        }

        /*
         * Get file and line from node.
         */
        if (node != null)
        {
            if ((file == null) && (nodeDom(node) != null))
                file = domUrl(nodeDom(node));

            if (line == 0)
            {
                if (nodeKind(node) == ELEMENT)
                    line = nodeLine(node);
                if (line == 0)
                    line = treeGetLineNo(node);
            }
        }

        /*
         * Formatting the message (xmlVSetError)
         */
        if (msg == null)
        {
            message = strdup("No error message provided");
        }
        else
        {
            message = vasprintf(XML_XSD10_MAX_ERR_MSG_SIZE, msg, ap);
        }
        if (message == null)
            return -1;

        err = new Xsd10Error();
        err.domain = domain;
        err.code = code;
        err.message = message;
        err.level = level;
        err.file = strdup(file);
        err.line = line;
        err.str1 = strdup(str1);
        err.str2 = strdup(str2);
        err.str3 = strdup(str3);
        err.int1 = int1;
        err.int2 = col;
        err.node = node;
        /* Only the loader passes ctx for I/O errors: the context of the parser input as text
           (the schema error functions pass their schema context, which is not exposed). */
        err.ctxt = (domain == XML_XSD10_FROM_IO) ? ctx : null;

        if (schannel != null)
        {
            schannel.error(data, err);
        }
        else if (channel != null)
        {
            /* Don't invoke legacy error handlers */
            if (channel == GENERIC_ERROR)
                formatError(err, GENERIC_ERROR, null);
            else
                channel.error(data, "%s", err.message);
        }

        return 0;
    }

    // xsd10_schemas_raise_error_ctxt
    /* Raise an error with a ... argument list (libxml2 xmlRaiseError), with the input context text. */
    private static int raiseErrorCtxt(StructuredErrorFunc schannel, ValidityErrorFunc channel, Object data,
                                      String input_context,
                                      XmlNode node, int domain, int code, int level,
                                      String file, int line, String str1, int int1, int col,
                                      String msg, Object... ap)
    {
        return vRaiseError(schannel, channel, data, input_context, node, domain, code,
                           level, file, line, str1, null, null, int1, col, msg, ap);
    }

    // xsd10_schemas_raise_error
    /* Raise an error with a ... argument list (libxml2 xmlRaiseError). */
    private static int raiseError(StructuredErrorFunc schannel, ValidityErrorFunc channel, Object data,
                                  XmlNode node, int domain, int code, int level,
                                  String file, int line, String str1, int int1, int col,
                                  String msg, Object... ap)
    {
        return vRaiseError(schannel, channel, data, null, node, domain, code,
                           level, file, line, str1, null, null, int1, col, msg, ap);
    }

    /* ***********************************************************************
     *                                                                      *
     *          Regexps and loading schema documents                        *
     *                                                                      *
     ************************************************************************/

    // xml_xsd10_install_regexp_hooks (the static const hooks)
    private static final Xsd10RegexpHooks REGEXP_HOOKS = new Xsd10RegexpHooks();
    static
    {
        // xsd10_schemas_regexp_compile
        REGEXP_HOOKS.compile = regexp -> XmlRegexp.regexpCompile(regexp);
        // xsd10_schemas_regexp_exec
        REGEXP_HOOKS.exec = (regexp, value) -> XmlRegexp.regexpExec((XmlRegexp) regexp, value);
        // xsd10_schemas_regexp_free
        REGEXP_HOOKS.free = regexp ->
        {
            if (regexp instanceof XmlRegexp r) XmlRegexp.regexpFree(r);
        };
    }

    // xml_xsd10_install_regexp_hooks
    /** Port of xml_xsd10_install_regexp_hooks. */
    static void installRegexpHooks()
    {
        setRegexpHooks(REGEXP_HOOKS);
    }

    /* Port of xmlIOErr (errno to the libxml2 XML_IO_* codes, which start at 1501 for EACCES)
       and the messages of xmlErrString: { glibc strerror text, code, libxml2 message }. */
    private static final Object[][] IO_ERRS = {
        { "Permission denied", 1501, "Permission denied" },
        { "Resource temporarily unavailable", 1502, "Resource temporarily unavailable" },
        { "Bad file descriptor", 1503, "Bad file descriptor" },
        { "Bad message", 1504, "Bad message" },
        { "Device or resource busy", 1505, "Resource busy" },
        { "Operation canceled", 1506, "Operation canceled" },
        { "No child processes", 1507, "No child processes" },
        { "Resource deadlock avoided", 1508, "Resource deadlock avoided" },
        { "Numerical argument out of domain", 1509, "Domain error" },
        { "File exists", 1510, "File exists" },
        { "Bad address", 1511, "Bad address" },
        { "File too large", 1512, "File too large" },
        { "Operation now in progress", 1513, "Operation in progress" },
        { "Interrupted system call", 1514, "Interrupted function call" },
        { "Invalid argument", 1515, "Invalid argument" },
        { "Input/output error", 1516, "Input/output error" },
        { "Is a directory", 1517, "Is a directory" },
        { "Too many open files", 1518, "Too many open files" },
        { "Too many links", 1519, "Too many links" },
        { "Message too long", 1520, "Inappropriate message buffer length" },
        { "File name too long", 1521, "Filename too long" },
        { "Too many open files in system", 1522, "Too many open files in system" },
        { "No such device", 1523, "No such device" },
        { "No such file or directory", 1524, "No such file or directory" },
        { "Exec format error", 1525, "Exec format error" },
        { "No locks available", 1526, "No locks available" },
        { "Cannot allocate memory", 1527, "Not enough space" },
        { "No space left on device", 1528, "No space left on device" },
        { "Function not implemented", 1529, "Function not implemented" },
        { "Not a directory", 1530, "Not a directory" },
        { "Directory not empty", 1531, "Directory not empty" },
        { "Operation not supported", 1532, "Not supported" },
        { "Inappropriate ioctl for device", 1533, "Inappropriate I/O control operation" },
        { "No such device or address", 1534, "No such device or address" },
        { "Operation not permitted", 1535, "Operation not permitted" },
        { "Broken pipe", 1536, "Broken pipe" },
        { "Numerical result out of range", 1537, "Result too large" },
        { "Read-only file system", 1538, "Read-only file system" },
        { "Illegal seek", 1539, "Invalid seek" },
        { "No such process", 1540, "No such process" },
        { "Connection timed out", 1541, "Operation timed out" },
        { "Invalid cross-device link", 1542, "Improper link" },
    };

    /* The glibc strerror text of a failed file access (Java has no errno), null if unknown. */
    private static String ioErrno(IOException e)
    {
        if (e instanceof NoSuchFileException) return "No such file or directory";
        if (e instanceof AccessDeniedException) return "Permission denied";
        if (e instanceof NotDirectoryException) return "Not a directory";
        if (e instanceof FileSystemException fe && fe.getReason() != null) return fe.getReason();
        return null;
    }

    // xsd10_schemas_read_file
    /* Read a whole file, null on errors (then errno[0] is the glibc strerror text of the error, or
       null for an unknown error). read_failed[0] is set when the file could be opened but not read
       (eg a directory): libxml2 reports that from the parser input instead of from the loader. */
    private static byte[] readFile(String path, String[] errno, int[] read_failed)
    {
        read_failed[0] = 0;
        errno[0] = null;
        Path p;
        try
        {
            if (path.isEmpty() || path.indexOf('\0') >= 0)
            {
                errno[0] = "No such file or directory";
                return null;
            }
            p = Path.of(path);
        }
        catch (RuntimeException e)
        {
            errno[0] = "No such file or directory";
            return null;
        }
        if (Files.isDirectory(p))
        {
            // fopen succeeds on a readable directory, fread fails with EISDIR.
            if (!Files.isReadable(p))
            {
                errno[0] = "Permission denied";
                return null;
            }
            read_failed[0] = 1;
            errno[0] = "Is a directory";
            return null;
        }
        try
        {
            return Files.readAllBytes(p);
        }
        catch (IOException e)
        {
            errno[0] = ioErrno(e);
            return null;
        }
        catch (RuntimeException | OutOfMemoryError e)
        {
            errno[0] = "Cannot allocate memory";
            return null;
        }
    }

    // Xsd10SchemasLoadReport
    /* Where the problems of loading a schema document go: the structured callback of the
       parser context, else stderr (libxml2 reports the xml parser errors to the parser
       context of the schema document, which only gets the structured callback). */
    private static final class LoadReport
    {
        StructuredErrorFunc serror;
        Object data;
    }

    /* Write bytes to stderr unchanged (C fputs). */
    private static void fputsStderr(byte[] b, int off)
    {
        System.err.write(b, off, b.length - off);
    }

    // xsd10_schemas_report_parse_error
    private static void reportParseError(LoadReport rep, String uri, XmlParseException perr)
    {
        if (perr == null) return;
        if ((rep != null) && (rep.serror != null))
        {
            /* The Java parser does not report the column, int2 is 0. */
            raiseError(rep.serror, null, rep.data, null, XML_XSD10_FROM_PARSER,
                       XML_XSD10_ERR_INTERNAL_ERROR, XML_XSD10_ERR_FATAL, uri, perr.line,
                       null, 0, 0, "%s",
                       perr.error != null ? perr.error : perr.getMessage());
        }
        else
        {
            byte[] m = perr.bytes;
            byte[] p = "noname.xml:".getBytes(StandardCharsets.US_ASCII);
            boolean noname = (uri == null) && (m.length >= 11);
            for (int i = 0; noname && i < 11; i++)
            {
                if (m[i] != p[i]) noname = false;
            }
            if (noname)
            {
                /* libxml2 has no file name for a memory buffer: "Entity: line 1: parser error : ..." */
                fputsStderr("Entity: line ".getBytes(StandardCharsets.US_ASCII), 0);
                fputsStderr(m, 11);
            }
            else
            {
                fputsStderr(m, 0);
            }
        }
    }

    // xsd10_schemas_parse
    /* Parse the buffer like xmlCtxtReadMemory with XML_PARSE_NOENT (the default parse settings of
       xml_parse_settings_default: no NOBLANKS, no NOCDATA, no DTDATTR). */
    private static XmlDocument schemasParse(byte[] buf, String uri, LoadReport rep)
    {
        XmlDocument dom;
        // xsd10_schemas_parse_message: null lets the parser print to stderr (C fputs).
        java.util.function.Consumer<String> messages = null;
        if ((rep != null) && (rep.serror != null))
        {
            messages = message -> raiseError(rep.serror, null, rep.data, null, XML_XSD10_FROM_PARSER,
                                             XML_XSD10_ERR_INTERNAL_ERROR, XML_XSD10_ERR_WARNING, null, 0,
                                             null, 0, 0, "%s", message);
        }
        try
        {
            dom = XmlParser.parse(buf, uri, 0, messages);
        }
        catch (XmlParseException e)
        {
            reportParseError(rep, uri, e);
            dom = null;
        }
        if (dom != null)
        {
            /* XML_PARSE_NOENT: entities are substituted. */
            try
            {
                XmlEntityExpander.expand(dom, null, buf.length, 0);
            }
            catch (XmlParseException e)
            {
                reportParseError(rep, uri, e);
                dom = null;
            }
        }
        return dom;
    }

    // xsd10_schemas_load_file
    /* Load uri like xmlCtxtReadFile with XML_PARSE_NOENT. notLocated[0] is 1 if the file could not be read. */
    private static XmlDocument loadFile(String uri, LoadReport rep, int[] notLocated)
    {
        String path = uri;
        byte[] buf;
        XmlDocument dom;

        if (notLocated != null) notLocated[0] = 0;
        if (uri == null) return null;
        if (path.startsWith("file://"))
        {
            path = path.substring(7);
            if (path.startsWith("localhost/")) path = path.substring(9);
        }
        int[] read_failed = { 0 };
        String[] errno = { null };
        buf = readFile(path, errno, read_failed);
        if (buf == null)
        {
            /* Port of xmlIOErr and xmlCtxtErrIO: only a warning if the file could not be found
               (or the error is unknown). */
            int code = XML_XSD10_IO_UNKNOWN;
            int level;
            String errstr = "Unknown IO error";
            for (Object[] k : IO_ERRS)
            {
                if (k[0].equals(errno[0]))
                {
                    code = (Integer) k[1];
                    errstr = (String) k[2];
                    break;
                }
            }
            if (code == XML_XSD10_IO_ENOENT || code == XML_XSD10_IO_UNKNOWN) level = XML_XSD10_ERR_WARNING;
            else level = XML_XSD10_ERR_FATAL;
            if (read_failed[0] != 0)
            {
                /* The file was opened, reading it failed in the parser input: libxml2 reports the
                   error without the uri (msg "%s\n"), at line 1 column 1 of the input. */
                /* The context of the (empty) input: an empty line and the pointer. */
                if ((rep != null) && (rep.serror != null))
                    raiseErrorCtxt(rep.serror, null, rep.data, "\n^\n", null, XML_XSD10_FROM_IO, code, level,
                                   uri, 1, null, 0, 1, "%s\n", errstr);
                else
                    raiseErrorCtxt(null, GENERIC_ERROR, null, "\n^\n", null, XML_XSD10_FROM_IO, code, level,
                                   uri, 1, null, 0, 1, "%s\n", errstr);
                if (notLocated != null) notLocated[0] = 1;
                return null;
            }
            if ((rep != null) && (rep.serror != null))
                raiseError(rep.serror, null, rep.data, null, XML_XSD10_FROM_IO, code, level,
                           null, 0, uri, 0, 0, "failed to load \"%s\": %s\n", uri, errstr);
            else
                raiseError(null, GENERIC_ERROR, null, null, XML_XSD10_FROM_IO, code, level,
                           null, 0, uri, 0, 0, "failed to load \"%s\": %s\n", uri, errstr);
            if (notLocated != null) notLocated[0] = 1;
            return null;
        }
        dom = schemasParse(buf, uri, rep);
        if (dom != null)
            domSetUrl(dom, uri);
        return dom;
    }

    // xml_xsd10_default_loader
    /** Port of xml_xsd10_default_loader: read and parse the file uri, problems go to stderr.
        @param user_data unused @param uri the uri or path @return the document or null */
    public static XmlDocument defaultLoader(Object user_data, String uri)
    {
        return loadFile(uri, null, null);
    }

    // xml_xsd10_p_read_file
    /** Port of xml_xsd10_p_read_file. @param pctxt parser context @param url url
        @param notLocated out: 1 if not found, may be null @return the document or null */
    static XmlDocument pReadFile(Xsd10ParserCtxt pctxt, String url, int[] notLocated)
    {
        LoadReport rep = new LoadReport();
        XmlDocument dom;

        if (notLocated != null) notLocated[0] = 0;
        if (url == null) return null;
        if ((pctxt != null) && (pctxt.resourceLoader != null))
        {
            dom = pctxt.resourceLoader.load(pctxt.resourceCtxt, url);
            /* A custom loader cannot tell why it failed, assume the resource was not found. */
            if ((dom == null) && (notLocated != null)) notLocated[0] = 1;
            if ((dom != null) && (domUrl(dom) == null)) domSetUrl(dom, url);
            return dom;
        }
        rep.serror = pctxt != null ? pctxt.serror : null;
        rep.data = pctxt != null ? pctxt.errCtxt : null;
        return loadFile(url, rep, notLocated);
    }

    // xml_xsd10_p_read_memory
    /** Port of xml_xsd10_p_read_memory: parse the first size UTF-8 bytes of buffer.
        @param pctxt parser context @param buffer the text @param size its size in bytes
        @param notLocated out, may be null @return the document or null */
    static XmlDocument pReadMemory(Xsd10ParserCtxt pctxt, String buffer, int size, int[] notLocated)
    {
        LoadReport rep = new LoadReport();
        XmlDocument dom;

        if (notLocated != null) notLocated[0] = 0;
        if ((buffer == null) || (size < 0)) return null;
        rep.serror = pctxt != null ? pctxt.serror : null;
        rep.data = pctxt != null ? pctxt.errCtxt : null;
        byte[] b = buffer.getBytes(StandardCharsets.UTF_8);
        if (size < b.length) b = Arrays.copyOf(b, size);
        dom = schemasParse(b, null, rep);
        if (dom != null)
            domSetUrl(dom, "in_memory_buffer");
        return dom;
    }

    // xsd10_schemas_ns_href
    /* The namespace uri of an element or attribute, null if none (libxml2 node->ns->href when node->ns != NULL). */
    private static String nsHrefOf(XmlNode node)
    {
        return nodeNsUri(node);
    }

    /* ***********************************************************************
     *                                                                      *
     *                  Helper functions                                    *
     *                                                                      *
     ************************************************************************/

    // xml_xsd10_item_type_to_str
    /** Port of xmlSchemaItemTypeToStr. @param type the type of the schema item
        @return the component name of a schema item */
    static String itemTypeToStr(int type)
    {
        switch (type)
        {
        case XML_XSD10_SCHEMA_TYPE_BASIC:
            return "simple type definition";
        case XML_XSD10_SCHEMA_TYPE_SIMPLE:
            return "simple type definition";
        case XML_XSD10_SCHEMA_TYPE_COMPLEX:
            return "complex type definition";
        case XML_XSD10_SCHEMA_TYPE_ELEMENT:
            return "element declaration";
        case XML_XSD10_SCHEMA_TYPE_ATTRIBUTE_USE:
            return "attribute use";
        case XML_XSD10_SCHEMA_TYPE_ATTRIBUTE:
            return "attribute declaration";
        case XML_XSD10_SCHEMA_TYPE_GROUP:
            return "model group definition";
        case XML_XSD10_SCHEMA_TYPE_ATTRIBUTEGROUP:
            return "attribute group definition";
        case XML_XSD10_SCHEMA_TYPE_NOTATION:
            return "notation declaration";
        case XML_XSD10_SCHEMA_TYPE_SEQUENCE:
            return "model group (sequence)";
        case XML_XSD10_SCHEMA_TYPE_CHOICE:
            return "model group (choice)";
        case XML_XSD10_SCHEMA_TYPE_ALL:
            return "model group (all)";
        case XML_XSD10_SCHEMA_TYPE_PARTICLE:
            return "particle";
        case XML_XSD10_SCHEMA_TYPE_IDC_UNIQUE:
            return "unique identity-constraint";
            /* return("IDC (unique)"); */
        case XML_XSD10_SCHEMA_TYPE_IDC_KEY:
            return "key identity-constraint";
            /* return("IDC (key)"); */
        case XML_XSD10_SCHEMA_TYPE_IDC_KEYREF:
            return "keyref identity-constraint";
            /* return("IDC (keyref)"); */
        case XML_XSD10_SCHEMA_TYPE_ANY:
            return "wildcard (any)";
        case XML_XSD10_SCHEMA_EXTRA_QNAMEREF:
            return "[helper component] QName reference";
        case XML_XSD10_SCHEMA_EXTRA_ATTR_USE_PROHIB:
            return "[helper component] attribute use prohibition";
        default:
            return "Not a schema component";
        }
    }

    // xml_xsd10_get_component_type_str
    /** Port of xmlSchemaGetComponentTypeStr. @param item the item @return the component name */
    static String getComponentTypeStr(Xsd10BasicItem item)
    {
        switch (item.type)
        {
        case XML_XSD10_SCHEMA_TYPE_BASIC:
            if ((item instanceof Xsd10Type t) && (wxsIsComplex(t) != 0))
                return "complex type definition";
            else
                return "simple type definition";
        default:
            return itemTypeToStr(item.type);
        }
    }

    // xml_xsd10_get_component_node
    /** Port of xmlSchemaGetComponentNode. NOTE that such a node need not be available; plus, a
        component's node need not to reflect the component directly, since there is no one-to-one
        relationship between the XML Schema representation and the component representation.
        @param item a schema component @return node associated with the schema component */
    static XmlNode getComponentNode(Xsd10BasicItem item)
    {
        switch (item.type)
        {
        case XML_XSD10_SCHEMA_TYPE_ELEMENT:
            return (item instanceof Xsd10Element e) ? e.node : null;
        case XML_XSD10_SCHEMA_TYPE_ATTRIBUTE:
            return (item instanceof Xsd10Attribute a) ? a.node : null;
        case XML_XSD10_SCHEMA_TYPE_COMPLEX:
        case XML_XSD10_SCHEMA_TYPE_SIMPLE:
            return (item instanceof Xsd10Type t) ? t.node : null;
        case XML_XSD10_SCHEMA_TYPE_ANY:
        case XML_XSD10_SCHEMA_TYPE_ANY_ATTRIBUTE:
            return (item instanceof Xsd10Wildcard w) ? w.node : null;
        case XML_XSD10_SCHEMA_TYPE_PARTICLE:
            return (item instanceof Xsd10Particle p) ? p.node : null;
        case XML_XSD10_SCHEMA_TYPE_SEQUENCE:
        case XML_XSD10_SCHEMA_TYPE_CHOICE:
        case XML_XSD10_SCHEMA_TYPE_ALL:
            return (item instanceof Xsd10ModelGroup m) ? m.node : null;
        case XML_XSD10_SCHEMA_TYPE_GROUP:
            return (item instanceof Xsd10ModelGroupDef g) ? g.node : null;
        case XML_XSD10_SCHEMA_TYPE_ATTRIBUTEGROUP:
            return (item instanceof Xsd10AttributeGroup g) ? g.node : null;
        case XML_XSD10_SCHEMA_TYPE_IDC_UNIQUE:
        case XML_XSD10_SCHEMA_TYPE_IDC_KEY:
        case XML_XSD10_SCHEMA_TYPE_IDC_KEYREF:
            return (item instanceof Xsd10IDC i) ? i.node : null;
        case XML_XSD10_SCHEMA_EXTRA_QNAMEREF:
            return (item instanceof Xsd10QNameRef q) ? q.node : null;
        /* TODO: What to do with NOTATIONs?
        case XML_XSD10_SCHEMA_TYPE_NOTATION:
            return (((XmlXsd10Notation *) item)->node);
        */
        case XML_XSD10_SCHEMA_TYPE_ATTRIBUTE_USE:
            return (item instanceof Xsd10AttributeUse u) ? u.node : null;
        default:
            return null;
        }
    }

    // xml_xsd10_format_q_name
    /** Port of xmlSchemaFormatQName: "{namespaceName}localName" or just "localName" if namespaceName
        is null. @param buf the string buffer @param namespaceName the namespace name
        @param localName the local name @return the localName if namespaceName is null, a formatted
        string otherwise */
    static String formatQName(String[] buf, String namespaceName, String localName)
    {
        buf[0] = null;
        if (namespaceName != null)
        {
            buf[0] = strdup("{");
            buf[0] = strcat(buf[0], namespaceName);
            buf[0] = strcat(buf[0], "}");
        }
        if (localName != null)
        {
            if (namespaceName == null)
                return localName;
            buf[0] = strcat(buf[0], localName);
        }
        else
        {
            buf[0] = strcat(buf[0], "(NULL)");
        }
        return buf[0];
    }

    // xml_xsd10_format_q_name_ns
    /** Port of xmlSchemaFormatQNameNs. @param buf buffer @param ns ns @param localName local name
        @return the formatted name */
    static String formatQNameNs(String[] buf, XmlNs ns, String localName)
    {
        if (ns != null)
            return formatQName(buf, nsHref(ns), localName);
        else
            return formatQName(buf, null, localName);
    }

    // xml_xsd10_get_component_name
    /** Port of xmlSchemaGetComponentName. @param item item @return the name or null */
    static String getComponentName(Xsd10BasicItem item)
    {
        if (item == null)
        {
            return null;
        }
        switch (item.type)
        {
        case XML_XSD10_SCHEMA_TYPE_ELEMENT:
            return (item instanceof Xsd10Element e) ? e.name : null;
        case XML_XSD10_SCHEMA_TYPE_ATTRIBUTE:
            return (item instanceof Xsd10Attribute a) ? a.name : null;
        case XML_XSD10_SCHEMA_TYPE_ATTRIBUTEGROUP:
            return (item instanceof Xsd10AttributeGroup g) ? g.name : null;
        case XML_XSD10_SCHEMA_TYPE_BASIC:
        case XML_XSD10_SCHEMA_TYPE_SIMPLE:
        case XML_XSD10_SCHEMA_TYPE_COMPLEX:
            return (item instanceof Xsd10Type t) ? t.name : null;
        case XML_XSD10_SCHEMA_TYPE_GROUP:
            return (item instanceof Xsd10ModelGroupDef g) ? g.name : null;
        case XML_XSD10_SCHEMA_TYPE_IDC_KEY:
        case XML_XSD10_SCHEMA_TYPE_IDC_UNIQUE:
        case XML_XSD10_SCHEMA_TYPE_IDC_KEYREF:
            return (item instanceof Xsd10IDC i) ? i.name : null;
        case XML_XSD10_SCHEMA_TYPE_ATTRIBUTE_USE:
            if ((item instanceof Xsd10AttributeUse) && (wxsAttruseDecl(item) != null))
            {
                return getComponentName(wxsAttruseDecl(item));
            }
            else
                return null;
        case XML_XSD10_SCHEMA_EXTRA_QNAMEREF:
            return (item instanceof Xsd10QNameRef q) ? q.name : null;
        case XML_XSD10_SCHEMA_TYPE_NOTATION:
            return (item instanceof Xsd10Notation n) ? n.name : null;
        default:
            /*
             * Other components cannot have names.
             */
            break;
        }
        return null;
    }

    // xml_xsd10_get_component_target_ns
    /** Port of xmlSchemaGetComponentTargetNs. @param item item @return the namespace or null */
    static String getComponentTargetNs(Xsd10BasicItem item)
    {
        if (item == null)
        {
            return null;
        }
        switch (item.type)
        {
        case XML_XSD10_SCHEMA_TYPE_ELEMENT:
            return (item instanceof Xsd10Element e) ? e.targetNamespace : null;
        case XML_XSD10_SCHEMA_TYPE_ATTRIBUTE:
            return (item instanceof Xsd10Attribute a) ? a.targetNamespace : null;
        case XML_XSD10_SCHEMA_TYPE_ATTRIBUTEGROUP:
            return (item instanceof Xsd10AttributeGroup g) ? g.targetNamespace : null;
        case XML_XSD10_SCHEMA_TYPE_BASIC:
            return "http://www.w3.org/2001/XMLSchema";
        case XML_XSD10_SCHEMA_TYPE_SIMPLE:
        case XML_XSD10_SCHEMA_TYPE_COMPLEX:
            return (item instanceof Xsd10Type t) ? t.targetNamespace : null;
        case XML_XSD10_SCHEMA_TYPE_GROUP:
            return (item instanceof Xsd10ModelGroupDef g) ? g.targetNamespace : null;
        case XML_XSD10_SCHEMA_TYPE_IDC_KEY:
        case XML_XSD10_SCHEMA_TYPE_IDC_UNIQUE:
        case XML_XSD10_SCHEMA_TYPE_IDC_KEYREF:
            return (item instanceof Xsd10IDC i) ? i.targetNamespace : null;
        case XML_XSD10_SCHEMA_TYPE_ATTRIBUTE_USE:
            if ((item instanceof Xsd10AttributeUse) && (wxsAttruseDecl(item) != null))
            {
                return getComponentTargetNs(wxsAttruseDecl(item));
            }
            /* TODO: Will returning NULL break something? */
            break;
        case XML_XSD10_SCHEMA_EXTRA_QNAMEREF:
            return (item instanceof Xsd10QNameRef q) ? q.targetNamespace : null;
        case XML_XSD10_SCHEMA_TYPE_NOTATION:
            return (item instanceof Xsd10Notation n) ? n.targetNamespace : null;
        default:
            /*
             * Other components cannot have names.
             */
            break;
        }
        return null;
    }

    // xml_xsd10_get_component_q_name
    /** Port of xmlSchemaGetComponentQName. @param buf buffer @param item item @return the qname */
    static String getComponentQName(String[] buf, Object item)
    {
        return formatQName(buf,
                           getComponentTargetNs((Xsd10BasicItem) item),
                           getComponentName((Xsd10BasicItem) item));
    }

    // xml_xsd10_get_component_designation
    /** Port of xmlSchemaGetComponentDesignation (appends to buf). @param buf buffer @param item item
        @return buf[0] */
    static String getComponentDesignation(String[] buf, Object item)
    {
        String[] str = { null };

        buf[0] = strcat(buf[0], wxsItemTypeName((Xsd10BasicItem) item));
        buf[0] = strcat(buf[0], " '");
        buf[0] = strcat(buf[0], getComponentQName(str, item));
        buf[0] = strcat(buf[0], "'");
        return buf[0];
    }

    // xml_xsd10_get_idc_designation
    /** Port of xmlSchemaGetIDCDesignation. @param buf buffer @param idc idc @return buf[0] */
    static String getIdcDesignation(String[] buf, Xsd10IDC idc)
    {
        return getComponentDesignation(buf, idc);
    }

    // xml_xsd10_wildcard_pc_to_string
    /** Port of xmlSchemaWildcardPCToString. @param pc the type of processContents
        @return a string representation of the type of processContents */
    static String wildcardPcToString(int pc)
    {
        switch (pc)
        {
        case XML_XSD10_SCHEMAS_ANY_SKIP:
            return "skip";
        case XML_XSD10_SCHEMAS_ANY_LAX:
            return "lax";
        case XML_XSD10_SCHEMAS_ANY_STRICT:
            return "strict";
        default:
            return "invalid process contents";
        }
    }

    // xml_xsd10_get_canon_value_whtsp_ext_1
    /** Port of xmlSchemaGetCanonValueWhtspExt_1: the canonical representation of the value.
        @param val the precomputed value @param ws the whitespace type of the value
        @param retValue out: the value @param for_hash non-zero if this is supposed to generate a
        string for hashing @return 0 if the value could be built and -1 in case of API errors or if
        the value type is not supported yet */
    static int getCanonValueWhtspExt1(Xsd10Val val, int ws, String[] retValue, int for_hash)
    {
        boolean list;
        int valType;
        String value;
        String[] value2 = { null };

        if ((retValue == null) || (val == null))
            return -1;
        list = valueGetNext(val) != null;
        retValue[0] = null;
        do
        {
            value = null;
            valType = getValType(val);
            switch (valType)
            {
            case XML_XSD10_SCHEMAS_STRING:
            case XML_XSD10_SCHEMAS_NORMSTRING:
            case XML_XSD10_SCHEMAS_ANYSIMPLETYPE:
                value = valueGetAsString(val);
                if (value != null)
                {
                    if (ws == XML_XSD10_SCHEMA_WHITESPACE_COLLAPSE)
                        value2[0] = collapseString(value);
                    else if (ws == XML_XSD10_SCHEMA_WHITESPACE_REPLACE)
                        value2[0] = whiteSpaceReplace(value);
                    if (value2[0] != null)
                        value = value2[0];
                }
                break;
            default:
                if (getCanonValue(val, value2) == -1)
                {
                    retValue[0] = null;
                    return -1;
                }
                if ((for_hash != 0) && valType == XML_XSD10_SCHEMAS_DECIMAL)
                {
                    /* We can mostly use the canonical value for hashing,
                       except in the case of decimal.  There the canonical
                       representation requires a trailing '.0' even for
                       non-fractional numbers, but for the derived integer
                       types it forbids any decimal point.  Nevertheless they
                       compare equal if the value is equal.  We need to generate
                       the same hash value for this to work, and it's easiest
                       to just cut off the useless '.0' suffix for the
                       decimal type.  */
                    String v2 = value2[0];
                    int len = v2 == null ? 0 : v2.length();
                    if (len > 2 && v2.charAt(len - 1) == '0' && v2.charAt(len - 2) == '.')
                        value2[0] = v2.substring(0, len - 2);
                }
                value = value2[0];
            }
            if (retValue[0] == null)
            {
                if (value == null)
                {
                    if (!list)
                        retValue[0] = strdup("");
                }
                else
                    retValue[0] = strdup(value);
            }
            else if (value != null)
            {
                /* List. */
                retValue[0] = strcat(retValue[0], " ");
                retValue[0] = strcat(retValue[0], value);
            }
            value2[0] = null;
            val = valueGetNext(val);
        } while (val != null);

        return 0;
    }

    // xml_xsd10_get_canon_value_whtsp_ext
    /** Port of xmlSchemaGetCanonValueWhtspExt. @param val value @param ws whitespace
        @param retValue out @return 0 or -1 */
    static int getCanonValueWhtspExt(Xsd10Val val, int ws, String[] retValue)
    {
        return getCanonValueWhtspExt1(val, ws, retValue, 0);
    }

    // xml_xsd10_get_canon_value_hash
    /** Port of xmlSchemaGetCanonValueHash. @param val value @param retValue out @return 0 or -1 */
    static int getCanonValueHash(Xsd10Val val, String[] retValue)
    {
        return getCanonValueWhtspExt1(val, XML_XSD10_SCHEMA_WHITESPACE_COLLAPSE,
                                      retValue, 1);
    }

    // xml_xsd10_format_item_for_report
    /** Port of xmlSchemaFormatItemForReport. The following order is used to build the resulting
        designation if the arguments are not null: 1a. If itemDes not null -&gt; itemDes; 2. If the
        preceding was null and (item not null) -&gt; item; 3. If the preceding was null and (itemNode
        not null) -&gt; itemNode. If the itemNode is an attribute node, the name of the attribute will
        be appended to the result.
        @param buf the string buffer @param itemDes the designation of the item @param item the item
        @param itemNode the node of the item @return the formatted (escaped) string, also in buf[0] */
    static String formatItemForReport(String[] buf, String itemDes, Xsd10BasicItem item, XmlNode itemNode)
    {
        String[] str = { null };
        int named = 1;

        buf[0] = null;

        if (itemDes != null)
        {
            buf[0] = strdup(itemDes);
        }
        else if (item != null)
        {
            switch (item.type)
            {
            case XML_XSD10_SCHEMA_TYPE_BASIC:
            {
                Xsd10Type type = (Xsd10Type) item;

                if (wxsIsAtomic(type) != 0)
                    buf[0] = strdup("atomic type 'xs:");
                else if (wxsIsList(type) != 0)
                    buf[0] = strdup("list type 'xs:");
                else if (wxsIsUnion(type) != 0)
                    buf[0] = strdup("union type 'xs:");
                else
                    buf[0] = strdup("simple type 'xs:");
                buf[0] = strcat(buf[0], type.name);
                buf[0] = strcat(buf[0], "'");
            }
            break;
            case XML_XSD10_SCHEMA_TYPE_SIMPLE:
            {
                Xsd10Type type = (Xsd10Type) item;

                if ((type.flags & XML_XSD10_SCHEMAS_TYPE_GLOBAL) != 0)
                {
                    buf[0] = strdup("");
                }
                else
                {
                    buf[0] = strdup("local ");
                }
                if (wxsIsAtomic(type) != 0)
                    buf[0] = strcat(buf[0], "atomic type");
                else if (wxsIsList(type) != 0)
                    buf[0] = strcat(buf[0], "list type");
                else if (wxsIsUnion(type) != 0)
                    buf[0] = strcat(buf[0], "union type");
                else
                    buf[0] = strcat(buf[0], "simple type");
                if ((type.flags & XML_XSD10_SCHEMAS_TYPE_GLOBAL) != 0)
                {
                    buf[0] = strcat(buf[0], " '");
                    buf[0] = strcat(buf[0], type.name);
                    buf[0] = strcat(buf[0], "'");
                }
            }
            break;
            case XML_XSD10_SCHEMA_TYPE_COMPLEX:
            {
                Xsd10Type type = (Xsd10Type) item;

                if ((type.flags & XML_XSD10_SCHEMAS_TYPE_GLOBAL) != 0)
                    buf[0] = strdup("");
                else
                    buf[0] = strdup("local ");
                buf[0] = strcat(buf[0], "complex type");
                if ((type.flags & XML_XSD10_SCHEMAS_TYPE_GLOBAL) != 0)
                {
                    buf[0] = strcat(buf[0], " '");
                    buf[0] = strcat(buf[0], type.name);
                    buf[0] = strcat(buf[0], "'");
                }
            }
            break;
            case XML_XSD10_SCHEMA_TYPE_ATTRIBUTE_USE:
            {
                Xsd10AttributeUse ause;

                ause = (Xsd10AttributeUse) item;
                buf[0] = strdup("attribute use ");
                if (wxsAttruseDecl(ause) != null)
                {
                    buf[0] = strcat(buf[0], "'");
                    buf[0] = strcat(buf[0], getComponentQName(str, wxsAttruseDecl(ause)));
                    str[0] = null;
                    buf[0] = strcat(buf[0], "'");
                }
                else
                {
                    buf[0] = strcat(buf[0], "(unknown)");
                }
            }
            break;
            case XML_XSD10_SCHEMA_TYPE_ATTRIBUTE:
            {
                Xsd10Attribute attr;

                attr = (Xsd10Attribute) item;
                buf[0] = strdup("attribute decl.");
                buf[0] = strcat(buf[0], " '");
                buf[0] = strcat(buf[0], formatQName(str, attr.targetNamespace, attr.name));
                str[0] = null;
                buf[0] = strcat(buf[0], "'");
            }
            break;
            case XML_XSD10_SCHEMA_TYPE_ATTRIBUTEGROUP:
                getComponentDesignation(buf, item);
                break;
            case XML_XSD10_SCHEMA_TYPE_ELEMENT:
            {
                Xsd10Element elem;

                elem = (Xsd10Element) item;
                buf[0] = strdup("element decl.");
                buf[0] = strcat(buf[0], " '");
                buf[0] = strcat(buf[0], formatQName(str, elem.targetNamespace, elem.name));
                buf[0] = strcat(buf[0], "'");
            }
            break;
            case XML_XSD10_SCHEMA_TYPE_IDC_UNIQUE:
            case XML_XSD10_SCHEMA_TYPE_IDC_KEY:
            case XML_XSD10_SCHEMA_TYPE_IDC_KEYREF:
                if (item.type == XML_XSD10_SCHEMA_TYPE_IDC_UNIQUE)
                    buf[0] = strdup("unique '");
                else if (item.type == XML_XSD10_SCHEMA_TYPE_IDC_KEY)
                    buf[0] = strdup("key '");
                else
                    buf[0] = strdup("keyRef '");
                buf[0] = strcat(buf[0], ((Xsd10IDC) item).name);
                buf[0] = strcat(buf[0], "'");
                break;
            case XML_XSD10_SCHEMA_TYPE_ANY:
            case XML_XSD10_SCHEMA_TYPE_ANY_ATTRIBUTE:
                buf[0] = strdup(wildcardPcToString(((Xsd10Wildcard) item).processContents));
                buf[0] = strcat(buf[0], " wildcard");
                break;
            case XML_XSD10_SCHEMA_FACET_MININCLUSIVE:
            case XML_XSD10_SCHEMA_FACET_MINEXCLUSIVE:
            case XML_XSD10_SCHEMA_FACET_MAXINCLUSIVE:
            case XML_XSD10_SCHEMA_FACET_MAXEXCLUSIVE:
            case XML_XSD10_SCHEMA_FACET_TOTALDIGITS:
            case XML_XSD10_SCHEMA_FACET_FRACTIONDIGITS:
            case XML_XSD10_SCHEMA_FACET_PATTERN:
            case XML_XSD10_SCHEMA_FACET_ENUMERATION:
            case XML_XSD10_SCHEMA_FACET_WHITESPACE:
            case XML_XSD10_SCHEMA_FACET_LENGTH:
            case XML_XSD10_SCHEMA_FACET_MAXLENGTH:
            case XML_XSD10_SCHEMA_FACET_MINLENGTH:
                buf[0] = strdup("facet '");
                buf[0] = strcat(buf[0], facetTypeToString(item.type));
                buf[0] = strcat(buf[0], "'");
                break;
            case XML_XSD10_SCHEMA_TYPE_GROUP:
            {
                buf[0] = strdup("model group def.");
                buf[0] = strcat(buf[0], " '");
                buf[0] = strcat(buf[0], getComponentQName(str, item));
                buf[0] = strcat(buf[0], "'");
                str[0] = null;
            }
            break;
            case XML_XSD10_SCHEMA_TYPE_SEQUENCE:
            case XML_XSD10_SCHEMA_TYPE_CHOICE:
            case XML_XSD10_SCHEMA_TYPE_ALL:
            case XML_XSD10_SCHEMA_TYPE_PARTICLE:
                buf[0] = strdup(wxsItemTypeName(item));
                break;
            case XML_XSD10_SCHEMA_TYPE_NOTATION:
            {
                buf[0] = strdup(wxsItemTypeName(item));
                buf[0] = strcat(buf[0], " '");
                buf[0] = strcat(buf[0], getComponentQName(str, item));
                buf[0] = strcat(buf[0], "'");
                str[0] = null;
            }
            /* Falls through. */
                named = 0;
                break;
            default:
                named = 0;
            }
        }
        else
            named = 0;

        if ((named == 0) && (itemNode != null))
        {
            XmlNode elem;

            if (nodeKind(itemNode) == ATTRIBUTE)
                elem = nodeParent(itemNode);
            else
                elem = itemNode;
            /* libxml2 overwrites (leaks) a notation designation here, the output is the same. */
            buf[0] = null;
            buf[0] = strdup("Element '");
            if (nsHrefOf(elem) != null)
            {
                buf[0] = strcat(buf[0], formatQName(str, nsHrefOf(elem), nodeName(elem)));
                str[0] = null;
            }
            else
                buf[0] = strcat(buf[0], nodeName(elem));
            buf[0] = strcat(buf[0], "'");

        }
        if ((itemNode != null) && (nodeKind(itemNode) == ATTRIBUTE))
        {
            buf[0] = strcat(buf[0], ", attribute '");
            if (nsHrefOf(itemNode) != null)
            {
                buf[0] = strcat(buf[0], formatQName(str, nsHrefOf(itemNode), nodeName(itemNode)));
                str[0] = null;
            }
            else
                buf[0] = strcat(buf[0], nodeName(itemNode));
            buf[0] = strcat(buf[0], "'");
        }

        return escapeFormatString(buf);
    }

    // xml_xsd10_format_facet_enum_set
    /** Port of xmlSchemaFormatFacetEnumSet: builds a string consisting of all enumeration elements.
        @param actxt schema context @param buf the string buffer @param type the type holding the
        enumeration facets @return a string of all enumeration elements */
    static String formatFacetEnumSet(Xsd10AbstractCtxt actxt, String[] buf, Xsd10Type type)
    {
        Xsd10Facet facet;
        int ws;
        String[] value = { null };
        int res;
        boolean found = false;

        buf[0] = null;

        do
        {
            /*
             * Use the whitespace type of the base type.
             */
            ws = getWhiteSpaceFacetValue(type.baseType);
            for (facet = type.facets; facet != null; facet = facet.next)
            {
                if (facet.type != XML_XSD10_SCHEMA_FACET_ENUMERATION)
                    continue;
                found = true;
                res = getCanonValueWhtspExt(facet.val, ws, value);
                if (res == -1)
                {
                    internalErr(actxt,
                                "xmlSchemaFormatFacetEnumSet",
                                "compute the canonical lexical representation");
                    buf[0] = null;
                    return null;
                }
                if (buf[0] == null)
                    buf[0] = strdup("'");
                else
                    buf[0] = strcat(buf[0], ", '");
                buf[0] = strcat(buf[0], value[0]);
                buf[0] = strcat(buf[0], "'");
                value[0] = null;
            }
            /*
             * The enumeration facet of a type restricts the enumeration
             * facet of the ancestor type; i.e., such restricted enumerations
             * do not belong to the set of the given type. Thus we break
             * on the first found enumeration.
             */
            if (found)
                break;
            type = type.baseType;
        } while ((type != null) && (type.type != XML_XSD10_SCHEMA_TYPE_BASIC));

        return buf[0];
    }

    /* ***********************************************************************
     *                                                                      *
     *                  Error functions                                     *
     *                                                                      *
     ************************************************************************/

    // xml_xsd10_p_err_memory
    /** Port of xmlSchemaPErrMemory: handle an out of memory condition. @param ctxt schema parser context */
    static void pErrMemory(Xsd10ParserCtxt ctxt)
    {
        ValidityErrorFunc channel = null;
        StructuredErrorFunc schannel = null;
        Object data = null;

        if (ctxt != null)
        {
            ctxt.nberrors++;
            ctxt.err = XML_XSD10_ERR_NO_MEMORY;
            channel = ctxt.error;
            schannel = ctxt.serror;
            data = ctxt.errCtxt;
        }

        raiseMemoryError(schannel, channel, data, XML_XSD10_FROM_SCHEMASP);
    }

    // xml_xsd10_p_err_full
    /** Port of xmlSchemaPErrFull. @param ctxt context @param node node @param code code @param level
        level @param file file @param line line @param str1 str1 @param str2 str2 @param str3 str3
        @param col column @param msg format @param args arguments */
    static void pErrFull(Xsd10ParserCtxt ctxt, XmlNode node, int code, int level, String file, int line, String str1, String str2, String str3, int col, String msg, Object... args)
    {
        ValidityErrorFunc channel = null;
        StructuredErrorFunc schannel = null;
        Object data = null;
        int res;

        if (ctxt != null)
        {
            /* Don't overwrite memory errors */
            if (ctxt.err == XML_XSD10_ERR_NO_MEMORY)
                return;

            if (level == XML_XSD10_ERR_WARNING)
            {
                channel = warningChannel(ctxt.warning);
            }
            else
            {
                ctxt.nberrors++;
                ctxt.err = code;
                channel = ctxt.error;
            }
            data = ctxt.errCtxt;
            schannel = ctxt.serror;
        }

        if ((channel == null) && (schannel == null))
        {
            channel = GENERIC_ERROR;
            data = null;
        }

        res = vRaiseError(schannel, channel, data, ctxt, node,
                          XML_XSD10_FROM_SCHEMASP, code, level, file, line,
                          str1,
                          str2,
                          str3,
                          0, col, msg, args);

        if (res < 0)
            pErrMemory(ctxt);
    }

    // xml_xsd10_p_err
    /** Port of xmlSchemaPErr: handle a parser error. @param ctxt the parsing context @param node the
        context node @param code the error code @param msg the error message @param str1 extra data
        @param str2 extra data */
    static void pErr(Xsd10ParserCtxt ctxt, XmlNode node, int code, String msg, String str1, String str2)
    {
        pErrFull(ctxt, node, code, XML_XSD10_ERR_ERROR, null, 0,
                 str1, str2, null, 0, msg, str1, str2);
    }

    // xml_xsd10_p_err2
    /** Port of xmlSchemaPErr2: handle a parser error. @param ctxt the parsing context @param node the
        context node @param child the current child @param error the error code @param msg the error
        message @param str1 extra data @param str2 extra data */
    static void pErr2(Xsd10ParserCtxt ctxt, XmlNode node, XmlNode child, int error, String msg, String str1, String str2)
    {
        if (child != null)
            pErr(ctxt, child, error, msg, str1, str2);
        else
            pErr(ctxt, node, error, msg, str1, str2);
    }

    // xml_xsd10_p_err_ext
    /** Port of xmlSchemaPErrExt: handle a parser error. @param ctxt the parsing context @param node the
        context node @param code the error code @param strData1 extra data @param strData2 extra data
        @param strData3 extra data @param msg the message @param str1 param @param str2 param
        @param str3 param @param str4 param @param str5 param */
    static void pErrExt(Xsd10ParserCtxt ctxt, XmlNode node, int code, String strData1, String strData2, String strData3, String msg, String str1, String str2, String str3, String str4, String str5)
    {
        pErrFull(ctxt, node, code, XML_XSD10_ERR_ERROR, null, 0,
                 strData1, strData2, strData3, 0,
                 msg, str1, str2, str3, str4, str5);
    }

    /* ***********************************************************************
     *                                                                      *
     *                  Allround error functions                            *
     *                                                                      *
     ************************************************************************/

    // xml_xsd10_v_err_memory
    /** Port of xmlSchemaVErrMemory: handle an out of memory condition. @param ctxt schema validation context */
    static void vErrMemory(Xsd10ValidCtxt ctxt)
    {
        ValidityErrorFunc channel = null;
        StructuredErrorFunc schannel = null;
        Object data = null;

        if (ctxt != null)
        {
            ctxt.nberrors++;
            ctxt.err = XML_XSD10_ERR_NO_MEMORY;
            channel = ctxt.error;
            schannel = ctxt.serror;
            data = ctxt.errCtxt;
        }

        raiseMemoryError(schannel, channel, data, XML_XSD10_FROM_SCHEMASV);
    }

    // xml_xsd10_v_err_full
    /** Port of xmlSchemaVErrFull. @param ctxt context @param node node @param code code @param level
        level @param file file @param line line @param str1 str1 @param str2 str2 @param str3 str3
        @param col column @param msg format @param args arguments */
    static void vErrFull(Xsd10ValidCtxt ctxt, XmlNode node, int code, int level, String file, int line, String str1, String str2, String str3, int col, String msg, Object... args)
    {
        ValidityErrorFunc channel = null;
        StructuredErrorFunc schannel = null;
        Object data = null;
        int res;

        if (ctxt != null)
        {
            /* Don't overwrite memory errors */
            if (ctxt.err == XML_XSD10_ERR_NO_MEMORY)
                return;

            if (level == XML_XSD10_ERR_WARNING)
            {
                channel = warningChannel(ctxt.warning);
            }
            else
            {
                ctxt.nberrors++;
                ctxt.err = code;
                channel = ctxt.error;
            }
            data = ctxt.errCtxt;
            schannel = ctxt.serror;
        }

        if ((channel == null) && (schannel == null))
        {
            channel = GENERIC_ERROR;
            data = null;
        }

        res = vRaiseError(schannel, channel, data, ctxt, node,
                          XML_XSD10_FROM_SCHEMASV, code, level, file, line,
                          str1,
                          str2,
                          str3,
                          0, col, msg, args);

        if (res < 0)
            vErrMemory(ctxt);
    }

    // xml_xsd10_err4_line
    /** Port of xmlSchemaErr4Line: handle a validation error. The streaming branches (no node tree: the
        position of the parser input or of the locator) are not ported. @param ctxt the validation
        context @param errorLevel the error level @param code the error code @param node the context
        node @param line the line number @param msg the error message @param str1 extra data
        @param str2 extra data @param str3 extra data @param str4 extra data */
    static void err4Line(Xsd10AbstractCtxt ctxt, int errorLevel, int code, XmlNode node, int line, String msg, String str1, String str2, String str3, String str4)
    {
        if (ctxt != null)
        {
            if (ctxt.type == XML_XSD10_SCHEMA_CTXT_VALIDATOR)
            {
                Xsd10ValidCtxt vctxt = (Xsd10ValidCtxt) ctxt;
                String file = null;
                int col = 0;

                /*
                 * Error node. If we specify a line number, then
                 * do not channel any node to the error function.
                 */
                if (line == 0)
                {
                    if ((node == null) &&
                        (vctxt.depth >= 0) &&
                        (vctxt.inode != null))
                    {
                        node = vctxt.inode.node;
                    }
                }
                else
                {
                    /*
                     * Override the given node's (if any) position
                     * and channel only the given line number.
                     */
                    node = null;
                    /*
                     * Get filename.
                     */
                    if (vctxt.doc != null)
                        file = domUrl(vctxt.doc);
                }
                if ((file == null) && (vctxt.filename != null))
                    file = vctxt.filename;

                vErrFull(vctxt, node, code, errorLevel,
                         file, line, str1, str2, str3, col,
                         msg, str1, str2, str3, str4);
            }
            else if (ctxt.type == XML_XSD10_SCHEMA_CTXT_PARSER)
            {
                Xsd10ParserCtxt pctxt = (Xsd10ParserCtxt) ctxt;

                pErrFull(pctxt, node, code, errorLevel,
                         null, 0, str1, str2, str3, 0,
                         msg, str1, str2, str3, str4);
            }
        }
    }

    // xml_xsd10_err3
    /** Port of xmlSchemaErr3: handle a validation error. @param actxt the validation context
        @param error the error code @param node the context node @param msg the error message
        @param str1 extra data @param str2 extra data @param str3 extra data */
    static void err3(Xsd10AbstractCtxt actxt, int error, XmlNode node, String msg, String str1, String str2, String str3)
    {
        err4Line(actxt, XML_XSD10_ERR_ERROR, error, node, 0,
                 msg, str1, str2, str3, null);
    }

    // xml_xsd10_err4
    /** Port of xmlSchemaErr4. @param actxt context @param error code @param node node @param msg message
        @param str1 str1 @param str2 str2 @param str3 str3 @param str4 str4 */
    static void err4(Xsd10AbstractCtxt actxt, int error, XmlNode node, String msg, String str1, String str2, String str3, String str4)
    {
        err4Line(actxt, XML_XSD10_ERR_ERROR, error, node, 0,
                 msg, str1, str2, str3, str4);
    }

    // xml_xsd10_err
    /** Port of xmlSchemaErr. @param actxt context @param error code @param node node @param msg message
        @param str1 str1 @param str2 str2 */
    static void err(Xsd10AbstractCtxt actxt, int error, XmlNode node, String msg, String str1, String str2)
    {
        err4(actxt, error, node, msg, str1, str2, null, null);
    }

    // xml_xsd10_format_node_for_error
    /** Port of xmlSchemaFormatNodeForError. @param msg out: the text @param actxt context @param node
        node @return msg[0] (escaped) */
    static String formatNodeForError(String[] msg, Xsd10AbstractCtxt actxt, XmlNode node)
    {
        String[] str = { null };

        msg[0] = null;
        if ((node != null) &&
            (nodeKind(node) != ELEMENT) &&
            (nodeKind(node) != ATTRIBUTE))
        {
            /*
             * Don't try to format other nodes than element and
             * attribute nodes.
             * Play safe and return an empty string.
             */
            msg[0] = strdup("");
            return msg[0];
        }
        if (node != null)
        {
            /*
             * Work on tree nodes.
             */
            if (nodeKind(node) == ATTRIBUTE)
            {
                XmlNode elem = nodeParent(node);

                msg[0] = strdup("Element '");
                if (nsHrefOf(elem) != null)
                    msg[0] = strcat(msg[0], formatQName(str, nsHrefOf(elem), nodeName(elem)));
                else
                    msg[0] = strcat(msg[0], formatQName(str, null, nodeName(elem)));
                str[0] = null;
                msg[0] = strcat(msg[0], "', ");
                msg[0] = strcat(msg[0], "attribute '");
            }
            else
            {
                msg[0] = strdup("Element '");
            }
            if (nsHrefOf(node) != null)
                msg[0] = strcat(msg[0], formatQName(str, nsHrefOf(node), nodeName(node)));
            else
                msg[0] = strcat(msg[0], formatQName(str, null, nodeName(node)));
            str[0] = null;
            msg[0] = strcat(msg[0], "': ");
        }
        else if (actxt.type == XML_XSD10_SCHEMA_CTXT_VALIDATOR)
        {
            Xsd10ValidCtxt vctxt = (Xsd10ValidCtxt) actxt;
            /*
             * Work on node infos.
             */
            if (vctxt.inode.nodeType == ATTRIBUTE)
            {
                Xsd10NodeInfo ielem = vctxt.elemInfos[vctxt.depth];

                msg[0] = strdup("Element '");
                msg[0] = strcat(msg[0], formatQName(str, ielem.nsName, ielem.localName));
                str[0] = null;
                msg[0] = strcat(msg[0], "', ");
                msg[0] = strcat(msg[0], "attribute '");
            }
            else
            {
                msg[0] = strdup("Element '");
            }
            msg[0] = strcat(msg[0], formatQName(str, vctxt.inode.nsName, vctxt.inode.localName));
            str[0] = null;
            msg[0] = strcat(msg[0], "': ");
        }
        else if (actxt.type == XML_XSD10_SCHEMA_CTXT_PARSER)
        {
            /*
             * Hmm, no node while parsing?
             * Return an empty string, in case NULL will break something.
             */
            msg[0] = strdup("");
        }
        else
        {
            /* TODO */
            return null;
        }

        /*
         * xmlSchemaFormatItemForReport() also returns an escaped format
         * string, so do this before calling it below (in the future).
         */
        escapeFormatString(msg);

        /*
         * VAL TODO: The output of the given schema component is currently
         * disabled.
         */
        return msg[0];
    }

    // xml_xsd10_internal_err2
    /** Port of xmlSchemaInternalErr2. @param actxt context @param funcName function @param message
        message @param str1 str1 @param str2 str2 */
    static void internalErr2(Xsd10AbstractCtxt actxt, String funcName, String message, String str1, String str2)
    {
        String msg = null;

        if (actxt == null)
            return;
        msg = strdup("Internal error: %s, ");
        msg = strcat(msg, message);
        msg = strcat(msg, ".\n");

        if (actxt.type == XML_XSD10_SCHEMA_CTXT_VALIDATOR)
            err3(actxt, XML_XSD10_SCHEMAV_INTERNAL, null,
                 msg, funcName, str1, str2);
        else if (actxt.type == XML_XSD10_SCHEMA_CTXT_PARSER)
            err3(actxt, XML_XSD10_SCHEMAP_INTERNAL, null,
                 msg, funcName, str1, str2);
    }

    // xml_xsd10_internal_err
    /** Port of xmlSchemaInternalErr. @param actxt context @param funcName function @param message message */
    static void internalErr(Xsd10AbstractCtxt actxt, String funcName, String message)
    {
        internalErr2(actxt, funcName, message, null, null);
    }

    // xml_xsd10_custom_err4
    /** Port of xmlSchemaCustomErr4. @param actxt context @param error code @param node node
        @param item item @param message message @param str1 str1 @param str2 str2 @param str3 str3
        @param str4 str4 */
    static void customErr4(Xsd10AbstractCtxt actxt, int error, XmlNode node, Xsd10BasicItem item, String message, String str1, String str2, String str3, String str4)
    {
        String[] msg = { null };

        if ((node == null) && (item != null) &&
            (actxt.type == XML_XSD10_SCHEMA_CTXT_PARSER))
        {
            node = wxsItemNode(item);
            formatItemForReport(msg, null, item, null);
            msg[0] = strcat(msg[0], ": ");
        }
        else
            formatNodeForError(msg, actxt, node);
        msg[0] = strcat(msg[0], message);
        msg[0] = strcat(msg[0], ".\n");
        err4(actxt, error, node,
             msg[0], str1, str2, str3, str4);
    }

    // xml_xsd10_custom_err
    /** Port of xmlSchemaCustomErr. @param actxt context @param error code @param node node
        @param item item @param message message @param str1 str1 @param str2 str2 */
    static void customErr(Xsd10AbstractCtxt actxt, int error, XmlNode node, Xsd10BasicItem item, String message, String str1, String str2)
    {
        customErr4(actxt, error, node, item,
                   message, str1, str2, null, null);
    }

    // xml_xsd10_custom_warning
    /** Port of xmlSchemaCustomWarning. @param actxt context @param error code @param node node
        @param type unused @param message message @param str1 str1 @param str2 str2 @param str3 str3 */
    static void customWarning(Xsd10AbstractCtxt actxt, int error, XmlNode node, Xsd10Type type, String message, String str1, String str2, String str3)
    {
        String[] msg = { null };

        formatNodeForError(msg, actxt, node);
        msg[0] = strcat(msg[0], message);
        msg[0] = strcat(msg[0], ".\n");

        /* URGENT TODO: Set the error code to something sane. */
        err4Line(actxt, XML_XSD10_ERR_WARNING, error, node, 0,
                 msg[0], str1, str2, str3, null);
    }

    // xml_xsd10_keyref_err
    /** Port of xmlSchemaKeyrefErr. @param vctxt context @param error code @param idcNode the node
        @param type unused @param message message @param str1 str1 @param str2 str2 */
    static void keyrefErr(Xsd10ValidCtxt vctxt, int error, Xsd10PSVIIDCNode idcNode, Xsd10BasicItem type, String message, String str1, String str2)
    {
        String msg;
        String[] qname = { null };

        msg = strdup("Element '%s': ");
        msg = strcat(msg, message);
        msg = strcat(msg, ".\n");
        err4Line(vctxt, XML_XSD10_ERR_ERROR,
                 error, null, idcNode.nodeLine, msg,
                 formatQName(qname,
                             (String) vctxt.nodeQNames.items[idcNode.nodeQNameID + 1],
                             (String) vctxt.nodeQNames.items[idcNode.nodeQNameID]),
                 str1, str2, null);
    }

    // xml_xsd10_eval_error_node_type
    /** Port of xmlSchemaEvalErrorNodeType. @param actxt context @param node node @return the node kind or -1 */
    static int evalErrorNodeType(Xsd10AbstractCtxt actxt, XmlNode node)
    {
        if (node != null)
            return nodeKind(node);
        if ((actxt.type == XML_XSD10_SCHEMA_CTXT_VALIDATOR) &&
            (((Xsd10ValidCtxt) actxt).inode != null))
            return ((Xsd10ValidCtxt) actxt).inode.nodeType;
        return -1;
    }

    // xml_xsd10_is_global_item
    /** Port of xmlSchemaIsGlobalItem. @param item item @return 1 or 0 */
    static int isGlobalItem(Xsd10BasicItem item)
    {
        switch (item.type)
        {
        case XML_XSD10_SCHEMA_TYPE_COMPLEX:
        case XML_XSD10_SCHEMA_TYPE_SIMPLE:
            if ((item instanceof Xsd10Type t) && ((t.flags & XML_XSD10_SCHEMAS_TYPE_GLOBAL) != 0))
                return 1;
            break;
        case XML_XSD10_SCHEMA_TYPE_GROUP:
            return 1;
        case XML_XSD10_SCHEMA_TYPE_ELEMENT:
            if ((item instanceof Xsd10Element e) && ((e.flags & XML_XSD10_SCHEMAS_ELEM_GLOBAL) != 0))
                return 1;
            break;
        case XML_XSD10_SCHEMA_TYPE_ATTRIBUTE:
            if ((item instanceof Xsd10Attribute a) && ((a.flags & XML_XSD10_SCHEMAS_ATTR_GLOBAL) != 0))
                return 1;
            break;
        /* Note that attribute groups are always global. */
        default:
            return 1;
        }
        return 0;
    }

    // xml_xsd10_simple_type_err
    /** Port of xmlSchemaSimpleTypeErr. @param actxt context @param error code @param node node
        @param value value @param type type @param displayValue display the value */
    static void simpleTypeErr(Xsd10AbstractCtxt actxt, int error, XmlNode node, String value, Xsd10Type type, int displayValue)
    {
        String[] msg = { null };

        formatNodeForError(msg, actxt, node);

        if ((displayValue != 0) || (evalErrorNodeType(actxt, node) ==
                                    ATTRIBUTE))
            msg[0] = strcat(msg[0], "'%s' is not a valid value of ");
        else
            msg[0] = strcat(msg[0], "The character content is not a valid " +
                            "value of ");

        if (isGlobalItem(type) == 0)
            msg[0] = strcat(msg[0], "the local ");
        else
            msg[0] = strcat(msg[0], "the ");

        if (wxsIsAtomic(type) != 0)
            msg[0] = strcat(msg[0], "atomic type");
        else if (wxsIsList(type) != 0)
            msg[0] = strcat(msg[0], "list type");
        else if (wxsIsUnion(type) != 0)
            msg[0] = strcat(msg[0], "union type");

        if (isGlobalItem(type) != 0)
        {
            String[] str = { null };
            msg[0] = strcat(msg[0], " '");
            if (type.builtInType != 0)
            {
                msg[0] = strcat(msg[0], "xs:");
                str[0] = strdup(type.name);
            }
            else
            {
                String qName = formatQName(str, type.targetNamespace, type.name);
                if (str[0] == null)
                    str[0] = strdup(qName);
            }
            msg[0] = strcat(msg[0], escapeFormatString(str));
            msg[0] = strcat(msg[0], "'");
        }
        msg[0] = strcat(msg[0], ".\n");
        if ((displayValue != 0) || (evalErrorNodeType(actxt, node) ==
                                    ATTRIBUTE))
            err(actxt, error, node, msg[0], value, null);
        else
            err(actxt, error, node, msg[0], null, null);
    }

    // xml_xsd10_format_error_node_q_name
    /** Port of xmlSchemaFormatErrorNodeQName. @param str buffer @param ni node info @param node node
        @return the qname or null */
    static String formatErrorNodeQName(String[] str, Xsd10NodeInfo ni, XmlNode node)
    {
        if (node != null)
        {
            if (nsHrefOf(node) != null)
                return formatQName(str, nsHrefOf(node), nodeName(node));
            else
                return formatQName(str, null, nodeName(node));
        }
        else if (ni != null)
            return formatQName(str, ni.nsName, ni.localName);
        return null;
    }

    // xml_xsd10_illegal_attr_err
    /** Port of xmlSchemaIllegalAttrErr. @param actxt context @param error code @param ni attribute info
        @param node node */
    static void illegalAttrErr(Xsd10AbstractCtxt actxt, int error, Xsd10AttrInfo ni, XmlNode node)
    {
        String[] msg = { null };
        String[] str = { null };

        formatNodeForError(msg, actxt, node);
        msg[0] = strcat(msg[0], "The attribute '%s' is not allowed.\n");
        err(actxt, error, node, msg[0],
            formatErrorNodeQName(str, ni, node),
            null);
    }

    // xml_xsd10_complex_type_err
    /** Port of xmlSchemaComplexTypeErr. @param actxt context @param error code @param node node
        @param type unused @param message message @param nbval number of values @param nbneg number of
        negated values @param values the values */
    static void complexTypeErr(Xsd10AbstractCtxt actxt, int error, XmlNode node, Xsd10Type type, String message, int nbval, int nbneg, String[] values)
    {
        String[] str = { null };
        String[] msg = { null };
        String localName, nsName;
        String cur;
        int curPos, end;
        int i;

        formatNodeForError(msg, actxt, node);
        msg[0] = strcat(msg[0], message);
        msg[0] = strcat(msg[0], ".");
        /*
         * Note that is does not make sense to report that we have a
         * wildcard here, since the wildcard might be unfolded into
         * multiple transitions.
         */
        if (nbval + nbneg > 0)
        {
            if (nbval + nbneg > 1)
            {
                str[0] = strdup(" Expected is one of ( ");
            }
            else
                str[0] = strdup(" Expected is ( ");
            nsName = null;

            for (i = 0; i < nbval + nbneg; i++)
            {
                cur = values[i];
                if (cur == null)
                    continue;
                curPos = 0;
                if (cur.startsWith("not "))
                {
                    curPos += 4;
                    str[0] = strcat(str[0], "##other");
                }
                /*
                 * Get the local name.
                 */
                localName = null;

                end = curPos;
                if (end < cur.length() && cur.charAt(end) == '*')
                {
                    localName = strdup("*");
                    end++;
                }
                else
                {
                    while ((end < cur.length()) && (cur.charAt(end) != '|'))
                        end++;
                    localName = (end - curPos == 0) ? localName : strcat(localName, cur.substring(curPos, end));
                }
                if (end < cur.length())
                {
                    end++;
                    /*
                     * Skip "*|*" if they come with negated expressions, since
                     * they represent the same negated wildcard.
                     */
                    boolean endStar = end < cur.length() && cur.charAt(end) == '*';
                    boolean localStar = localName != null && localName.startsWith("*");
                    if ((nbneg == 0) || !endStar || !localStar)
                    {
                        /*
                         * Get the namespace name.
                         */
                        curPos = end;
                        if (endStar)
                        {
                            nsName = strdup("{*}");
                        }
                        else
                        {
                            end = cur.length();

                            if (i >= nbval)
                                nsName = strdup("{##other:");
                            else
                                nsName = strdup("{");

                            nsName = strcat(nsName, cur.substring(curPos, end));
                            nsName = strcat(nsName, "}");
                        }
                        str[0] = strcat(str[0], nsName);
                        nsName = null;
                    }
                    else
                    {
                        continue;
                    }
                }
                str[0] = strcat(str[0], localName);

                if (i < nbval + nbneg - 1)
                    str[0] = strcat(str[0], ", ");
            }
            str[0] = strcat(str[0], " ).\n");
            msg[0] = strcat(msg[0], escapeFormatString(str));
        }
        else
            msg[0] = strcat(msg[0], "\n");
        err(actxt, error, node, msg[0], null, null);
    }

    // xml_xsd10_facet_err
    /** Port of xmlSchemaFacetErr. @param actxt context @param error code @param node node @param value
        value @param length the actual length (C unsigned long) @param type type @param facet facet
        @param message message or null @param str1 str1 @param str2 str2 */
    static void facetErr(Xsd10AbstractCtxt actxt, int error, XmlNode node, String value, long length, Xsd10Type type, Xsd10Facet facet, String message, String str1, String str2)
    {
        String[] str = { null };
        String[] msg = { null };
        int facetType;
        int nodeType = evalErrorNodeType(actxt, node);

        formatNodeForError(msg, actxt, node);
        if (error == XML_XSD10_SCHEMAV_CVC_ENUMERATION_VALID)
        {
            facetType = XML_XSD10_SCHEMA_FACET_ENUMERATION;
            /*
             * If enumerations are validated, one must not expect the
             * facet to be given.
             */
        }
        else
            facetType = facet.type;
        msg[0] = strcat(msg[0], "[");
        msg[0] = strcat(msg[0], "facet '");
        msg[0] = strcat(msg[0], facetTypeToString(facetType));
        msg[0] = strcat(msg[0], "'] ");
        if (message == null)
        {
            /*
             * Use a default message.
             */
            if ((facetType == XML_XSD10_SCHEMA_FACET_LENGTH) ||
                (facetType == XML_XSD10_SCHEMA_FACET_MINLENGTH) ||
                (facetType == XML_XSD10_SCHEMA_FACET_MAXLENGTH))
            {
                String len, actLen;

                /* FIXME, TODO: What is the max expected string length of the
                 * this value?
                 */
                if (nodeType == ATTRIBUTE)
                    msg[0] = strcat(msg[0], "The value '%s' has a length of '%s'; ");
                else
                    msg[0] = strcat(msg[0], "The value has a length of '%s'; ");

                len = format("%lu", getFacetValueAsUlong(facet));
                actLen = format("%lu", length);

                if (facetType == XML_XSD10_SCHEMA_FACET_LENGTH)
                    msg[0] = strcat(msg[0],
                                    "this differs from the allowed length of '%s'.\n");
                else if (facetType == XML_XSD10_SCHEMA_FACET_MAXLENGTH)
                    msg[0] = strcat(msg[0],
                                    "this exceeds the allowed maximum length of '%s'.\n");
                else if (facetType == XML_XSD10_SCHEMA_FACET_MINLENGTH)
                    msg[0] = strcat(msg[0],
                                    "this underruns the allowed minimum length of '%s'.\n");

                if (nodeType == ATTRIBUTE)
                    err3(actxt, error, node, msg[0],
                         value, actLen, len);
                else
                    err(actxt, error, node, msg[0],
                        actLen, len);

            }
            else if (facetType == XML_XSD10_SCHEMA_FACET_ENUMERATION)
            {
                msg[0] = strcat(msg[0], "The value '%s' is not an element " +
                                "of the set {%s}.\n");
                err(actxt, error, node, msg[0], value,
                    formatFacetEnumSet(actxt, str, type));
            }
            else if (facetType == XML_XSD10_SCHEMA_FACET_PATTERN)
            {
                msg[0] = strcat(msg[0], "The value '%s' is not accepted " +
                                "by the pattern '%s'.\n");
                err(actxt, error, node, msg[0], value,
                    facet.value);
            }
            else if (facetType == XML_XSD10_SCHEMA_FACET_MININCLUSIVE)
            {
                msg[0] = strcat(msg[0], "The value '%s' is less than the " +
                                "minimum value allowed ('%s').\n");
                err(actxt, error, node, msg[0], value,
                    facet.value);
            }
            else if (facetType == XML_XSD10_SCHEMA_FACET_MAXINCLUSIVE)
            {
                msg[0] = strcat(msg[0], "The value '%s' is greater than the " +
                                "maximum value allowed ('%s').\n");
                err(actxt, error, node, msg[0], value,
                    facet.value);
            }
            else if (facetType == XML_XSD10_SCHEMA_FACET_MINEXCLUSIVE)
            {
                msg[0] = strcat(msg[0], "The value '%s' must be greater than " +
                                "'%s'.\n");
                err(actxt, error, node, msg[0], value,
                    facet.value);
            }
            else if (facetType == XML_XSD10_SCHEMA_FACET_MAXEXCLUSIVE)
            {
                msg[0] = strcat(msg[0], "The value '%s' must be less than " +
                                "'%s'.\n");
                err(actxt, error, node, msg[0], value,
                    facet.value);
            }
            else if (facetType == XML_XSD10_SCHEMA_FACET_TOTALDIGITS)
            {
                msg[0] = strcat(msg[0], "The value '%s' has more " +
                                "digits than are allowed ('%s').\n");
                err(actxt, error, node, msg[0], value,
                    facet.value);
            }
            else if (facetType == XML_XSD10_SCHEMA_FACET_FRACTIONDIGITS)
            {
                msg[0] = strcat(msg[0], "The value '%s' has more fractional " +
                                "digits than are allowed ('%s').\n");
                err(actxt, error, node, msg[0], value,
                    facet.value);
            }
            else if (nodeType == ATTRIBUTE)
            {
                msg[0] = strcat(msg[0], "The value '%s' is not facet-valid.\n");
                err(actxt, error, node, msg[0], value, null);
            }
            else
            {
                msg[0] = strcat(msg[0], "The value is not facet-valid.\n");
                err(actxt, error, node, msg[0], null, null);
            }
        }
        else
        {
            msg[0] = strcat(msg[0], message);
            msg[0] = strcat(msg[0], ".\n");
            err(actxt, error, node, msg[0], str1, str2);
        }
    }

    // xml_xsd10_p_missing_attr_err
    /** Port of xmlSchemaPMissingAttrErr: reports an illegal attribute. @param ctxt the schema
        validation context @param error the error code @param ownerItem the owner as a schema object
        @param ownerElem the owner as an element node @param name name of the attribute
        @param message the error message */
    static void pMissingAttrErr(Xsd10ParserCtxt ctxt, int error, Xsd10BasicItem ownerItem, XmlNode ownerElem, String name, String message)
    {
        String[] des = { null };

        formatItemForReport(des, null, ownerItem, ownerElem);

        if (message != null)
            pErr(ctxt, ownerElem, error, "%s: %s.\n", des[0], message);
        else
            pErr(ctxt, ownerElem, error,
                 "%s: The attribute '%s' is required but missing.\n",
                 des[0], name);
    }

    // xml_xsd10_p_res_comp_attr_err
    /** Port of xmlSchemaPResCompAttrErr: used to report QName attribute values that failed to resolve
        to schema components. @param ctxt the schema validation context @param error the error code
        @param ownerItem the owner as a schema object @param ownerElem the owner as an element node
        @param name the name of the attribute holding the QName @param refName the referenced local
        name @param refURI the referenced namespace URI @param refType the referenced type
        @param refTypeStr the referenced type string */
    static void pResCompAttrErr(Xsd10ParserCtxt ctxt, int error, Xsd10BasicItem ownerItem, XmlNode ownerElem, String name, String refName, String refURI, int refType, String refTypeStr)
    {
        String[] des = { null };
        String[] strA = { null };

        formatItemForReport(des, null, ownerItem, ownerElem);
        if (refTypeStr == null)
            refTypeStr = itemTypeToStr(refType);
        pErrExt(ctxt, ownerElem, error,
                null, null, null,
                "%s, attribute '%s': The QName value '%s' does not resolve to a(n) " +
                "%s.\n", des[0], name,
                formatQName(strA, refURI, refName),
                refTypeStr, null);
    }

    // xml_xsd10_p_custom_attr_err
    /** Port of xmlSchemaPCustomAttrErr: reports an illegal attribute during the parse.
        @param ctxt the schema parser context @param error the error code @param ownerDes the
        designation of the owner (in/out, may be null) @param ownerItem the owner as a schema object
        @param attr the illegal attribute node @param msg the error message */
    static void pCustomAttrErr(Xsd10ParserCtxt ctxt, int error, String[] ownerDes, Xsd10BasicItem ownerItem, XmlNode attr, String msg)
    {
        String des = null;

        if (ownerDes == null)
        {
            String[] d = { null };
            formatItemForReport(d, null, ownerItem, attr != null ? nodeParent(attr) : null);
            des = d[0];
        }
        else if (ownerDes[0] == null)
        {
            formatItemForReport(ownerDes, null, ownerItem, attr != null ? nodeParent(attr) : null);
            des = ownerDes[0];
        }
        else
            des = ownerDes[0];
        if (attr == null)
        {
            pErrExt(ctxt, null, error, null, null, null,
                    "%s, attribute '%s': %s.\n",
                    des, "Unknown",
                    msg, null, null);
        }
        else
        {
            pErrExt(ctxt, attr, error, null, null, null,
                    "%s, attribute '%s': %s.\n",
                    des, nodeName(attr), msg, null, null);
        }
    }

    // xml_xsd10_p_illegal_attr_err
    /** Port of xmlSchemaPIllegalAttrErr: reports an illegal attribute during the parse.
        @param ctxt the schema parser context @param error the error code @param ownerComp the
        attribute's owner item (unused) @param attr the illegal attribute node */
    static void pIllegalAttrErr(Xsd10ParserCtxt ctxt, int error, Xsd10BasicItem ownerComp, XmlNode attr)
    {
        String[] strA = { null };
        String[] strB = { null };

        formatNodeForError(strA, ctxt, nodeParent(attr));
        err4(ctxt, error, attr,
             "%sThe attribute '%s' is not allowed.\n", strA[0],
             formatQName(strB, nsHrefOf(attr), nodeName(attr)),
             null, null);
    }

    // xml_xsd10_p_custom_err_ext
    /** Port of xmlSchemaPCustomErrExt: reports an error during parsing. @param ctxt the schema parser
        context @param error the error code @param item the schema item @param itemElem the node of the
        schema item @param message the error message @param str1 optional param @param str2 optional
        param @param str3 optional param */
    static void pCustomErrExt(Xsd10ParserCtxt ctxt, int error, Xsd10BasicItem item, XmlNode itemElem, String message, String str1, String str2, String str3)
    {
        String[] des = { null };
        String msg;

        formatItemForReport(des, null, item, itemElem);
        msg = strdup("%s: ");
        msg = strcat(msg, message);
        msg = strcat(msg, ".\n");
        if ((itemElem == null) && (item != null))
            itemElem = wxsItemNode(item);
        pErrExt(ctxt, itemElem, error, null, null, null,
                msg, des[0], str1, str2, str3, null);
    }

    // xml_xsd10_p_custom_err
    /** Port of xmlSchemaPCustomErr: reports an error during parsing. @param ctxt the schema parser
        context @param error the error code @param item the schema item @param itemElem the node of the
        schema item @param message the error message @param str1 the optional param */
    static void pCustomErr(Xsd10ParserCtxt ctxt, int error, Xsd10BasicItem item, XmlNode itemElem, String message, String str1)
    {
        pCustomErrExt(ctxt, error, item, itemElem, message,
                      str1, null, null);
    }

    // xml_xsd10_p_attr_use_err4
    /** Port of xmlSchemaPAttrUseErr4: reports an attribute use error during parsing.
        @param ctxt the schema parser context @param error the error code @param node the context node
        @param ownerItem the owner as a schema object @param attruse the invalid schema attribute
        @param message the error message @param str1 param @param str2 param @param str3 param
        @param str4 param */
    static void pAttrUseErr4(Xsd10ParserCtxt ctxt, int error, XmlNode node, Xsd10BasicItem ownerItem, Xsd10AttributeUse attruse, String message, String str1, String str2, String str3, String str4)
    {
        String[] str = { null };
        String[] msg = { null };

        formatItemForReport(msg, null, ownerItem, null);
        msg[0] = strcat(msg[0], ", ");
        msg[0] = strcat(msg[0],
                        formatItemForReport(str, null,
                                            attruse, null));
        str[0] = null;
        msg[0] = strcat(msg[0], ": ");
        msg[0] = strcat(msg[0], message);
        msg[0] = strcat(msg[0], ".\n");
        err4(ctxt, error, node,
             msg[0], str1, str2, str3, str4);
    }

    // xml_xsd10_p_illegal_facet_atomic_err
    /** Port of xmlSchemaPIllegalFacetAtomicErr: reports an illegal facet for atomic simple types.
        @param ctxt the schema parser context @param error the error code @param type the schema type
        @param baseType the base type of type @param facet the illegal facet */
    static void pIllegalFacetAtomicErr(Xsd10ParserCtxt ctxt, int error, Xsd10Type type, Xsd10Type baseType, Xsd10Facet facet)
    {
        String[] des = { null };
        String[] strT = { null };

        formatItemForReport(des, null, type, type.node);
        pErrExt(ctxt, type.node, error, null, null, null,
                "%s: The facet '%s' is not allowed on types derived from the " +
                "type %s.\n",
                des[0], facetTypeToString(facet.type),
                formatItemForReport(strT, null, baseType, null),
                null, null);
    }

    // xml_xsd10_p_illegal_facet_list_union_err
    /** Port of xmlSchemaPIllegalFacetListUnionErr: reports an illegal facet for list and union.
        @param ctxt the schema parser context @param error the error code @param type the schema type
        @param facet the illegal facet */
    static void pIllegalFacetListUnionErr(Xsd10ParserCtxt ctxt, int error, Xsd10Type type, Xsd10Facet facet)
    {
        String[] des = { null };

        formatItemForReport(des, null, type,
                            type.node);
        pErr(ctxt, type.node, error,
             "%s: The facet '%s' is not allowed.\n",
             des[0], facetTypeToString(facet.type));
    }

    // xml_xsd10_p_mutual_excl_attr_err
    /** Port of xmlSchemaPMutualExclAttrErr: reports an illegal attribute. @param ctxt the schema
        validation context @param error the error code @param ownerItem the owner as a schema object
        @param attr the bad attribute node @param name1 first mutually exclusive name
        @param name2 second mutually exclusive name */
    static void pMutualExclAttrErr(Xsd10ParserCtxt ctxt, int error, Xsd10BasicItem ownerItem, XmlNode attr, String name1, String name2)
    {
        String[] des = { null };

        formatItemForReport(des, null, ownerItem, nodeParent(attr));
        pErrExt(ctxt, attr, error, null, null, null,
                "%s: The attributes '%s' and '%s' are mutually exclusive.\n",
                des[0], name1, name2, null, null);
    }

    // xml_xsd10_p_simple_type_err
    /** Port of xmlSchemaPSimpleTypeErr: reports a simple type validation error. TODO: Should this
        report the value of an element as well? @param ctxt the schema validation context
        @param error the error code @param ownerItem the schema object if existent (unused)
        @param node the validated node @param type the type specifier @param expected the expected
        value @param value the validated value @param message error message @param str1 extra data
        @param str2 extra data */
    static void pSimpleTypeErr(Xsd10ParserCtxt ctxt, int error, Xsd10BasicItem ownerItem, XmlNode node, Xsd10Type type, String expected, String value, String message, String str1, String str2)
    {
        String[] msg = { null };

        formatNodeForError(msg, ctxt, node);
        if (message == null)
        {
            /*
             * Use default messages.
             */
            if (type != null)
            {
                if (nodeKind(node) == ATTRIBUTE)
                    msg[0] = strcat(msg[0], "'%s' is not a valid value of ");
                else
                    msg[0] = strcat(msg[0], "The character content is not a " +
                                    "valid value of ");
                if (isGlobalItem(type) == 0)
                    msg[0] = strcat(msg[0], "the local ");
                else
                    msg[0] = strcat(msg[0], "the ");

                if (wxsIsAtomic(type) != 0)
                    msg[0] = strcat(msg[0], "atomic type");
                else if (wxsIsList(type) != 0)
                    msg[0] = strcat(msg[0], "list type");
                else if (wxsIsUnion(type) != 0)
                    msg[0] = strcat(msg[0], "union type");

                if (isGlobalItem(type) != 0)
                {
                    String[] str = { null };
                    msg[0] = strcat(msg[0], " '");
                    if (type.builtInType != 0)
                    {
                        msg[0] = strcat(msg[0], "xs:");
                        str[0] = strdup(type.name);
                    }
                    else
                    {
                        String qName = formatQName(str, type.targetNamespace, type.name);
                        if (str[0] == null)
                            str[0] = strdup(qName);
                    }
                    msg[0] = strcat(msg[0], escapeFormatString(str));
                    msg[0] = strcat(msg[0], "'.");
                }
            }
            else
            {
                if (nodeKind(node) == ATTRIBUTE)
                    msg[0] = strcat(msg[0], "The value '%s' is not valid.");
                else
                    msg[0] = strcat(msg[0], "The character content is not " +
                                    "valid.");
            }
            if (expected != null)
            {
                String[] expectedEscaped = { charStrdup(expected) };
                msg[0] = strcat(msg[0], " Expected is '");
                msg[0] = strcat(msg[0], escapeFormatString(expectedEscaped));
                msg[0] = strcat(msg[0], "'.\n");
            }
            else
                msg[0] = strcat(msg[0], "\n");
            if (nodeKind(node) == ATTRIBUTE)
                pErr(ctxt, node, error, msg[0], value, null);
            else
                pErr(ctxt, node, error, msg[0], null, null);
        }
        else
        {
            msg[0] = strcat(msg[0], message);
            msg[0] = strcat(msg[0], ".\n");
            pErrExt(ctxt, node, error, null, null, null,
                    msg[0], str1, str2, null, null, null);
        }
    }

    // xml_xsd10_p_content_err
    /** Port of xmlSchemaPContentErr: reports an error concerning the content of a schema element.
        @param ctxt the schema parser context @param error the error code @param ownerItem the owner
        item of the holder of the content @param ownerElem the node of the holder of the content
        @param child the invalid child node @param message the optional error message
        @param content the optional string describing the correct content */
    static void pContentErr(Xsd10ParserCtxt ctxt, int error, Xsd10BasicItem ownerItem, XmlNode ownerElem, XmlNode child, String message, String content)
    {
        String[] des = { null };

        formatItemForReport(des, null, ownerItem, ownerElem);
        if (message != null)
            pErr2(ctxt, ownerElem, child, error,
                  "%s: %s.\n",
                  des[0], message);
        else
        {
            if (content != null)
            {
                pErr2(ctxt, ownerElem, child, error,
                      "%s: The content is not valid. Expected is %s.\n",
                      des[0], content);
            }
            else
            {
                pErr2(ctxt, ownerElem, child, error,
                      "%s: The content is not valid.\n",
                      des[0], null);
            }
        }
    }

    /** The function xml_xsd10_generic_error as a ValidityWarningFunc (C casts the warning function to
        the error function type; a warning channel equal to this one is the generic error channel). */
    public static final ValidityWarningFunc GENERIC_WARNING = Xsd10Schemas::genericError;

    /* The warning function of a context as an error channel (C uses the same function pointer). */
    private static ValidityErrorFunc warningChannel(ValidityWarningFunc w)
    {
        if (w == null) return null;
        if (w == GENERIC_WARNING) return GENERIC_ERROR;
        return w::warning;
    }

    /* ***********************************************************************
     *                                                                      *
     *                  Allocation functions                                *
     *                                                                      *
     ************************************************************************/

    // xml_xsd10_new_schema
    /** Port of xmlSchemaNewSchema: allocate a new Schema structure. @param ctxt a schema validation
        context @return the newly allocated structure */
    static Xsd10Schema newSchema(Xsd10ParserCtxt ctxt)
    {
        Xsd10Schema ret;

        ret = new Xsd10Schema();
        ret.dict = ctxt.dict;
        dictReference(ret.dict);

        return ret;
    }

    /* xmlSchemaNewFacet is xml_xsd10_new_facet of xml_xsd10_types. */

    // xml_xsd10_new_annot
    /** Port of xmlSchemaNewAnnot: allocate a new annotation structure. @param ctxt a schema validation
        context @param node a node @return the newly allocated structure */
    static Xsd10Annot newAnnot(Xsd10ParserCtxt ctxt, XmlNode node)
    {
        Xsd10Annot ret;

        ret = new Xsd10Annot();
        ret.content = node;
        return ret;
    }

    // xml_xsd10_item_list_create
    /** Port of xmlSchemaItemListCreate. @return the list */
    static Xsd10ItemList itemListCreate()
    {
        return new Xsd10ItemList();
    }

    // xml_xsd10_item_list_clear
    /** Port of xmlSchemaItemListClear. @param list list */
    static void itemListClear(Xsd10ItemList list)
    {
        if (list.items != null)
        {
            list.items = null;
        }
        list.nbItems = 0;
        list.sizeItems = 0;
    }

    // xml_xsd10_item_list_grow
    /** Port of xmlSchemaItemListGrow. @param list list @param initialSize initial size @return 0 or -1 */
    static int itemListGrow(Xsd10ItemList list, int initialSize)
    {
        int newSize;

        if (initialSize <= 0)
            initialSize = 1;
        newSize = growCapacity(list.sizeItems, 8,
                               initialSize, XML_XSD10_MAX_ITEMS);
        if (newSize < 0)
            return -1;
        list.items = (list.items == null) ? new Object[newSize] : Arrays.copyOf(list.items, newSize);
        list.sizeItems = newSize;
        return 0;
    }

    // xml_xsd10_item_list_add_size
    /** Port of xmlSchemaItemListAddSize. @param list list @param initialSize initial size @param item
        item @return 0 or -1 */
    static int itemListAddSize(Xsd10ItemList list, int initialSize, Object item)
    {
        if (list.sizeItems <= list.nbItems)
        {
            if (itemListGrow(list, initialSize) < 0)
            {
                pErrMemory(null);
                return -1;
            }
        }

        list.items[list.nbItems++] = item;
        return 0;
    }

    // xml_xsd10_item_list_add
    /** Port of xmlSchemaItemListAdd. @param list list @param item item @return 0 or -1 */
    static int itemListAdd(Xsd10ItemList list, Object item)
    {
        return itemListAddSize(list, 20, item);
    }

    // xml_xsd10_item_list_insert
    /** Port of xmlSchemaItemListInsert. @param list list @param item item @param idx index @return 0 or -1 */
    static int itemListInsert(Xsd10ItemList list, Object item, int idx)
    {
        if (list.sizeItems <= list.nbItems)
        {
            if (itemListGrow(list, 20) < 0)
            {
                pErrMemory(null);
                return -1;
            }
        }

        /*
         * Just append if the index is greater/equal than the item count.
         */
        if (idx >= list.nbItems)
        {
            list.items[list.nbItems++] = item;
        }
        else
        {
            int i;
            for (i = list.nbItems; i > idx; i--)
                list.items[i] = list.items[i - 1];
            list.items[idx] = item;
            list.nbItems++;
        }
        return 0;
    }

    // xml_xsd10_item_list_remove
    /** Port of xmlSchemaItemListRemove. @param list list @param idx index @return 0 or -1 */
    static int itemListRemove(Xsd10ItemList list, int idx)
    {
        int i;
        if ((list.items == null) || (idx >= list.nbItems))
            return -1;

        if (list.nbItems == 1)
        {
            /* TODO: Really free the list? */
            list.items = null;
            list.nbItems = 0;
            list.sizeItems = 0;
        }
        else if (list.nbItems - 1 == idx)
        {
            list.nbItems--;
        }
        else
        {
            for (i = idx; i < list.nbItems - 1; i++)
                list.items[i] = list.items[i + 1];
            list.nbItems--;
        }
        return 0;
    }

    // xml_xsd10_item_list_free
    /** Port of xmlSchemaItemListFree (nothing to free in Java). @param list a list */
    static void itemListFree(Xsd10ItemList list)
    {
        if (list == null)
            return;
        /* Garbage collected. */
    }

    // xml_xsd10_bucket_free
    /** Port of xmlSchemaBucketFree (also frees the ids of the bucket). @param bucket bucket */
    static void bucketFree(Xsd10Bucket bucket)
    {
        if (bucket == null)
            return;
        if (bucket.globals != null)
        {
            componentListFree(bucket.globals);
            itemListFree(bucket.globals);
        }
        if (bucket.locals != null)
        {
            componentListFree(bucket.locals);
            itemListFree(bucket.locals);
        }
        /* The relations are garbage collected. */
        if (bucket.ids != null)
            idsFree(bucket.ids);
        /* (! bucket->preserveDoc) && (bucket->doc != NULL): xml_dom_free, garbage collected. */
        if (bucket.type == XML_XSD10_SCHEMA_SCHEMA_IMPORT)
        {
            if ((bucket instanceof Xsd10Import) && (wxsImpbucket(bucket).schema != null))
                free(wxsImpbucket(bucket).schema);
        }
    }

    // xml_xsd10_bucket_free_entry
    /** Port of xmlSchemaBucketFreeEntry. @param bucket bucket @param name unused */
    static void bucketFreeEntry(Object bucket, String name)
    {
        bucketFree((Xsd10Bucket) bucket);
    }

    // xml_xsd10_bucket_create
    /** Port of xmlSchemaBucketCreate. @param pctxt parser context @param type bucket type
        @param targetNamespace namespace @return the bucket or null */
    static Xsd10Bucket bucketCreate(Xsd10ParserCtxt pctxt, int type, String targetNamespace)
    {
        Xsd10Bucket ret;
        Xsd10Schema mainSchema;

        if (wxsConstructor(pctxt).mainSchema == null)
        {
            perrorInt(pctxt, "xmlSchemaBucketCreate",
                      "no main schema on constructor");
            return null;
        }
        mainSchema = wxsConstructor(pctxt).mainSchema;
        /* Create the schema bucket. */
        if (wxsIsBucketIncredef(type) != 0)
            ret = new Xsd10Include();
        else
            ret = new Xsd10Import();
        ret.targetNamespace = targetNamespace;
        ret.type = type;
        ret.globals = itemListCreate();
        ret.locals = itemListCreate();
        /*
         * The following will assure that only the first bucket is marked as
         * XML_SCHEMA_SCHEMA_MAIN and it points to the *main* schema.
         * For each following import buckets an xmlSchema will be created.
         * An xmlSchema will be created for every distinct targetNamespace.
         * We assign the targetNamespace to the schemata here.
         */
        if (wxsHasBuckets(pctxt) == 0)
        {
            if (wxsIsBucketIncredef(type) != 0)
            {
                perrorInt(pctxt, "xmlSchemaBucketCreate",
                          "first bucket but it's an include or redefine");
                bucketFree(ret);
                return null;
            }
            /* Force the type to be XML_SCHEMA_SCHEMA_MAIN. */
            ret.type = XML_XSD10_SCHEMA_SCHEMA_MAIN;
            /* Point to the *main* schema. */
            wxsConstructor(pctxt).mainBucket = ret;
            wxsImpbucket(ret).schema = mainSchema;
            /*
             * Ensure that the main schema gets a targetNamespace.
             */
            mainSchema.targetNamespace = targetNamespace;
        }
        else
        {
            if (type == XML_XSD10_SCHEMA_SCHEMA_MAIN)
            {
                perrorInt(pctxt, "xmlSchemaBucketCreate",
                          "main bucket but it's not the first one");
                bucketFree(ret);
                return null;
            }
            else if (type == XML_XSD10_SCHEMA_SCHEMA_IMPORT)
            {
                /*
                 * Create a schema for imports and assign the
                 * targetNamespace.
                 */
                wxsImpbucket(ret).schema = newSchema(pctxt);
                if (wxsImpbucket(ret).schema == null)
                {
                    bucketFree(ret);
                    return null;
                }
                wxsImpbucket(ret).schema.targetNamespace = targetNamespace;
            }
        }
        if (wxsIsBucketImpmain(type) != 0)
        {
            int res;
            /*
             * Imports go into the "schemasImports" slot of the main *schema*.
             * Note that we create an import entry for the main schema as well; i.e.,
             * even if there's only one schema, we'll get an import.
             */
            if (mainSchema.schemasImports == null)
            {
                mainSchema.schemasImports = hashCreateDict(5,
                                                           wxsConstructor(pctxt).dict);
            }
            if (targetNamespace == null)
                res = hashAddEntry(mainSchema.schemasImports,
                                   XML_XSD10_SCHEMAS_NO_NAMESPACE, ret);
            else
                res = hashAddEntry(mainSchema.schemasImports,
                                   targetNamespace, ret);
            if (res != 0)
            {
                perrorInt(pctxt, "xmlSchemaBucketCreate",
                          "failed to add the schema bucket to the hash");
                bucketFree(ret);
                return null;
            }
        }
        else
        {
            /* Set the @ownerImport of an include bucket. */
            Xsd10Bucket cur = wxsConstructor(pctxt).bucket;
            if (wxsIsBucketImpmain(cur.type) != 0)
                wxsIncbucket(ret).ownerImport = wxsImpbucket(cur);
            else
                wxsIncbucket(ret).ownerImport = wxsIncbucket(cur).ownerImport;

            /* Includes got into the "includes" slot of the *main* schema. */
            if (mainSchema.includes == null)
            {
                mainSchema.includes = itemListCreate();
            }
            if (itemListAdd((Xsd10ItemList) mainSchema.includes, ret) < 0)
            {
                bucketFree(ret);
                return null;
            }
        }
        /*
         * Add to list of all buckets; this is used for lookup
         * during schema construction time only.
         */
        if (itemListAdd(wxsConstructor(pctxt).buckets, ret) == -1)
            return null;
        return ret;
    }

    // xml_xsd10_add_item_size
    /** Port of xmlSchemaAddItemSize. @param list the list (in/out, created if null) @param initialSize
        initial size @param item item @return 0 or -1 */
    static int addItemSize(Xsd10ItemList[] list, int initialSize, Object item)
    {
        if (list[0] == null)
        {
            list[0] = itemListCreate();
        }
        return itemListAddSize(list[0], initialSize, item);
    }

    // xml_xsd10_free_annot
    /** Port of xmlSchemaFreeAnnot: deallocate a annotation structure (garbage collected).
        @param annot a schema type structure */
    static void freeAnnot(Xsd10Annot annot)
    {
    }

    // xml_xsd10_free_notation
    /** Port of xmlSchemaFreeNotation. @param nota a schema notation structure */
    static void freeNotation(Xsd10Notation nota)
    {
        if (nota == null)
            return;
        if (nota.annot != null)
            freeAnnot(nota.annot);
    }

    // xml_xsd10_free_attribute
    /** Port of xmlSchemaFreeAttribute: deallocates an attribute declaration structure.
        @param attr an attribute declaration */
    static void freeAttribute(Xsd10Attribute attr)
    {
        if (attr == null)
            return;
        if (attr.annot != null)
            freeAnnot(attr.annot);
        if (attr.defVal != null)
            freeValue(attr.defVal);
    }

    // xml_xsd10_free_attribute_use
    /** Port of xmlSchemaFreeAttributeUse: deallocates an attribute use structure. @param use an attribute use */
    static void freeAttributeUse(Xsd10AttributeUse use)
    {
        if (use == null)
            return;
        if (use.annot != null)
            freeAnnot(use.annot);
        if (use.defVal != null)
            freeValue(use.defVal);
    }

    // xml_xsd10_free_attribute_use_prohib
    /** Port of xmlSchemaFreeAttributeUseProhib. @param prohib an attribute use prohibition */
    static void freeAttributeUseProhib(Xsd10AttributeUseProhib prohib)
    {
    }

    // xml_xsd10_free_wildcard_ns_set
    /** Port of xmlSchemaFreeWildcardNsSet: deallocates a list of wildcard constraint structures.
        @param set a schema wildcard namespace */
    static void freeWildcardNsSet(Xsd10WildcardNs set)
    {
    }

    // xml_xsd10_free_wildcard
    /** Port of xmlSchemaFreeWildcard: deallocates a wildcard structure. @param wildcard a wildcard structure */
    static void freeWildcard(Xsd10Wildcard wildcard)
    {
        if (wildcard == null)
            return;
        if (wildcard.annot != null)
            freeAnnot(wildcard.annot);
        if (wildcard.nsSet != null)
            freeWildcardNsSet(wildcard.nsSet);
    }

    // xml_xsd10_free_attribute_group
    /** Port of xmlSchemaFreeAttributeGroup. @param attrGr a schema attribute group structure */
    static void freeAttributeGroup(Xsd10AttributeGroup attrGr)
    {
        if (attrGr == null)
            return;
        if (attrGr.annot != null)
            freeAnnot(attrGr.annot);
        if (attrGr.attrUses instanceof Xsd10ItemList l)
            itemListFree(l);
    }

    // xml_xsd10_free_q_name_ref
    /** Port of xmlSchemaFreeQNameRef. @param item a QName reference structure */
    static void freeQNameRef(Xsd10QNameRef item)
    {
    }

    // xml_xsd10_free_type_link_list
    /** Port of xmlSchemaFreeTypeLinkList: deallocate a list of types. @param link a type link */
    static void freeTypeLinkList(Xsd10TypeLink link)
    {
    }

    // xml_xsd10_free_idc_state_obj_list
    /** Port of xmlSchemaFreeIDCStateObjList. @param sto the list */
    static void freeIdcStateObjList(Xsd10IDCStateObj sto)
    {
        Xsd10IDCStateObj next;
        while (sto != null)
        {
            next = sto.next;
            if (sto.xpathCtxt instanceof XmlStreamCtxt x)
                XmlPattern.streamCtxtFree(x);
            sto = next;
        }
    }

    // xml_xsd10_free_idc
    /** Port of xmlSchemaFreeIDC: deallocates an identity-constraint definition.
        @param idcDef a identity-constraint definition */
    static void freeIdc(Xsd10IDC idcDef)
    {
        Xsd10IDCSelect cur, prev;

        if (idcDef == null)
            return;
        if (idcDef.annot != null)
            freeAnnot(idcDef.annot);
        /* Selector */
        if (idcDef.selector != null)
        {
            if (idcDef.selector.xpathComp instanceof XmlPattern p)
                XmlPattern.patternFree(p);
        }
        /* Fields */
        if (idcDef.fields != null)
        {
            cur = idcDef.fields;
            do
            {
                prev = cur;
                cur = cur.next;
                if (prev.xpathComp instanceof XmlPattern p)
                    XmlPattern.patternFree(p);
            } while (cur != null);
        }
    }

    // xml_xsd10_free_element
    /** Port of xmlSchemaFreeElement. @param elem a schema element structure */
    static void freeElement(Xsd10Element elem)
    {
        if (elem == null)
            return;
        if (elem.annot != null)
            freeAnnot(elem.annot);
        if (elem.contModel instanceof XmlRegexp r)
            XmlRegexp.regexpFree(r);
        if (elem.defVal != null)
            freeValue(elem.defVal);
    }

    /* xmlSchemaFreeFacet is xml_xsd10_free_facet of xml_xsd10_types (the regexp is
       freed through the regexp hooks, that is xml_regexp_free). */

    // xml_xsd10_free_type
    /** Port of xmlSchemaFreeType. @param type a schema type structure */
    static void freeType(Xsd10Type type)
    {
        if (type == null)
            return;
        if (type.annot != null)
            freeAnnot(type.annot);
        if (type.facets != null)
        {
            Xsd10Facet facet, next;

            facet = type.facets;
            while (facet != null)
            {
                next = facet.next;
                freeFacet(facet);
                facet = next;
            }
        }
        if (type.attrUses instanceof Xsd10ItemList l)
            itemListFree(l);
        if (type.memberTypes != null)
            freeTypeLinkList(type.memberTypes);
        /* The facetSet links are garbage collected. */
        if (type.contModel instanceof XmlRegexp r)
            XmlRegexp.regexpFree(r);
    }

    // xml_xsd10_free_model_group_def
    /** Port of xmlSchemaFreeModelGroupDef. @param item a schema model group definition */
    static void freeModelGroupDef(Xsd10ModelGroupDef item)
    {
        if (item.annot != null)
            freeAnnot(item.annot);
    }

    // xml_xsd10_free_model_group
    /** Port of xmlSchemaFreeModelGroup. @param item a schema model group */
    static void freeModelGroup(Xsd10ModelGroup item)
    {
        if (item.annot != null)
            freeAnnot(item.annot);
    }

    // xml_xsd10_component_list_free
    /** Port of xmlSchemaComponentListFree. @param list the components */
    static void componentListFree(Xsd10ItemList list)
    {
        if ((list == null) || (list.nbItems == 0))
            return;
        {
            Object item;
            Object[] items = list.items;
            int i;

            for (i = 0; i < list.nbItems; i++)
            {
                item = items[i];
                if (!(item instanceof Xsd10BasicItem))
                    continue;
                switch (((Xsd10BasicItem) item).type)
                {
                case XML_XSD10_SCHEMA_TYPE_SIMPLE:
                case XML_XSD10_SCHEMA_TYPE_COMPLEX:
                    if (item instanceof Xsd10Type t) freeType(t);
                    break;
                case XML_XSD10_SCHEMA_TYPE_ATTRIBUTE:
                    if (item instanceof Xsd10Attribute a) freeAttribute(a);
                    break;
                case XML_XSD10_SCHEMA_TYPE_ATTRIBUTE_USE:
                    if (item instanceof Xsd10AttributeUse u) freeAttributeUse(u);
                    break;
                case XML_XSD10_SCHEMA_EXTRA_ATTR_USE_PROHIB:
                    if (item instanceof Xsd10AttributeUseProhib p) freeAttributeUseProhib(p);
                    break;
                case XML_XSD10_SCHEMA_TYPE_ELEMENT:
                    if (item instanceof Xsd10Element e) freeElement(e);
                    break;
                case XML_XSD10_SCHEMA_TYPE_PARTICLE:
                    if ((item instanceof Xsd10AnnotItem ai) && (ai.annot != null))
                        freeAnnot(ai.annot);
                    break;
                case XML_XSD10_SCHEMA_TYPE_SEQUENCE:
                case XML_XSD10_SCHEMA_TYPE_CHOICE:
                case XML_XSD10_SCHEMA_TYPE_ALL:
                    if (item instanceof Xsd10ModelGroup m) freeModelGroup(m);
                    break;
                case XML_XSD10_SCHEMA_TYPE_ATTRIBUTEGROUP:
                    if (item instanceof Xsd10AttributeGroup g) freeAttributeGroup(g);
                    break;
                case XML_XSD10_SCHEMA_TYPE_GROUP:
                    if (item instanceof Xsd10ModelGroupDef g) freeModelGroupDef(g);
                    break;
                case XML_XSD10_SCHEMA_TYPE_ANY:
                case XML_XSD10_SCHEMA_TYPE_ANY_ATTRIBUTE:
                    if (item instanceof Xsd10Wildcard w) freeWildcard(w);
                    break;
                case XML_XSD10_SCHEMA_TYPE_IDC_KEY:
                case XML_XSD10_SCHEMA_TYPE_IDC_UNIQUE:
                case XML_XSD10_SCHEMA_TYPE_IDC_KEYREF:
                    if (item instanceof Xsd10IDC c) freeIdc(c);
                    break;
                case XML_XSD10_SCHEMA_TYPE_NOTATION:
                    if (item instanceof Xsd10Notation n) freeNotation(n);
                    break;
                case XML_XSD10_SCHEMA_EXTRA_QNAMEREF:
                    if (item instanceof Xsd10QNameRef q) freeQNameRef(q);
                    break;
                default:
                    /* TODO: This should never be hit. */
                    break;
                }
            }
            list.nbItems = 0;
        }
    }

    // xml_xsd10_free
    /** Port of xmlSchemaFree: deallocate a Schema structure. @param schema a schema structure */
    public static void free(Xsd10Schema schema)
    {
        if (schema == null)
            return;
        /*
         * Note that those slots are not responsible for freeing
         * schema components anymore; this will now be done by
         * the schema buckets.
         */
        if (schema.notaDecl != null)
            hashFree(schema.notaDecl, null);
        if (schema.attrDecl != null)
            hashFree(schema.attrDecl, null);
        if (schema.attrgrpDecl != null)
            hashFree(schema.attrgrpDecl, null);
        if (schema.elemDecl != null)
            hashFree(schema.elemDecl, null);
        if (schema.typeDecl != null)
            hashFree(schema.typeDecl, null);
        if (schema.groupDecl != null)
            hashFree(schema.groupDecl, null);
        if (schema.idcDef != null)
            hashFree(schema.idcDef, null);

        if (schema.schemasImports != null)
            hashFree(schema.schemasImports, Xsd10Schemas::bucketFreeEntry);
        if (schema.includes != null)
        {
            Xsd10ItemList list = (Xsd10ItemList) schema.includes;
            int i;
            for (i = 0; i < list.nbItems; i++)
            {
                bucketFree((Xsd10Bucket) list.items[i]);
            }
            itemListFree(list);
        }
        if (schema.annot != null)
            freeAnnot(schema.annot);
        /* Never free the doc here, since this will be done by the buckets. */

        dictFree(schema.dict);
    }

    /* The debug functions (xmlSchemaDump and friends) are not ported. */

    /* ***********************************************************************
     *                                                                      *
     *                  Utilities                                           *
     *                                                                      *
     ************************************************************************/

    // xml_xsd10_get_prop_node
    /** Port of xmlSchemaGetPropNode: seeks an attribute with a name of name in no namespace.
        @param node the element node @param name the name of the attribute
        @return the attribute (as a node) or null if not present */
    static XmlNode getPropNode(XmlNode node, String name)
    {
        if ((node == null) || (name == null))
            return null;
        for (XmlAttr prop = nodeFirstAttr(node); prop != null; prop = attrNext(prop))
        {
            XmlNode an = attrNode(prop);
            if ((nsHrefOf(an) == null) && (strEqual(nodeName(an), name) != 0))
                return an;
        }
        return null;
    }

    // xml_xsd10_get_prop_node_ns
    /** Port of xmlSchemaGetPropNodeNs: seeks an attribute with a local name of name and a namespace
        URI of uri. @param node the element node @param uri the uri @param name the name of the
        attribute @return the attribute (as a node) or null if not present */
    static XmlNode getPropNodeNs(XmlNode node, String uri, String name)
    {
        if ((node == null) || (name == null))
            return null;
        for (XmlAttr prop = nodeFirstAttr(node); prop != null; prop = attrNext(prop))
        {
            XmlNode an = attrNode(prop);
            if ((nsHrefOf(an) != null) &&
                (strEqual(nodeName(an), name) != 0) &&
                (strEqual(nsHrefOf(an), uri) != 0))
                return an;
        }
        return null;
    }

    // xml_xsd10_get_node_content
    /** Port of xmlSchemaGetNodeContent: the content interned in the dict. @param ctxt context
        @param node node @return the content */
    static String getNodeContent(Xsd10ParserCtxt ctxt, XmlNode node)
    {
        String val;
        String ret;

        val = treeNodeGetContent(node);
        if (val == null)
            val = strdup("");
        ret = dictLookup(ctxt.dict, val, -1);
        if (ret == null)
            pErrMemory(ctxt);
        return ret;
    }

    // xml_xsd10_get_node_content_no_dict
    /** Port of xmlSchemaGetNodeContentNoDict. @param node node @return the content */
    static String getNodeContentNoDict(XmlNode node)
    {
        return treeNodeGetContent(node);
    }

    // xml_xsd10_get_prop
    /** Port of xmlSchemaGetProp: read a attribute value and internalize the string.
        @param ctxt the parser context @param node the node @param name the property name
        @return the string or null if not present */
    static String getProp(Xsd10ParserCtxt ctxt, XmlNode node, String name)
    {
        String val;

        val = treeGetNoNsProp(node, name);
        if (val == null)
            return null;
        return dictLookup(ctxt.dict, val, -1);
    }

    /* ***********************************************************************
     *                                                                      *
     *                  The context lifecycle                               *
     *                                                                      *
     ************************************************************************/

    // xml_xsd10_subst_group_free
    /** Port of xmlSchemaSubstGroupFree. @param group group */
    static void substGroupFree(Xsd10SubstGroup group)
    {
        if (group == null)
            return;
        if (group.members != null)
            itemListFree(group.members);
    }

    // xml_xsd10_subst_group_free_entry
    /** Port of xmlSchemaSubstGroupFreeEntry. @param group group @param name unused */
    static void substGroupFreeEntry(Object group, String name)
    {
        substGroupFree((Xsd10SubstGroup) group);
    }

    // xml_xsd10_redef_list_free
    /** Port of xmlSchemaRedefListFree (garbage collected). @param redef the list */
    static void redefListFree(Xsd10Redef redef)
    {
    }

    // xml_xsd10_construction_ctxt_free
    /** Port of xmlSchemaConstructionCtxtFree. @param con the construction context */
    static void constructionCtxtFree(Xsd10ConstructionCtxt con)
    {
        /*
         * After the construction context has been freed, there will be
         * no schema graph available any more. Only the schema buckets
         * will stay alive, which are put into the "schemasImports" and
         * "includes" slots of the xmlSchema.
         */
        if (con.buckets != null)
            itemListFree(con.buckets);
        if (con.pending != null)
            itemListFree(con.pending);
        if (con.substGroups != null)
            hashFree(con.substGroups, Xsd10Schemas::substGroupFreeEntry);
        if (con.redefs != null)
            redefListFree(con.redefs);
        if (con.dict != null)
            dictFree(con.dict);
    }

    // xml_xsd10_construction_ctxt_create
    /** Port of xmlSchemaConstructionCtxtCreate. @param dict dict @return the construction context */
    static Xsd10ConstructionCtxt constructionCtxtCreate(Xsd10Dict dict)
    {
        Xsd10ConstructionCtxt ret;

        ret = new Xsd10ConstructionCtxt();
        ret.buckets = itemListCreate();
        ret.pending = itemListCreate();
        ret.dict = dict;
        dictReference(dict);
        return ret;
    }

    // xml_xsd10_parser_ctxt_create
    /** Port of xmlSchemaParserCtxtCreate (also installs the regexp hooks of xml_xsd10_types).
        @return the parser context */
    static Xsd10ParserCtxt parserCtxtCreate()
    {
        Xsd10ParserCtxt ret;

        installRegexpHooks();
        ret = new Xsd10ParserCtxt();
        ret.type = XML_XSD10_SCHEMA_CTXT_PARSER;
        ret.attrProhibs = itemListCreate();
        return ret;
    }

    // xml_xsd10_new_parser_ctxt_use_dict
    /** Port of xmlSchemaNewParserCtxtUseDict: create an XML Schemas parse context for that
        file/resource expected to contain an XML Schemas file. @param URL the location of the schema
        @param dict the dictionary to be used @return the parser context */
    static Xsd10ParserCtxt newParserCtxtUseDict(String URL, Xsd10Dict dict)
    {
        Xsd10ParserCtxt ret;

        ret = parserCtxtCreate();
        if (ret == null)
            return null;
        ret.dict = dict;
        dictReference(dict);
        if (URL != null)
            ret.URL = dictLookup(dict, URL, -1);
        return ret;
    }

    // xml_xsd10_create_p_ctxt_on_v_ctxt
    /** Port of xmlSchemaCreatePCtxtOnVCtxt. @param vctxt the validation context @return 0 or -1 */
    static int createPCtxtOnVCtxt(Xsd10ValidCtxt vctxt)
    {
        if (vctxt.pctxt == null)
        {
            if (vctxt.schema != null)
                vctxt.pctxt =
                    newParserCtxtUseDict("*", vctxt.schema.dict);
            else
                vctxt.pctxt = newParserCtxt("*");
            if (vctxt.pctxt == null)
            {
                verrorInt(vctxt, "xmlSchemaCreatePCtxtOnVCtxt",
                          "failed to create a temp. parser context");
                return -1;
            }
            /* TODO: Pass user data. */
            setParserErrors(vctxt.pctxt, vctxt.error,
                            vctxt.warning, vctxt.errCtxt);
            setParserStructuredErrors(vctxt.pctxt, vctxt.serror,
                                      vctxt.errCtxt);
        }
        return 0;
    }

    /* ***********************************************************************
     *                                                                      *
     *                  Reading/Writing Schemas                             *
     *                                                                      *
     ************************************************************************/

    // xml_xsd10_new_parser_ctxt
    /** Port of xmlSchemaNewParserCtxt: create an XML Schemas parse context for that file/resource
        expected to contain an XML Schemas file. @param URL the location of the schema
        @return the parser context or null in case of error */
    public static Xsd10ParserCtxt newParserCtxt(String URL)
    {
        Xsd10ParserCtxt ret;

        if (URL == null)
            return null;

        ret = parserCtxtCreate();
        if (ret == null)
            return null;
        ret.dict = dictCreate();
        ret.URL = dictLookup(ret.dict, URL, -1);
        return ret;
    }

    // xml_xsd10_new_mem_parser_ctxt
    /** Port of xmlSchemaNewMemParserCtxt: create an XML Schemas parse context for that memory buffer
        expected to contain an XML Schemas file. @param buffer the schema text @param size its size
        (in UTF-8 bytes) @return the parser context or null in case of error */
    public static Xsd10ParserCtxt newMemParserCtxt(String buffer, int size)
    {
        Xsd10ParserCtxt ret;

        if ((buffer == null) || (size <= 0))
            return null;
        ret = parserCtxtCreate();
        if (ret == null)
            return null;
        ret.buffer = buffer;
        ret.size = size;
        ret.dict = dictCreate();
        return ret;
    }

    // xml_xsd10_new_doc_parser_ctxt
    /** Port of xmlSchemaNewDocParserCtxt: create an XML Schemas parse context for that document.
        NB. The document may be modified during the parsing process. @param doc a preparsed document
        tree @return the parser context or null in case of error */
    public static Xsd10ParserCtxt newDocParserCtxt(XmlDocument doc)
    {
        Xsd10ParserCtxt ret;

        if (doc == null)
            return null;
        ret = parserCtxtCreate();
        if (ret == null)
            return null;
        ret.doc = doc;
        ret.dict = dictCreate();
        /* The application has responsibility for the document */
        ret.preserve = 1;

        return ret;
    }

    // xml_xsd10_free_parser_ctxt
    /** Port of xmlSchemaFreeParserCtxt: free the resources associated to the schema parser context.
        @param ctxt the schema parser context */
    public static void freeParserCtxt(Xsd10ParserCtxt ctxt)
    {
        if (ctxt == null)
            return;
        /* ctxt->doc != NULL && !ctxt->preserve: xml_dom_free, garbage collected. */
        if (ctxt.vctxt != null)
        {
            freeValidCtxt(ctxt.vctxt);
        }
        if ((ctxt.ownsConstructor != 0) && (ctxt.constructor != null))
        {
            constructionCtxtFree(ctxt.constructor);
            ctxt.constructor = null;
            ctxt.ownsConstructor = 0;
        }
        if (ctxt.attrProhibs != null)
            itemListFree(ctxt.attrProhibs);
        dictFree(ctxt.dict);
    }

    // xml_xsd10_set_parser_errors
    /** Port of xmlSchemaSetParserErrors: set the callback functions used to handle errors for a
        validation context. @param ctxt a schema validation context @param err the error callback
        @param warn the warning callback @param ctx contextual data for the callbacks */
    public static void setParserErrors(Xsd10ParserCtxt ctxt, ValidityErrorFunc err, ValidityWarningFunc warn, Object ctx)
    {
        if (ctxt == null)
            return;
        ctxt.error = err;
        ctxt.warning = warn;
        ctxt.errCtxt = ctx;
        if (ctxt.vctxt != null)
            setValidErrors(ctxt.vctxt, err, warn, ctx);
    }

    // xml_xsd10_set_parser_structured_errors
    /** Port of xmlSchemaSetParserStructuredErrors: set the structured error callback.
        @param ctxt a schema parser context @param serror the structured error function
        @param ctx the functions context */
    public static void setParserStructuredErrors(Xsd10ParserCtxt ctxt, StructuredErrorFunc serror, Object ctx)
    {
        if (ctxt == null)
            return;
        ctxt.serror = serror;
        ctxt.errCtxt = ctx;
        if (ctxt.vctxt != null)
            setValidStructuredErrors(ctxt.vctxt, serror, ctx);
    }

    // xml_xsd10_get_parser_errors
    /** Port of xmlSchemaGetParserErrors: get the callback information used to handle errors for a
        parser context. @param ctxt a XMl-Schema parser context @param err the error callback result
        @param warn the warning callback result @param ctx contextual data for the callbacks result
        @return -1 in case of failure, 0 otherwise */
    public static int getParserErrors(Xsd10ParserCtxt ctxt, ValidityErrorFunc[] err, ValidityWarningFunc[] warn, Object[] ctx)
    {
        if (ctxt == null)
            return -1;
        if (err != null)
            err[0] = ctxt.error;
        if (warn != null)
            warn[0] = ctxt.warning;
        if (ctx != null)
            ctx[0] = ctxt.errCtxt;
        return 0;
    }

    // xml_xsd10_set_resource_loader
    /** Port of xmlSchemaSetResourceLoader: register a callback function that will be called to load
        documents or external entities. @param ctxt schema parser @param loader resource loader
        @param data user data which will be passed to the loader */
    public static void setResourceLoader(Xsd10ParserCtxt ctxt, Loader loader, Object data)
    {
        if (ctxt == null)
            return;
        ctxt.resourceLoader = loader;
        ctxt.resourceCtxt = data;
    }

    // xml_xsd10_facet_type_to_string
    /** Port of xmlSchemaFacetTypeToString: convert the facet type to a string. @param type the facet
        type @return the string representation of the facet type if the type is a facet and an
        "Internal Error" string otherwise */
    static String facetTypeToString(int type)
    {
        switch (type)
        {
        case XML_XSD10_SCHEMA_FACET_PATTERN:
            return "pattern";
        case XML_XSD10_SCHEMA_FACET_MAXEXCLUSIVE:
            return "maxExclusive";
        case XML_XSD10_SCHEMA_FACET_MAXINCLUSIVE:
            return "maxInclusive";
        case XML_XSD10_SCHEMA_FACET_MINEXCLUSIVE:
            return "minExclusive";
        case XML_XSD10_SCHEMA_FACET_MININCLUSIVE:
            return "minInclusive";
        case XML_XSD10_SCHEMA_FACET_WHITESPACE:
            return "whiteSpace";
        case XML_XSD10_SCHEMA_FACET_ENUMERATION:
            return "enumeration";
        case XML_XSD10_SCHEMA_FACET_LENGTH:
            return "length";
        case XML_XSD10_SCHEMA_FACET_MAXLENGTH:
            return "maxLength";
        case XML_XSD10_SCHEMA_FACET_MINLENGTH:
            return "minLength";
        case XML_XSD10_SCHEMA_FACET_TOTALDIGITS:
            return "totalDigits";
        case XML_XSD10_SCHEMA_FACET_FRACTIONDIGITS:
            return "fractionDigits";
        default:
            break;
        }
        return "Internal Error";
    }

    // xml_xsd10_get_white_space_facet_value
    /** Port of xmlSchemaGetWhiteSpaceFacetValue. @param type type @return the whitespace value or -1 */
    static int getWhiteSpaceFacetValue(Xsd10Type type)
    {
        /*
         * The normalization type can be changed only for types which are derived
         * from xsd:string.
         */
        if (type.type == XML_XSD10_SCHEMA_TYPE_BASIC)
        {
            /*
             * Note that we assume a whitespace of preserve for anySimpleType.
             */
            if ((type.builtInType == XML_XSD10_SCHEMAS_STRING) ||
                (type.builtInType == XML_XSD10_SCHEMAS_ANYSIMPLETYPE))
                return XML_XSD10_SCHEMA_WHITESPACE_PRESERVE;
            else if (type.builtInType == XML_XSD10_SCHEMAS_NORMSTRING)
                return XML_XSD10_SCHEMA_WHITESPACE_REPLACE;
            else
            {
                /*
                 * For all `atomic` datatypes other than string (and types `derived`
                 * by `restriction` from it) the value of whiteSpace is fixed to
                 * collapse
                 * Note that this includes built-in list datatypes.
                 */
                return XML_XSD10_SCHEMA_WHITESPACE_COLLAPSE;
            }
        }
        else if (wxsIsList(type) != 0)
        {
            /*
             * For list types the facet "whiteSpace" is fixed to "collapse".
             */
            return XML_XSD10_SCHEMA_WHITESPACE_COLLAPSE;
        }
        else if (wxsIsUnion(type) != 0)
        {
            return XML_XSD10_SCHEMA_WHITESPACE_UNKNOWN;
        }
        else if (wxsIsAtomic(type) != 0)
        {
            if ((type.flags & XML_XSD10_SCHEMAS_TYPE_WHITESPACE_PRESERVE) != 0)
                return XML_XSD10_SCHEMA_WHITESPACE_PRESERVE;
            else if ((type.flags & XML_XSD10_SCHEMAS_TYPE_WHITESPACE_REPLACE) != 0)
                return XML_XSD10_SCHEMA_WHITESPACE_REPLACE;
            else
                return XML_XSD10_SCHEMA_WHITESPACE_COLLAPSE;
        }
        return -1;
    }

    /* ***********************************************************************
     *                                                                      *
     *                  The validation context setters                      *
     *                                                                      *
     ************************************************************************/

    // xml_xsd10_is_valid
    /** Port of xmlSchemaIsValid: check if any error was detected during validation.
        @param ctxt the schema validation context @return 1 if valid so far, 0 if errors were
        detected, and -1 in case of internal error */
    public static int isValid(Xsd10ValidCtxt ctxt)
    {
        if (ctxt == null)
            return -1;
        return (ctxt.err == 0) ? 1 : 0;
    }

    // xml_xsd10_set_valid_errors
    /** Port of xmlSchemaSetValidErrors: set the error and warning callback information.
        @param ctxt a schema validation context @param err the error function @param warn the warning
        function @param ctx the functions context */
    public static void setValidErrors(Xsd10ValidCtxt ctxt, ValidityErrorFunc err, ValidityWarningFunc warn, Object ctx)
    {
        if (ctxt == null)
            return;
        ctxt.error = err;
        ctxt.warning = warn;
        ctxt.errCtxt = ctx;
        if (ctxt.pctxt != null)
            setParserErrors(ctxt.pctxt, err, warn, ctx);
    }

    // xml_xsd10_set_valid_structured_errors
    /** Port of xmlSchemaSetValidStructuredErrors: set the structured error callback.
        @param ctxt a schema validation context @param serror the structured error function
        @param ctx the functions context */
    public static void setValidStructuredErrors(Xsd10ValidCtxt ctxt, StructuredErrorFunc serror, Object ctx)
    {
        if (ctxt == null)
            return;
        ctxt.serror = serror;
        ctxt.error = null;
        ctxt.warning = null;
        ctxt.errCtxt = ctx;
        if (ctxt.pctxt != null)
            setParserStructuredErrors(ctxt.pctxt, serror, ctx);
    }

    // xml_xsd10_get_valid_errors
    /** Port of xmlSchemaGetValidErrors: get the error and warning callback information.
        @param ctxt a XML-Schema validation context @param err the error function result
        @param warn the warning function result @param ctx the functions context result
        @return -1 in case of error and 0 otherwise */
    public static int getValidErrors(Xsd10ValidCtxt ctxt, ValidityErrorFunc[] err, ValidityWarningFunc[] warn, Object[] ctx)
    {
        if (ctxt == null)
            return -1;
        if (err != null)
            err[0] = ctxt.error;
        if (warn != null)
            warn[0] = ctxt.warning;
        if (ctx != null)
            ctx[0] = ctxt.errCtxt;
        return 0;
    }

    // xml_xsd10_set_valid_options
    /** Port of xmlSchemaSetValidOptions: sets the options to be used during the validation.
        @param ctxt a schema validation context @param options a combination of xmlSchemaValidOption
        @return 0 in case of success, -1 in case of an API error */
    public static int setValidOptions(Xsd10ValidCtxt ctxt, int options)
    {
        int i;

        if (ctxt == null)
            return -1;
        /*
         * WARNING: Change the start value if adding to the
         * xmlSchemaValidOption.
         * TODO: Is there an other, more easy to maintain,
         * way?
         */
        for (i = 1; i < 32; i++)
        {
            if ((options & (1 << i)) != 0)
                return -1;
        }
        ctxt.options = options;
        return 0;
    }

    // xml_xsd10_valid_ctxt_get_options
    /** Port of xmlSchemaValidCtxtGetOptions: get the validation context options.
        @param ctxt a schema validation context @return the option combination or -1 on error */
    public static int validCtxtGetOptions(Xsd10ValidCtxt ctxt)
    {
        if (ctxt == null)
            return -1;
        else
            return ctxt.options;
    }
}
