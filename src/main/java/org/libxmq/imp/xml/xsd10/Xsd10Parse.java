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

import java.util.Objects;
import java.util.function.Function;

import org.libxmq.imp.xml.XmlDocument;
import org.libxmq.imp.xml.XmlNode;
import org.libxmq.imp.xml.XmlNs;
import org.libxmq.imp.xml.XmlPattern;
import org.libxmq.imp.xml.XmlUri;

import static org.libxmq.imp.xml.XmlDomApi.*;
import static org.libxmq.imp.xml.xsd10.Xsd10Const.*;
import static org.libxmq.imp.xml.xsd10.Xsd10Schemas.*;
import static org.libxmq.imp.xml.xsd10.Xsd10Build.*;
import static org.libxmq.imp.xml.xsd10.Xsd10Valid.*;
import static org.libxmq.imp.xml.xsd10.Xsd10Types.*;

/** Port of xml_xsd10_parse.c: parsing the schema documents into components. */
public final class Xsd10Parse
{
    private Xsd10Parse()
    {
    }

    /* Schema parsing (port of the parsing functions of libxml2 xmlschemas.c: parsing,
       utilities for parsing, schema extraction from an infoset, reading schemas). */

    private static final int XSD10_WXS_ATTR_DEF_VAL_DEFAULT = 1;
    private static final int XSD10_WXS_ATTR_DEF_VAL_FIXED = 2;

    /** A checked cast that yields null instead of throwing (C casts never fail). */
    private static <T> T as(Object o, Class<T> c)
    {
        return c.isInstance(o) ? c.cast(o) : null;
    }

    /** The character at i, 0 past the end (the C terminating zero). */
    private static char ch(String s, int i)
    {
        return (s != null && i >= 0 && i < s.length()) ? s.charAt(i) : 0;
    }

    // XSD10_IS_BLANK_CH
    private static boolean isBlankCh(int c)
    {
        return (c == 0x20) || ((0x9 <= c) && (c <= 0xa)) || (c == 0xd);
    }

    // XSD10_IS_BLANK_NODE
    private static boolean isBlankNode(XmlNode n)
    {
        return (nodeKind(n) == TEXT) && (isBlank(nodeContent(n), -1) != 0);
    }

    /*
     * libxml2 xmlSchemaCleanupDoc unlinks and frees the blank text nodes, comments,
     * processing instructions and entity references of a schema document before it
     * is parsed (it does not descend into an element whose first child is an entity
     * reference or declaration). The native port never modifies the schema documents,
     * so the parsing functions walk the children with xsd10_parse_first_child and
     * xsd10_parse_next, which skip exactly the nodes that libxml2 would have removed.
     */

    // xsd10_parse_cleanup_visits
    /* 1 if xmlSchemaCleanupDoc visits node: no ancestor element has an entity reference or
       declaration as first child. */
    private static int parseCleanupVisits(XmlNode node)
    {
        XmlNode p;

        for (p = nodeParent(node); (p != null) && (nodeKind(p) == ELEMENT); p = nodeParent(p))
        {
            XmlNode first = nodeFirstChild(p);
            if ((first != null) &&
                ((nodeKind(first) == ENTITY_DECL) ||
                 (nodeKind(first) == ENTITY_REF)))
                return 0;
        }
        return 1;
    }

    // xsd10_parse_removed_by_cleanup
    /* 1 if xmlSchemaCleanupDoc removes node. */
    private static int parseRemovedByCleanup(XmlNode cur)
    {
        if (nodeKind(cur) == TEXT)
        {
            if (isBlankNode(cur))
            {
                if (treeNodeGetSpacePreserve(cur) != 1)
                    return parseCleanupVisits(cur);
            }
            return 0;
        }
        else if ((nodeKind(cur) != ELEMENT) &&
                 (nodeKind(cur) != CDATA))
        {
            return parseCleanupVisits(cur);
        }
        return 0;
    }

    // xsd10_parse_first_child
    /* The first child that xmlSchemaCleanupDoc keeps (libxml2 node->children after the cleanup). */
    private static XmlNode parseFirstChild(XmlNode node)
    {
        XmlNode cur;

        if (node == null)
            return null;
        cur = nodeFirstChild(node);
        while ((cur != null) && (parseRemovedByCleanup(cur) != 0))
            cur = nodeNext(cur);
        return cur;
    }

    // xsd10_parse_next
    /* The next sibling that xmlSchemaCleanupDoc keeps (libxml2 node->next after the cleanup). */
    private static XmlNode parseNext(XmlNode node)
    {
        XmlNode cur;

        if (node == null)
            return null;
        cur = nodeNext(node);
        while ((cur != null) && (parseRemovedByCleanup(cur) != 0))
            cur = nodeNext(cur);
        return cur;
    }

    // xsd10_parse_first_attr
    /* The first attribute of an element as a node (libxml2 node->properties). */
    private static XmlNode parseFirstAttr(XmlNode node)
    {
        if ((node == null) || (nodeKind(node) != ELEMENT))
            return null;
        return attrNode(nodeFirstAttr(node));
    }

    // XSD10_WXS_FIND_GLOBAL_ITEM
    private static Object findGlobalItem(Xsd10Schema schema, Function<Xsd10Schema, Xsd10Hash> slot,
                                         String name, String nsName)
    {
        Object ret = null;

        if (strEqual(nsName, schema.targetNamespace) != 0)
        {
            ret = hashLookup(slot.apply(schema), name);
            if (ret != null) return ret;
        }
        if (hashSize(schema.schemasImports) > 1)
        {
            Xsd10Import imp;
            if (nsName == null)
                imp = as(hashLookup(schema.schemasImports, XML_XSD10_SCHEMAS_NO_NAMESPACE), Xsd10Import.class);
            else
                imp = as(hashLookup(schema.schemasImports, nsName), Xsd10Import.class);
            if (imp == null)
                return ret;
            if (imp.schema == null)
                return null;
            ret = hashLookup(slot.apply(imp.schema), name);
        }
        return ret;
    }

    // xml_xsd10_get_elem
    /** Port of xml_xsd10_get_elem. */
    static Xsd10Element getElem(Xsd10Schema schema, String name, String nsName)
    {
        if ((name == null) || (schema == null))
            return null;
        return as(findGlobalItem(schema, s -> s.elemDecl, name, nsName), Xsd10Element.class);
    }

    // xml_xsd10_get_type
    /** Port of xmlSchemaGetType. */
    static Xsd10Type getType(Xsd10Schema schema, String name, String nsName)
    {
        Xsd10Type ret = null;

        if (name == null)
            return null;
        /* First try the built-in types. */
        if ((nsName != null) && (strEqual(nsName, XML_XSD10_NS) != 0))
        {
            ret = getPredefinedType(name, nsName);
            if (ret != null)
                return ret;
            /*
            * Note that we try the parsed schemas as well here
            * since one might have parsed the S4S, which contain more
            * than the built-in types.
            */
        }
        if (schema != null)
        {
            ret = as(findGlobalItem(schema, s -> s.typeDecl, name, nsName), Xsd10Type.class);
        }
        return ret;
    }

    // xml_xsd10_get_attribute_decl
    /** Port of xmlSchemaGetAttributeDecl. */
    static Xsd10Attribute getAttributeDecl(Xsd10Schema schema, String name, String nsName)
    {
        if ((name == null) || (schema == null))
            return null;
        return as(findGlobalItem(schema, s -> s.attrDecl, name, nsName), Xsd10Attribute.class);
    }

    // xml_xsd10_get_attribute_group
    /** Port of xmlSchemaGetAttributeGroup. */
    static Xsd10AttributeGroup getAttributeGroup(Xsd10Schema schema, String name, String nsName)
    {
        if ((name == null) || (schema == null))
            return null;
        return as(findGlobalItem(schema, s -> s.attrgrpDecl, name, nsName), Xsd10AttributeGroup.class);
    }

    // xml_xsd10_get_group
    /** Port of xmlSchemaGetGroup. */
    static Xsd10ModelGroupDef getGroup(Xsd10Schema schema, String name, String nsName)
    {
        if ((name == null) || (schema == null))
            return null;
        return as(findGlobalItem(schema, s -> s.groupDecl, name, nsName), Xsd10ModelGroupDef.class);
    }

    // xml_xsd10_get_notation
    /** Port of xmlSchemaGetNotation. */
    static Xsd10Notation getNotation(Xsd10Schema schema, String name, String nsName)
    {
        if ((name == null) || (schema == null))
            return null;
        return as(findGlobalItem(schema, s -> s.notaDecl, name, nsName), Xsd10Notation.class);
    }

    // xml_xsd10_get_idc
    /** Port of xmlSchemaGetIDC. */
    static Xsd10IDC getIdc(Xsd10Schema schema, String name, String nsName)
    {
        if ((name == null) || (schema == null))
            return null;
        return as(findGlobalItem(schema, s -> s.idcDef, name, nsName), Xsd10IDC.class);
    }

    // xml_xsd10_get_named_component
    /** Port of xmlSchemaGetNamedComponent. */
    static Xsd10BasicItem getNamedComponent(Xsd10Schema schema, int itemType, String name, String targetNs)
    {
        switch (itemType)
        {
        case XML_XSD10_SCHEMA_TYPE_GROUP:
            return getGroup(schema, name, targetNs);
        case XML_XSD10_SCHEMA_TYPE_ELEMENT:
            return getElem(schema, name, targetNs);
        default:
            /* TODO */
            return null;
        }
    }

    // xml_xsd10_is_blank
    /** Port of xmlSchemaIsBlank. */
    static int isBlank(String str, int len)
    {
        if (str == null)
            return 1;
        int i = 0;
        if (len < 0)
        {
            while (ch(str, i) != 0)
            {
                if (!isBlankCh(ch(str, i)))
                    return 0;
                i++;
            }
        }
        else while ((ch(str, i) != 0) && (len != 0))
        {
            if (!isBlankCh(ch(str, i)))
                return 0;
            i++;
            len--;
        }
        return 1;
    }

    // xml_xsd10_find_redef_comp_in_graph
    /** Port of xmlSchemaFindRedefCompInGraph. */
    static Xsd10BasicItem findRedefCompInGraph(Xsd10Bucket bucket, int type, String name, String nsName)
    {
        Xsd10BasicItem ret;
        int i;

        if ((bucket == null) || (name == null))
            return null;
        if ((bucket.globals != null) && (bucket.globals.nbItems != 0))
        {
            /*
            * Search in global components.
            */
            for (i = 0; i < bucket.globals.nbItems; i++)
            {
                ret = as(bucket.globals.items[i], Xsd10BasicItem.class);
                if ((ret != null) && (ret.type == type))
                {
                    switch (type)
                    {
                    case XML_XSD10_SCHEMA_TYPE_COMPLEX:
                    case XML_XSD10_SCHEMA_TYPE_SIMPLE:
                        if ((ret instanceof Xsd10Type t) &&
                            Objects.equals(t.name, name) &&
                            Objects.equals(t.targetNamespace, nsName))
                        {
                            return ret;
                        }
                        break;
                    case XML_XSD10_SCHEMA_TYPE_GROUP:
                        if ((ret instanceof Xsd10ModelGroupDef g) &&
                            Objects.equals(g.name, name) &&
                            Objects.equals(g.targetNamespace, nsName))
                        {
                            return ret;
                        }
                        break;
                    case XML_XSD10_SCHEMA_TYPE_ATTRIBUTEGROUP:
                        if ((ret instanceof Xsd10AttributeGroup ag) &&
                            Objects.equals(ag.name, name) &&
                            Objects.equals(ag.targetNamespace, nsName))
                        {
                            return ret;
                        }
                        break;
                    default:
                        /* Should not be hit. */
                        return null;
                    }
                }
            }
        }
        /* subschemas: */
        /*
        * Process imported/included schemas.
        */
        if (bucket.relations != null)
        {
            Xsd10SchemaRelation rel = bucket.relations;

            /*
            * TODO: Marking the bucket will not avoid multiple searches
            * in the same schema, but avoids at least circularity.
            */
            bucket.flags |= XML_XSD10_SCHEMA_BUCKET_MARKED;
            do
            {
                if ((rel.bucket != null) &&
                    ((rel.bucket.flags & XML_XSD10_SCHEMA_BUCKET_MARKED) == 0))
                {
                    ret = findRedefCompInGraph(rel.bucket, type, name, nsName);
                    if (ret != null)
                        return ret;
                }
                rel = rel.next;
            } while (rel != null);
            bucket.flags ^= XML_XSD10_SCHEMA_BUCKET_MARKED;
        }
        return null;
    }

    // xml_xsd10_add_notation
    /** Port of xmlSchemaAddNotation. */
    static Xsd10Notation addNotation(Xsd10ParserCtxt ctxt, Xsd10Schema schema, String name, String nsName, XmlNode node)
    {
        Xsd10Notation ret;

        if ((ctxt == null) || (schema == null) || (name == null))
            return null;

        ret = new Xsd10Notation();
        ret.type = XML_XSD10_SCHEMA_TYPE_NOTATION;
        ret.name = name;
        ret.targetNamespace = nsName;
        /* TODO: do we need the node to be set?
        * ret->node = node;*/
        ret = wxsAddGlobal(ctxt, ret);
        return ret;
    }

    // xml_xsd10_add_attribute
    /** Port of xmlSchemaAddAttribute. */
    static Xsd10Attribute addAttribute(Xsd10ParserCtxt ctxt, Xsd10Schema schema, String name, String nsName, XmlNode node, int topLevel)
    {
        Xsd10Attribute ret;

        if ((ctxt == null) || (schema == null))
            return null;

        ret = new Xsd10Attribute();
        ret.type = XML_XSD10_SCHEMA_TYPE_ATTRIBUTE;
        ret.node = node;
        ret.name = name;
        ret.targetNamespace = nsName;

        if (topLevel != 0)
            ret = wxsAddGlobal(ctxt, ret);
        else
            ret = wxsAddLocal(ctxt, ret);
        wxsAddPending(ctxt, ret);
        return ret;
    }

    // xml_xsd10_add_attribute_use
    /** Port of xmlSchemaAddAttributeUse. */
    static Xsd10AttributeUse addAttributeUse(Xsd10ParserCtxt pctxt, XmlNode node)
    {
        Xsd10AttributeUse ret;

        if (pctxt == null)
            return null;

        ret = new Xsd10AttributeUse();
        ret.type = XML_XSD10_SCHEMA_TYPE_ATTRIBUTE_USE;
        ret.node = node;

        ret = wxsAddLocal(pctxt, ret);
        return ret;
    }

    // xml_xsd10_add_redef
    /** Port of xmlSchemaAddRedef. */
    static Xsd10Redef addRedef(Xsd10ParserCtxt pctxt, Xsd10Bucket targetBucket, Object item, String refName, String refTargetNs)
    {
        Xsd10Redef ret;

        ret = new Xsd10Redef();
        ret.item = (Xsd10BasicItem) item;
        ret.targetBucket = targetBucket;
        ret.refName = refName;
        ret.refTargetNs = refTargetNs;
        if (wxsConstructor(pctxt).redefs == null)
            wxsConstructor(pctxt).redefs = ret;
        else
            wxsConstructor(pctxt).lastRedef.next = ret;
        wxsConstructor(pctxt).lastRedef = ret;

        return ret;
    }

    // xml_xsd10_add_attribute_group_definition
    /** Port of xmlSchemaAddAttributeGroupDefinition. */
    static Xsd10AttributeGroup addAttributeGroupDefinition(Xsd10ParserCtxt pctxt, Xsd10Schema schema, String name, String nsName, XmlNode node)
    {
        Xsd10AttributeGroup ret;

        if ((pctxt == null) || (name == null))
            return null;

        ret = new Xsd10AttributeGroup();
        ret.type = XML_XSD10_SCHEMA_TYPE_ATTRIBUTEGROUP;
        ret.name = name;
        ret.targetNamespace = nsName;
        ret.node = node;

        /* TODO: Remove the flag. */
        ret.flags |= XML_XSD10_SCHEMAS_ATTRGROUP_GLOBAL;
        if (pctxt.isRedefine != 0)
        {
            pctxt.redef = addRedef(pctxt, pctxt.redefined, ret, name, nsName);
            if (pctxt.redef == null)
                return null;
            pctxt.redefCounter = 0;
        }
        ret = wxsAddGlobal(pctxt, ret);
        wxsAddPending(pctxt, ret);
        return ret;
    }

    // xml_xsd10_add_element
    /** Port of xmlSchemaAddElement. */
    static Xsd10Element addElement(Xsd10ParserCtxt ctxt, String name, String nsName, XmlNode node, int topLevel)
    {
        Xsd10Element ret;

        if ((ctxt == null) || (name == null))
            return null;

        ret = new Xsd10Element();
        ret.type = XML_XSD10_SCHEMA_TYPE_ELEMENT;
        ret.name = name;
        ret.targetNamespace = nsName;
        ret.node = node;

        if (topLevel != 0)
            ret = wxsAddGlobal(ctxt, ret);
        else
            ret = wxsAddLocal(ctxt, ret);
        wxsAddPending(ctxt, ret);
        return ret;
    }

    // xml_xsd10_add_type
    /** Port of xmlSchemaAddType. */
    static Xsd10Type addType(Xsd10ParserCtxt ctxt, Xsd10Schema schema, int type, String name, String nsName, XmlNode node, int topLevel)
    {
        Xsd10Type ret;

        if ((ctxt == null) || (schema == null))
            return null;

        ret = new Xsd10Type();
        ret.type = type;
        ret.name = name;
        ret.targetNamespace = nsName;
        ret.node = node;
        if (topLevel != 0)
        {
            if (ctxt.isRedefine != 0)
            {
                ctxt.redef = addRedef(ctxt, ctxt.redefined, ret, name, nsName);
                if (ctxt.redef == null)
                    return null;
                ctxt.redefCounter = 0;
            }
            ret = wxsAddGlobal(ctxt, ret);
        }
        else
            ret = wxsAddLocal(ctxt, ret);
        wxsAddPending(ctxt, ret);
        return ret;
    }

    // xml_xsd10_new_q_name_ref
    /** Port of xmlSchemaNewQNameRef. */
    static Xsd10QNameRef newQNameRef(Xsd10ParserCtxt pctxt, int refType, String refName, String refNs)
    {
        Xsd10QNameRef ret;

        ret = new Xsd10QNameRef();
        ret.node = null;
        ret.type = XML_XSD10_SCHEMA_EXTRA_QNAMEREF;
        ret.name = refName;
        ret.targetNamespace = refNs;
        ret.item = null;
        ret.itemType = refType;
        /*
        * Store the reference item in the schema.
        */
        ret = wxsAddLocal(pctxt, ret);
        return ret;
    }

    // xml_xsd10_add_attribute_use_prohib
    /** Port of xmlSchemaAddAttributeUseProhib. */
    static Xsd10AttributeUseProhib addAttributeUseProhib(Xsd10ParserCtxt pctxt)
    {
        Xsd10AttributeUseProhib ret;

        ret = new Xsd10AttributeUseProhib();
        ret.type = XML_XSD10_SCHEMA_EXTRA_ATTR_USE_PROHIB;
        ret = wxsAddLocal(pctxt, ret);
        return ret;
    }

    // xml_xsd10_add_model_group
    /** Port of xmlSchemaAddModelGroup. */
    static Xsd10ModelGroup addModelGroup(Xsd10ParserCtxt ctxt, Xsd10Schema schema, int type, XmlNode node)
    {
        Xsd10ModelGroup ret;

        if ((ctxt == null) || (schema == null))
            return null;

        ret = new Xsd10ModelGroup();
        ret.type = type;
        ret.node = node;
        ret = wxsAddLocal(ctxt, ret);
        if ((type == XML_XSD10_SCHEMA_TYPE_SEQUENCE) ||
            (type == XML_XSD10_SCHEMA_TYPE_CHOICE))
            wxsAddPending(ctxt, ret);
        return ret;
    }

    // xml_xsd10_add_particle
    /** Port of xmlSchemaAddParticle. */
    static Xsd10Particle addParticle(Xsd10ParserCtxt ctxt, XmlNode node, int min, int max)
    {
        Xsd10Particle ret;
        if (ctxt == null)
            return null;

        ret = new Xsd10Particle();
        ret.type = XML_XSD10_SCHEMA_TYPE_PARTICLE;
        ret.annot = null;
        ret.node = node;
        ret.minOccurs = min;
        ret.maxOccurs = max;
        ret.next = null;
        ret.children = null;

        ret = wxsAddLocal(ctxt, ret);
        /*
        * Note that addition to pending components will be done locally
        * to the specific parsing function, since the most particles
        * need not to be fixed up (i.e. the reference to be resolved).
        * REMOVED: WXS_ADD_PENDING(ctxt, ret);
        */
        return ret;
    }

    // xml_xsd10_add_model_group_definition
    /** Port of xmlSchemaAddModelGroupDefinition. */
    static Xsd10ModelGroupDef addModelGroupDefinition(Xsd10ParserCtxt ctxt, Xsd10Schema schema, String name, String nsName, XmlNode node)
    {
        Xsd10ModelGroupDef ret;

        if ((ctxt == null) || (schema == null) || (name == null))
            return null;

        ret = new Xsd10ModelGroupDef();
        ret.name = name;
        ret.type = XML_XSD10_SCHEMA_TYPE_GROUP;
        ret.node = node;
        ret.targetNamespace = nsName;

        if (ctxt.isRedefine != 0)
        {
            ctxt.redef = addRedef(ctxt, ctxt.redefined, ret, name, nsName);
            if (ctxt.redef == null)
                return null;
            ctxt.redefCounter = 0;
        }
        ret = wxsAddGlobal(ctxt, ret);
        wxsAddPending(ctxt, ret);
        return ret;
    }

    // xml_xsd10_new_wildcard_ns_constraint
    /** Port of xmlSchemaNewWildcardNsConstraint. */
    static Xsd10WildcardNs newWildcardNsConstraint(Xsd10ParserCtxt ctxt)
    {
        Xsd10WildcardNs ret = new Xsd10WildcardNs();
        ret.value = null;
        ret.next = null;
        return ret;
    }

    // xml_xsd10_add_idc
    /** Port of xmlSchemaAddIDC. */
    static Xsd10IDC addIdc(Xsd10ParserCtxt ctxt, Xsd10Schema schema, String name, String nsName, int category, XmlNode node)
    {
        Xsd10IDC ret;

        if ((ctxt == null) || (schema == null) || (name == null))
            return null;

        ret = new Xsd10IDC();
        /* The target namespace of the parent element declaration. */
        ret.targetNamespace = nsName;
        ret.name = name;
        ret.type = category;
        ret.node = node;

        ret = wxsAddGlobal(ctxt, ret);
        /*
        * Only keyrefs need to be fixup up.
        */
        if (category == XML_XSD10_SCHEMA_TYPE_IDC_KEYREF)
            wxsAddPending(ctxt, ret);
        return ret;
    }

    // xml_xsd10_add_wildcard
    /** Port of xmlSchemaAddWildcard. */
    static Xsd10Wildcard addWildcard(Xsd10ParserCtxt ctxt, Xsd10Schema schema, int type, XmlNode node)
    {
        Xsd10Wildcard ret;

        if ((ctxt == null) || (schema == null))
            return null;

        ret = new Xsd10Wildcard();
        ret.type = type;
        ret.node = node;
        ret = wxsAddLocal(ctxt, ret);
        return ret;
    }

    // xml_xsd10_subst_group_add
    /** Port of xmlSchemaSubstGroupAdd. */
    static Xsd10SubstGroup substGroupAdd(Xsd10ParserCtxt pctxt, Xsd10Element head)
    {
        Xsd10SubstGroup ret;

        /* Init subst group hash. */
        if (wxsSubstGroups(pctxt) == null)
        {
            wxsConstructor(pctxt).substGroups = hashCreateDict(10, pctxt.dict);
            if (wxsSubstGroups(pctxt) == null)
                return null;
        }
        /* Create a new substitution group. */
        ret = new Xsd10SubstGroup();
        ret.head = head;
        /* Create list of members. */
        ret.members = itemListCreate();
        if (ret.members == null)
        {
            substGroupFree(ret);
            return null;
        }
        /* Add subst group to hash. */
        if (hashAddEntry2(wxsSubstGroups(pctxt), head.name, head.targetNamespace, ret) != 0)
        {
            perrorInt(pctxt, "xmlSchemaSubstGroupAdd",
                "failed to add a new substitution container");
            substGroupFree(ret);
            return null;
        }
        return ret;
    }

    // xml_xsd10_subst_group_get
    /** Port of xmlSchemaSubstGroupGet. */
    static Xsd10SubstGroup substGroupGet(Xsd10ParserCtxt pctxt, Xsd10Element head)
    {
        if (wxsSubstGroups(pctxt) == null)
            return null;
        return as(hashLookup2(wxsSubstGroups(pctxt), head.name, head.targetNamespace), Xsd10SubstGroup.class);
    }

    // xml_xsd10_add_element_substitution_member
    /** Port of xmlSchemaAddElementSubstitutionMember. */
    static int addElementSubstitutionMember(Xsd10ParserCtxt pctxt, Xsd10Element head, Xsd10Element member)
    {
        Xsd10SubstGroup substGroup;

        if ((pctxt == null) || (head == null) || (member == null))
            return -1;

        substGroup = substGroupGet(pctxt, head);
        if (substGroup == null)
            substGroup = substGroupAdd(pctxt, head);
        if (substGroup == null)
            return -1;
        if (itemListAdd(substGroup.members, member) == -1)
            return -1;
        return 0;
    }

    /* ***********************************************************************
     *                                                                      *
     *              Utilities for parsing                                   *
     *                                                                      *
     ************************************************************************/

    // xml_xsd10_p_val_attr_node_q_name_value
    /** Port of xmlSchemaPValAttrNodeQNameValue. Arrays: {@code uri} is C {@code const char **}; {@code local} is C {@code const char **} (an out param: caller allocated array of length 1; a real C array: the array). */
    static int pValAttrNodeQNameValue(Xsd10ParserCtxt ctxt, Xsd10Schema schema, Xsd10BasicItem ownerItem, XmlNode attr, String value, String[] uri, String[] local)
    {
        String pref;
        XmlNs ns;
        int[] len = { 0 };
        int ret;

        uri[0] = null;
        local[0] = null;
        ret = treeValidateQname(value, 1);
        if (ret > 0)
        {
            pSimpleTypeErr(ctxt,
                XML_XSD10_SCHEMAP_S4S_ATTR_INVALID_VALUE,
                ownerItem, attr,
                getBuiltInType(XML_XSD10_SCHEMAS_QNAME),
                null, value, null, null, null);
            local[0] = value;
            return ctxt.err;
        }
        else if (ret < 0)
            return -1;

        if ((value == null) || (value.indexOf(':') < 0))
        {
            ns = treeSearchNs(nodeParent(attr), null);
            if ((ns != null) && (nsHref(ns) != null) && !nsHref(ns).isEmpty())
                uri[0] = dictLookup(ctxt.dict, nsHref(ns), -1);
            else if ((schema.flags & XML_XSD10_SCHEMAS_INCLUDING_CONVERT_NS) != 0)
            {
                /* TODO: move XML_SCHEMAS_INCLUDING_CONVERT_NS to the
                * parser context. */
                /*
                * This one takes care of included schemas with no
                * target namespace.
                */
                uri[0] = ctxt.targetNamespace;
            }
            local[0] = dictLookup(ctxt.dict, value, -1);
            return 0;
        }
        /*
        * At this point xmlSplitQName3 has to return a local name.
        */
        local[0] = treeSplitQname3(value, len);
        local[0] = dictLookup(ctxt.dict, local[0], -1);
        pref = dictLookup(ctxt.dict, value, len[0]);
        ns = treeSearchNs(nodeParent(attr), pref);
        if (ns == null)
        {
            pSimpleTypeErr(ctxt,
                XML_XSD10_SCHEMAP_S4S_ATTR_INVALID_VALUE,
                ownerItem, attr,
                getBuiltInType(XML_XSD10_SCHEMAS_QNAME), null, value,
                "The value '%s' of simple type 'xs:QName' has no " +
                "corresponding namespace declaration in scope", value, null);
            return ctxt.err;
        }
        else
        {
            uri[0] = dictLookup(ctxt.dict, nsHref(ns), -1);
        }
        return 0;
    }

    // xml_xsd10_p_val_attr_node_q_name
    /** Port of xmlSchemaPValAttrNodeQName. Arrays: {@code uri} is C {@code const char **}; {@code local} is C {@code const char **} (an out param: caller allocated array of length 1; a real C array: the array). */
    static int pValAttrNodeQName(Xsd10ParserCtxt ctxt, Xsd10Schema schema, Xsd10BasicItem ownerItem, XmlNode attr, String[] uri, String[] local)
    {
        String value;

        value = getNodeContent(ctxt, attr);
        return pValAttrNodeQNameValue(ctxt, schema, ownerItem, attr, value, uri, local);
    }

    // xml_xsd10_p_val_attr_q_name
    /** Port of xmlSchemaPValAttrQName. Arrays: {@code uri} is C {@code const char **}; {@code local} is C {@code const char **} (an out param: caller allocated array of length 1; a real C array: the array). */
    static int pValAttrQName(Xsd10ParserCtxt ctxt, Xsd10Schema schema, Xsd10BasicItem ownerItem, XmlNode ownerElem, String name, String[] uri, String[] local)
    {
        XmlNode attr;

        attr = getPropNode(ownerElem, name);
        if (attr == null)
        {
            local[0] = null;
            uri[0] = null;
            return 0;
        }
        return pValAttrNodeQName(ctxt, schema, ownerItem, attr, uri, local);
    }

    // xml_xsd10_p_val_attr_node_id
    /** Port of xmlSchemaPValAttrNodeID. */
    static int pValAttrNodeId(Xsd10ParserCtxt ctxt, XmlNode attr)
    {
        int ret;
        String value;

        if (attr == null)
            return 0;
        value = getNodeContentNoDict(attr);
        ret = treeValidateNcname(value, 1);
        if (ret == 0)
        {
            /*
            * NOTE: the IDness might have already be declared in the DTD
            */
            Xsd10Bucket bucket = wxsBucket(ctxt);

            /* libxml2 registers the IDs in the document (attr->atype, xmlAddIDSafe),
               the native port in the bucket of the schema document. */
            if ((bucket != null) && (bucket.ids == null))
                bucket.ids = idsNew();
            if ((bucket == null) || (bucket.ids == null))
            {
                pErrMemory(ctxt);
            }
            else if (idsAttrType(bucket.ids, attr) != XML_XSD10_ATTRIBUTE_ID)
            {
                String strip;
                int res;

                /*
                * TODO: Use xmlSchemaStrip here; it's not exported at this
                * moment.
                */
                strip = collapseString(value);
                if (strip != null)
                {
                    value = strip;
                }
                res = idsAddId(bucket.ids, attr, value);

                if (res < 0)
                {
                    pErrMemory(ctxt);
                }
                else if (res == 0)
                {
                    ret = XML_XSD10_SCHEMAP_S4S_ATTR_INVALID_VALUE;
                    pSimpleTypeErr(ctxt,
                        XML_XSD10_SCHEMAP_S4S_ATTR_INVALID_VALUE,
                        null, attr,
                        getBuiltInType(XML_XSD10_SCHEMAS_ID),
                        null, null, "Duplicate value '%s' of simple " +
                        "type 'xs:ID'", value, null);
                }
            }
        }
        else if (ret > 0)
        {
            ret = XML_XSD10_SCHEMAP_S4S_ATTR_INVALID_VALUE;
            pSimpleTypeErr(ctxt,
                XML_XSD10_SCHEMAP_S4S_ATTR_INVALID_VALUE,
                null, attr,
                getBuiltInType(XML_XSD10_SCHEMAS_ID),
                null, null, "The value '%s' of simple type 'xs:ID' is " +
                "not a valid 'xs:NCName'",
                value, null);
        }
        return ret;
    }

    // xml_xsd10_p_val_attr_id
    /** Port of xmlSchemaPValAttrID. */
    static int pValAttrId(Xsd10ParserCtxt ctxt, XmlNode ownerElem, String name)
    {
        XmlNode attr;

        attr = getPropNode(ownerElem, name);
        if (attr == null)
            return 0;
        return pValAttrNodeId(ctxt, attr);
    }

    /** The digits loop of xmlGetMaxOccurs/xmlGetMinOccurs: pos[0] is the cursor. */
    private static int parseOccursDigits(String val, int[] pos)
    {
        int ret = 0;
        int cur = pos[0];
        while ((ch(val, cur) >= '0') && (ch(val, cur) <= '9'))
        {
            if (ret > Integer.MAX_VALUE / 10)
            {
                ret = Integer.MAX_VALUE;
            }
            else
            {
                int digit = ch(val, cur) - '0';
                ret *= 10;
                if (ret > Integer.MAX_VALUE - digit)
                    ret = Integer.MAX_VALUE;
                else
                    ret += digit;
            }
            cur++;
        }
        pos[0] = cur;
        return ret;
    }

    // xml_xsd10_get_max_occurs
    /** Port of xmlGetMaxOccurs. */
    static int getMaxOccurs(Xsd10ParserCtxt ctxt, XmlNode node, int min, int max, int def, String expected)
    {
        String val;
        int cur;
        int ret;
        XmlNode attr;

        attr = getPropNode(node, "maxOccurs");
        if (attr == null)
            return def;
        val = getNodeContent(ctxt, attr);
        if (val == null)
            return def;

        if (strEqual(val, "unbounded") != 0)
        {
            if (max != XML_XSD10_UNBOUNDED)
            {
                pSimpleTypeErr(ctxt,
                    XML_XSD10_SCHEMAP_S4S_ATTR_INVALID_VALUE,
                    /* XML_SCHEMAP_INVALID_MINOCCURS, */
                    null, attr, null, expected,
                    val, null, null, null);
                return def;
            }
            else
                return XML_XSD10_UNBOUNDED;  /* encoding it with -1 might be another option */
        }

        cur = 0;
        while (isBlankCh(ch(val, cur)))
            cur++;
        if (ch(val, cur) == 0)
        {
            pSimpleTypeErr(ctxt,
                XML_XSD10_SCHEMAP_S4S_ATTR_INVALID_VALUE,
                /* XML_SCHEMAP_INVALID_MINOCCURS, */
                null, attr, null, expected,
                val, null, null, null);
            return def;
        }
        int[] pos = { cur };
        ret = parseOccursDigits(val, pos);
        cur = pos[0];
        while (isBlankCh(ch(val, cur)))
            cur++;
        /*
        * TODO: Restrict the maximal value to Integer.
        */
        if ((ch(val, cur) != 0) || (ret < min) || ((max != -1) && (ret > max)))
        {
            pSimpleTypeErr(ctxt,
                XML_XSD10_SCHEMAP_S4S_ATTR_INVALID_VALUE,
                /* XML_SCHEMAP_INVALID_MINOCCURS, */
                null, attr, null, expected,
                val, null, null, null);
            return def;
        }
        return ret;
    }

    // xml_xsd10_get_min_occurs
    /** Port of xmlGetMinOccurs. */
    static int getMinOccurs(Xsd10ParserCtxt ctxt, XmlNode node, int min, int max, int def, String expected)
    {
        String val;
        int cur;
        int ret;
        XmlNode attr;

        attr = getPropNode(node, "minOccurs");
        if (attr == null)
            return def;
        val = getNodeContent(ctxt, attr);
        if (val == null)
            return def;
        cur = 0;
        while (isBlankCh(ch(val, cur)))
            cur++;
        if (ch(val, cur) == 0)
        {
            pSimpleTypeErr(ctxt,
                XML_XSD10_SCHEMAP_S4S_ATTR_INVALID_VALUE,
                /* XML_SCHEMAP_INVALID_MINOCCURS, */
                null, attr, null, expected,
                val, null, null, null);
            return def;
        }
        int[] pos = { cur };
        ret = parseOccursDigits(val, pos);
        cur = pos[0];
        while (isBlankCh(ch(val, cur)))
            cur++;
        /*
        * TODO: Restrict the maximal value to Integer.
        */
        if ((ch(val, cur) != 0) || (ret < min) || ((max != -1) && (ret > max)))
        {
            pSimpleTypeErr(ctxt,
                XML_XSD10_SCHEMAP_S4S_ATTR_INVALID_VALUE,
                /* XML_SCHEMAP_INVALID_MINOCCURS, */
                null, attr, null, expected,
                val, null, null, null);
            return def;
        }
        return ret;
    }

    // xml_xsd10_p_get_bool_node_value
    /** Port of xmlSchemaPGetBoolNodeValue. */
    static int pGetBoolNodeValue(Xsd10ParserCtxt ctxt, Xsd10BasicItem ownerItem, XmlNode node)
    {
        String value;
        int res = 0;

        value = treeNodeGetContent(node);
        /*
        * 3.2.2.1 Lexical representation
        * An instance of a datatype that is defined as `boolean`
        * can have the following legal literals {true, false, 1, 0}.
        */
        if (strEqual(value, "true") != 0)
            res = 1;
        else if (strEqual(value, "false") != 0)
            res = 0;
        else if (strEqual(value, "1") != 0)
            res = 1;
        else if (strEqual(value, "0") != 0)
            res = 0;
        else
        {
            pSimpleTypeErr(ctxt,
                XML_XSD10_SCHEMAP_INVALID_BOOLEAN,
                ownerItem, node,
                getBuiltInType(XML_XSD10_SCHEMAS_BOOLEAN),
                null, value,
                null, null, null);
        }
        return res;
    }

    // xml_xsd10_get_boolean_prop
    /** Port of xmlGetBooleanProp. */
    static int getBooleanProp(Xsd10ParserCtxt ctxt, XmlNode node, String name, int def)
    {
        String val;

        val = getProp(ctxt, node, name);
        if (val == null)
            return def;
        /*
        * 3.2.2.1 Lexical representation
        * An instance of a datatype that is defined as `boolean`
        * can have the following legal literals {true, false, 1, 0}.
        */
        if (strEqual(val, "true") != 0)
            def = 1;
        else if (strEqual(val, "false") != 0)
            def = 0;
        else if (strEqual(val, "1") != 0)
            def = 1;
        else if (strEqual(val, "0") != 0)
            def = 0;
        else
        {
            pSimpleTypeErr(ctxt,
                XML_XSD10_SCHEMAP_INVALID_BOOLEAN,
                null,
                getPropNode(node, name),
                getBuiltInType(XML_XSD10_SCHEMAS_BOOLEAN),
                null, val, null, null, null);
        }
        return def;
    }

    /* ***********************************************************************
     *                                                                      *
     *              Schema extraction from an Infoset                       *
     *                                                                      *
     ************************************************************************/

    // xml_xsd10_p_val_attr_node_value
    /** Port of xmlSchemaPValAttrNodeValue. */
    static int pValAttrNodeValue(Xsd10ParserCtxt pctxt, Xsd10BasicItem ownerItem, XmlNode attr, String value, Xsd10Type type)
    {
        int ret = 0;

        /*
        * NOTE: Should we move this to xmlschematypes.c? Hmm, but this
        * one is really meant to be used internally, so better not.
        */
        if ((pctxt == null) || (type == null) || (attr == null))
            return -1;
        if (type.type != XML_XSD10_SCHEMA_TYPE_BASIC)
        {
            perrorInt(pctxt, "xmlSchemaPValAttrNodeValue",
                "the given type is not a built-in type");
            return -1;
        }
        switch (type.builtInType)
        {
        case XML_XSD10_SCHEMAS_NCNAME:
        case XML_XSD10_SCHEMAS_QNAME:
        case XML_XSD10_SCHEMAS_ANYURI:
        case XML_XSD10_SCHEMAS_TOKEN:
        case XML_XSD10_SCHEMAS_LANGUAGE:
            ret = valPredefTypeNode(type, value, null, attr);
            break;
        default:
            perrorInt(pctxt, "xmlSchemaPValAttrNodeValue",
                "validation using the given type is not supported while " +
                "parsing a schema");
            return -1;
        }
        /*
        * TODO: Should we use the S4S error codes instead?
        */
        if (ret < 0)
        {
            perrorInt(pctxt, "xmlSchemaPValAttrNodeValue",
                "failed to validate a schema attribute value");
            return -1;
        }
        else if (ret > 0)
        {
            if (wxsIsList(type) != 0)
                ret = XML_XSD10_SCHEMAV_CVC_DATATYPE_VALID_1_2_2;
            else
                ret = XML_XSD10_SCHEMAV_CVC_DATATYPE_VALID_1_2_1;
            pSimpleTypeErr(pctxt,
                ret, ownerItem, attr,
                type, null, value, null, null, null);
        }
        return ret;
    }

    // xml_xsd10_p_val_attr_node
    /** Port of xmlSchemaPValAttrNode. Arrays: {@code value} is C {@code const char **} (an out param: caller allocated array of length 1; a real C array: the array). */
    static int pValAttrNode(Xsd10ParserCtxt ctxt, Xsd10BasicItem ownerItem, XmlNode attr, Xsd10Type type, String[] value)
    {
        String val;

        if ((ctxt == null) || (type == null) || (attr == null))
            return -1;

        val = getNodeContent(ctxt, attr);
        if (value != null)
            value[0] = val;

        return pValAttrNodeValue(ctxt, ownerItem, attr, val, type);
    }

    // xml_xsd10_p_val_attr
    /** Port of xmlSchemaPValAttr. Arrays: {@code value} is C {@code const char **} (an out param: caller allocated array of length 1; a real C array: the array). */
    static int pValAttr(Xsd10ParserCtxt ctxt, Xsd10BasicItem ownerItem, XmlNode ownerElem, String name, Xsd10Type type, String[] value)
    {
        XmlNode attr;

        if ((ctxt == null) || (type == null))
        {
            if (value != null)
                value[0] = null;
            return -1;
        }
        if (type.type != XML_XSD10_SCHEMA_TYPE_BASIC)
        {
            if (value != null)
                value[0] = null;
            pErr(ctxt, ownerElem,
                XML_XSD10_SCHEMAP_INTERNAL,
                "Internal error: xmlSchemaPValAttr, the given " +
                "type '%s' is not a built-in type.\n",
                type.name, null);
            return -1;
        }
        attr = getPropNode(ownerElem, name);
        if (attr == null)
        {
            if (value != null)
                value[0] = null;
            return 0;
        }
        return pValAttrNode(ctxt, ownerItem, attr, type, value);
    }

    // xml_xsd10_check_reference
    /** Port of xmlSchemaCheckReference. */
    static int checkReference(Xsd10ParserCtxt pctxt, Xsd10Schema schema, XmlNode node, XmlNode attr, String namespaceName)
    {
        /* TODO: Pointer comparison instead? */
        if (strEqual(pctxt.targetNamespace, namespaceName) != 0)
            return 0;
        if (strEqual(XML_XSD10_NS, namespaceName) != 0)
            return 0;
        /*
        * Check if the referenced namespace was <import>ed.
        */
        if (wxsBucket(pctxt).relations != null)
        {
            Xsd10SchemaRelation rel;

            rel = wxsBucket(pctxt).relations;
            do
            {
                if ((wxsIsBucketImpmain(rel.type) != 0) &&
                    (strEqual(namespaceName, rel.importNamespace) != 0))
                    return 0;
                rel = rel.next;
            } while (rel != null);
        }
        /*
        * No matching <import>ed namespace found.
        */
        {
            XmlNode n = (attr != null) ? attr : node;

            if (namespaceName == null)
                customErr(pctxt,
                    XML_XSD10_SCHEMAP_SRC_RESOLVE, n, null,
                    "References from this schema to components in no " +
                    "namespace are not allowed, since not indicated by an " +
                    "import statement", null, null);
            else
                customErr(pctxt,
                    XML_XSD10_SCHEMAP_SRC_RESOLVE, n, null,
                    "References from this schema to components in the " +
                    "namespace '%s' are not allowed, since not indicated by an " +
                    "import statement", namespaceName, null);
        }
        return XML_XSD10_SCHEMAP_SRC_RESOLVE;
    }

    // xml_xsd10_parse_local_attributes
    /** Port of xmlSchemaParseLocalAttributes. Arrays: {@code child} is C {@code XMQNode **}; {@code list} is C {@code XmlXsd10ItemList **}; {@code hasRefs} is C {@code int *} (an out param: caller allocated array of length 1; a real C array: the array). */
    static int parseLocalAttributes(Xsd10ParserCtxt ctxt, Xsd10Schema schema, XmlNode[] child, Xsd10ItemList[] list, int parentType, int[] hasRefs)
    {
        Object item;

        while ((isSchema(child[0], "attribute") != 0) ||
               (isSchema(child[0], "attributeGroup") != 0))
        {
            if (isSchema(child[0], "attribute") != 0)
            {
                item = parseLocalAttribute(ctxt, schema, child[0], list[0], parentType);
            }
            else
            {
                item = parseAttributeGroupRef(ctxt, schema, child[0]);
                if ((item != null) && (hasRefs != null))
                    hasRefs[0] = 1;
            }
            if (item != null)
            {
                if (list[0] == null)
                {
                    /* TODO: Customize grow factor. */
                    list[0] = itemListCreate();
                    if (list[0] == null)
                        return -1;
                }
                if (itemListAddSize(list[0], 2, item) == -1)
                    return -1;
            }
            child[0] = parseNext(child[0]);
        }
        return 0;
    }

    /** Runs xmlSchemaParseLocalAttributes on the attrUses (void *) field of a type. */
    private static int parseLocalAttributesOfType(Xsd10ParserCtxt ctxt, Xsd10Schema schema, XmlNode[] child,
                                                  Xsd10Type type, int parentType)
    {
        Xsd10ItemList[] l = { as(type.attrUses, Xsd10ItemList.class) };
        int res = parseLocalAttributes(ctxt, schema, child, l, parentType, null);
        type.attrUses = l[0];
        return res;
    }

    // xml_xsd10_parse_annotation
    /** Port of xmlSchemaParseAnnotation. */
    static Xsd10Annot parseAnnotation(Xsd10ParserCtxt ctxt, XmlNode node, int needed)
    {
        Xsd10Annot ret;
        XmlNode child;
        XmlNode attr;
        int barked = 0;

        /*
        * INFO: S4S completed.
        */
        /*
        * id = ID
        * {any attributes with non-schema namespace . . .}>
        * Content: (appinfo | documentation)*
        */
        if ((ctxt == null) || (node == null))
            return null;
        if (needed != 0)
            ret = newAnnot(ctxt, node);
        else
            ret = null;
        attr = parseFirstAttr(node);
        while (attr != null)
        {
            if (((nodeNsUri(attr) == null) &&
                 (strEqual(nodeName(attr), "id") == 0)) ||
                ((nodeNsUri(attr) != null) &&
                 (strEqual(nodeNsUri(attr), XML_XSD10_NS) != 0)))
            {
                pIllegalAttrErr(ctxt,
                    XML_XSD10_SCHEMAP_S4S_ATTR_NOT_ALLOWED, null, attr);
            }
            attr = nodeNext(attr);
        }
        pValAttrId(ctxt, node, "id");
        /*
        * And now for the children...
        */
        child = parseFirstChild(node);
        while (child != null)
        {
            if (isSchema(child, "appinfo") != 0)
            {
                /* TODO: make available the content of "appinfo". */
                /*
                * source = anyURI
                * {any attributes with non-schema namespace . . .}>
                * Content: ({any})*
                */
                attr = parseFirstAttr(child);
                while (attr != null)
                {
                    if (((nodeNsUri(attr) == null) &&
                         (strEqual(nodeName(attr), "source") == 0)) ||
                        ((nodeNsUri(attr) != null) &&
                         (strEqual(nodeNsUri(attr), XML_XSD10_NS) != 0)))
                    {
                        pIllegalAttrErr(ctxt,
                            XML_XSD10_SCHEMAP_S4S_ATTR_NOT_ALLOWED, null, attr);
                    }
                    attr = nodeNext(attr);
                }
                pValAttr(ctxt, null, child, "source",
                    getBuiltInType(XML_XSD10_SCHEMAS_ANYURI), null);
                child = parseNext(child);
            }
            else if (isSchema(child, "documentation") != 0)
            {
                /* TODO: make available the content of "documentation". */
                /*
                * source = anyURI
                * {any attributes with non-schema namespace . . .}>
                * Content: ({any})*
                */
                attr = parseFirstAttr(child);
                while (attr != null)
                {
                    if (nodeNsUri(attr) == null)
                    {
                        if (strEqual(nodeName(attr), "source") == 0)
                        {
                            pIllegalAttrErr(ctxt,
                                XML_XSD10_SCHEMAP_S4S_ATTR_NOT_ALLOWED, null, attr);
                        }
                    }
                    else
                    {
                        if ((strEqual(nodeNsUri(attr), XML_XSD10_NS) != 0) ||
                            ((strEqual(nodeName(attr), "lang") != 0) &&
                             (strEqual(nodeNsUri(attr), XML_XSD10_XML_XML_NAMESPACE) == 0)))
                        {
                            pIllegalAttrErr(ctxt,
                                XML_XSD10_SCHEMAP_S4S_ATTR_NOT_ALLOWED, null, attr);
                        }
                    }
                    attr = nodeNext(attr);
                }
                /*
                * Attribute "xml:lang".
                */
                attr = getPropNodeNs(child, XML_XSD10_XML_XML_NAMESPACE, "lang");
                if (attr != null)
                    pValAttrNode(ctxt, null, attr,
                        getBuiltInType(XML_XSD10_SCHEMAS_LANGUAGE), null);
                child = parseNext(child);
            }
            else
            {
                if (barked == 0)
                    pContentErr(ctxt,
                        XML_XSD10_SCHEMAP_S4S_ELEM_NOT_ALLOWED,
                        null, node, child, null, "(appinfo | documentation)*");
                barked = 1;
                child = parseNext(child);
            }
        }

        return ret;
    }

    // xml_xsd10_parse_facet
    /** Port of xmlSchemaParseFacet. */
    static Xsd10Facet parseFacet(Xsd10ParserCtxt ctxt, Xsd10Schema schema, XmlNode node)
    {
        Xsd10Facet facet;
        XmlNode child = null;
        String value;

        if ((ctxt == null) || (schema == null) || (node == null))
            return null;

        facet = newFacet();
        if (facet == null)
        {
            pErrMemory(ctxt);
            return null;
        }
        facet.node = node;
        value = getProp(ctxt, node, "value");
        if (value == null)
        {
            pErr2(ctxt, node, child, XML_XSD10_SCHEMAP_FACET_NO_VALUE,
                "Facet %s has no value\n", nodeName(node), null);
            freeFacet(facet);
            return null;
        }
        if (isSchema(node, "minInclusive") != 0)
        {
            facet.type = XML_XSD10_SCHEMA_FACET_MININCLUSIVE;
        }
        else if (isSchema(node, "minExclusive") != 0)
        {
            facet.type = XML_XSD10_SCHEMA_FACET_MINEXCLUSIVE;
        }
        else if (isSchema(node, "maxInclusive") != 0)
        {
            facet.type = XML_XSD10_SCHEMA_FACET_MAXINCLUSIVE;
        }
        else if (isSchema(node, "maxExclusive") != 0)
        {
            facet.type = XML_XSD10_SCHEMA_FACET_MAXEXCLUSIVE;
        }
        else if (isSchema(node, "totalDigits") != 0)
        {
            facet.type = XML_XSD10_SCHEMA_FACET_TOTALDIGITS;
        }
        else if (isSchema(node, "fractionDigits") != 0)
        {
            facet.type = XML_XSD10_SCHEMA_FACET_FRACTIONDIGITS;
        }
        else if (isSchema(node, "pattern") != 0)
        {
            facet.type = XML_XSD10_SCHEMA_FACET_PATTERN;
        }
        else if (isSchema(node, "enumeration") != 0)
        {
            facet.type = XML_XSD10_SCHEMA_FACET_ENUMERATION;
        }
        else if (isSchema(node, "whiteSpace") != 0)
        {
            facet.type = XML_XSD10_SCHEMA_FACET_WHITESPACE;
        }
        else if (isSchema(node, "length") != 0)
        {
            facet.type = XML_XSD10_SCHEMA_FACET_LENGTH;
        }
        else if (isSchema(node, "maxLength") != 0)
        {
            facet.type = XML_XSD10_SCHEMA_FACET_MAXLENGTH;
        }
        else if (isSchema(node, "minLength") != 0)
        {
            facet.type = XML_XSD10_SCHEMA_FACET_MINLENGTH;
        }
        else
        {
            pErr2(ctxt, node, child, XML_XSD10_SCHEMAP_UNKNOWN_FACET_TYPE,
                "Unknown facet type %s\n", nodeName(node), null);
            freeFacet(facet);
            return null;
        }
        pValAttrId(ctxt, node, "id");
        facet.value = value;
        if ((facet.type != XML_XSD10_SCHEMA_FACET_PATTERN) &&
            (facet.type != XML_XSD10_SCHEMA_FACET_ENUMERATION))
        {
            String fixed;

            fixed = getProp(ctxt, node, "fixed");
            if (fixed != null)
            {
                if (strEqual(fixed, "true") != 0)
                    facet.fixed = 1;
            }
        }
        child = parseFirstChild(node);

        if (isSchema(child, "annotation") != 0)
        {
            facet.annot = parseAnnotation(ctxt, child, 1);
            child = parseNext(child);
        }
        if (child != null)
        {
            pErr2(ctxt, node, child, XML_XSD10_SCHEMAP_UNKNOWN_FACET_CHILD,
                "Facet %s has unexpected child content\n",
                nodeName(node), null);
        }
        return facet;
    }

    // xml_xsd10_parse_wildcard_ns
    /** Port of xmlSchemaParseWildcardNs. */
    static int parseWildcardNs(Xsd10ParserCtxt ctxt, Xsd10Schema schema, Xsd10Wildcard wildc, XmlNode node)
    {
        String pc, ns, dictnsItem;
        int ret = 0;
        String nsItem;
        Xsd10WildcardNs tmp, lastNs = null;
        XmlNode attr;

        pc = getProp(ctxt, node, "processContents");
        if ((pc == null)
            || (strEqual(pc, "strict") != 0))
        {
            wildc.processContents = XML_XSD10_SCHEMAS_ANY_STRICT;
        }
        else if (strEqual(pc, "skip") != 0)
        {
            wildc.processContents = XML_XSD10_SCHEMAS_ANY_SKIP;
        }
        else if (strEqual(pc, "lax") != 0)
        {
            wildc.processContents = XML_XSD10_SCHEMAS_ANY_LAX;
        }
        else
        {
            pSimpleTypeErr(ctxt,
                XML_XSD10_SCHEMAP_S4S_ATTR_INVALID_VALUE,
                null, node,
                null, "(strict | skip | lax)", pc,
                null, null, null);
            wildc.processContents = XML_XSD10_SCHEMAS_ANY_STRICT;
            ret = XML_XSD10_SCHEMAP_S4S_ATTR_INVALID_VALUE;
        }
        /*
         * Build the namespace constraints.
         */
        attr = getPropNode(node, "namespace");
        ns = getNodeContent(ctxt, attr);
        if (ns == null)
            return -1;
        if ((attr == null) || (strEqual(ns, "##any") != 0))
            wildc.any = 1;
        else if (strEqual(ns, "##other") != 0)
        {
            wildc.negNsSet = newWildcardNsConstraint(ctxt);
            if (wildc.negNsSet == null)
            {
                return -1;
            }
            wildc.negNsSet.value = ctxt.targetNamespace;
        }
        else
        {
            int end, cur;

            cur = 0;
            do
            {
                while (isBlankCh(ch(ns, cur)))
                    cur++;
                end = cur;
                while ((ch(ns, end) != 0) && (!isBlankCh(ch(ns, end))))
                    end++;
                if (end == cur)
                    break;
                nsItem = ns.substring(cur, end);
                if ((strEqual(nsItem, "##other") != 0) ||
                    (strEqual(nsItem, "##any") != 0))
                {
                    pSimpleTypeErr(ctxt,
                        XML_XSD10_SCHEMAP_WILDCARD_INVALID_NS_MEMBER,
                        null, attr,
                        null,
                        "((##any | ##other) | List of (xs:anyURI | " +
                        "(##targetNamespace | ##local)))",
                        nsItem, null, null, null);
                    ret = XML_XSD10_SCHEMAP_WILDCARD_INVALID_NS_MEMBER;
                }
                else
                {
                    if (strEqual(nsItem, "##targetNamespace") != 0)
                    {
                        dictnsItem = ctxt.targetNamespace;
                    }
                    else if (strEqual(nsItem, "##local") != 0)
                    {
                        dictnsItem = null;
                    }
                    else
                    {
                        /*
                        * Validate the item (anyURI).
                        */
                        pValAttrNodeValue(ctxt, null, attr,
                            nsItem, getBuiltInType(XML_XSD10_SCHEMAS_ANYURI));
                        dictnsItem = dictLookup(ctxt.dict, nsItem, -1);
                    }
                    /*
                    * Avoid duplicate namespaces.
                    */
                    tmp = wildc.nsSet;
                    while (tmp != null)
                    {
                        if (Objects.equals(dictnsItem, tmp.value))
                            break;
                        tmp = tmp.next;
                    }
                    if (tmp == null)
                    {
                        tmp = newWildcardNsConstraint(ctxt);
                        if (tmp == null)
                        {
                            return -1;
                        }
                        tmp.value = dictnsItem;
                        tmp.next = null;
                        if (wildc.nsSet == null)
                            wildc.nsSet = tmp;
                        else if (lastNs != null)
                            lastNs.next = tmp;
                        lastNs = tmp;
                    }

                }
                cur = end;
            } while (ch(ns, cur) != 0);
        }
        return ret;
    }

    // xml_xsd10_p_check_particle_correct_2
    /** Port of xmlSchemaPCheckParticleCorrect_2. */
    static int pCheckParticleCorrect2(Xsd10ParserCtxt ctxt, Xsd10Particle item, XmlNode node, int minOccurs, int maxOccurs)
    {
        if ((maxOccurs == 0) && (minOccurs == 0))
            return 0;
        if (maxOccurs != XML_XSD10_UNBOUNDED)
        {
            /*
            * TODO: Maybe we should better not create the particle,
            * if min/max is invalid, since it could confuse the build of the
            * content model.
            */
            /*
            * 3.9.6 Schema Component Constraint: Particle Correct
            *
            */
            if (maxOccurs < 1)
            {
                /*
                * 2.2 {max occurs} must be greater than or equal to 1.
                */
                pCustomAttrErr(ctxt,
                    XML_XSD10_SCHEMAP_P_PROPS_CORRECT_2_2,
                    null, null,
                    getPropNode(node, "maxOccurs"),
                    "The value must be greater than or equal to 1");
                return XML_XSD10_SCHEMAP_P_PROPS_CORRECT_2_2;
            }
            else if (minOccurs > maxOccurs)
            {
                /*
                * 2.1 {min occurs} must not be greater than {max occurs}.
                */
                pCustomAttrErr(ctxt,
                    XML_XSD10_SCHEMAP_P_PROPS_CORRECT_2_1,
                    null, null,
                    getPropNode(node, "minOccurs"),
                    "The value must not be greater than the value of 'maxOccurs'");
                return XML_XSD10_SCHEMAP_P_PROPS_CORRECT_2_1;
            }
        }
        return 0;
    }

    // xml_xsd10_parse_any
    /** Port of xmlSchemaParseAny. */
    static Xsd10Particle parseAny(Xsd10ParserCtxt ctxt, Xsd10Schema schema, XmlNode node)
    {
        Xsd10Particle particle;
        XmlNode child;
        Xsd10Wildcard wild;
        int min, max;
        XmlNode attr;
        Xsd10Annot annot = null;

        if ((ctxt == null) || (schema == null) || (node == null))
            return null;
        /*
        * Check for illegal attributes.
        */
        attr = parseFirstAttr(node);
        while (attr != null)
        {
            if (nodeNsUri(attr) == null)
            {
                if ((strEqual(nodeName(attr), "id") == 0) &&
                    (strEqual(nodeName(attr), "minOccurs") == 0) &&
                    (strEqual(nodeName(attr), "maxOccurs") == 0) &&
                    (strEqual(nodeName(attr), "namespace") == 0) &&
                    (strEqual(nodeName(attr), "processContents") == 0))
                {
                    pIllegalAttrErr(ctxt,
                        XML_XSD10_SCHEMAP_S4S_ATTR_NOT_ALLOWED, null, attr);
                }
            }
            else if (strEqual(nodeNsUri(attr), XML_XSD10_NS) != 0)
            {
                pIllegalAttrErr(ctxt,
                    XML_XSD10_SCHEMAP_S4S_ATTR_NOT_ALLOWED, null, attr);
            }
            attr = nodeNext(attr);
        }
        pValAttrId(ctxt, node, "id");
        /*
        * minOccurs/maxOccurs.
        */
        max = getMaxOccurs(ctxt, node, 0, XML_XSD10_UNBOUNDED, 1,
            "(xs:nonNegativeInteger | unbounded)");
        min = getMinOccurs(ctxt, node, 0, -1, 1,
            "xs:nonNegativeInteger");
        pCheckParticleCorrect2(ctxt, null, node, min, max);
        /*
        * Create & parse the wildcard.
        */
        wild = addWildcard(ctxt, schema, XML_XSD10_SCHEMA_TYPE_ANY, node);
        if (wild == null)
            return null;
        parseWildcardNs(ctxt, schema, wild, node);
        /*
        * And now for the children...
        */
        child = parseFirstChild(node);
        if (isSchema(child, "annotation") != 0)
        {
            annot = parseAnnotation(ctxt, child, 1);
            child = parseNext(child);
        }
        if (child != null)
        {
            pContentErr(ctxt,
                XML_XSD10_SCHEMAP_S4S_ELEM_NOT_ALLOWED,
                null, node, child,
                null, "(annotation?)");
        }
        /*
        * No component if minOccurs==maxOccurs==0.
        */
        if ((min == 0) && (max == 0))
        {
            /* Don't free the wildcard, since it's already on the list. */
            return null;
        }
        /*
        * Create the particle.
        */
        particle = addParticle(ctxt, node, min, max);
        if (particle == null)
            return null;
        particle.annot = annot;
        particle.children = wild;

        return particle;
    }

    // xml_xsd10_parse_notation
    /** Port of xmlSchemaParseNotation. */
    static Xsd10Notation parseNotation(Xsd10ParserCtxt ctxt, Xsd10Schema schema, XmlNode node)
    {
        String name;
        Xsd10Notation ret;
        XmlNode child = null;

        if ((ctxt == null) || (schema == null) || (node == null))
            return null;
        name = getProp(ctxt, node, "name");
        if (name == null)
        {
            pErr2(ctxt, node, child, XML_XSD10_SCHEMAP_NOTATION_NO_NAME,
                "Notation has no name\n", null, null);
            return null;
        }
        ret = addNotation(ctxt, schema, name, ctxt.targetNamespace, node);
        if (ret == null)
            return null;
        pValAttrId(ctxt, node, "id");

        child = parseFirstChild(node);
        if (isSchema(child, "annotation") != 0)
        {
            ret.annot = parseAnnotation(ctxt, child, 1);
            child = parseNext(child);
        }
        if (child != null)
        {
            pContentErr(ctxt,
                XML_XSD10_SCHEMAP_S4S_ELEM_NOT_ALLOWED,
                null, node, child,
                null, "(annotation?)");
        }

        return ret;
    }

    // xml_xsd10_parse_any_attribute
    /** Port of xmlSchemaParseAnyAttribute. */
    static Xsd10Wildcard parseAnyAttribute(Xsd10ParserCtxt ctxt, Xsd10Schema schema, XmlNode node)
    {
        Xsd10Wildcard ret;
        XmlNode child;
        XmlNode attr;

        if ((ctxt == null) || (schema == null) || (node == null))
            return null;

        ret = addWildcard(ctxt, schema, XML_XSD10_SCHEMA_TYPE_ANY_ATTRIBUTE, node);
        if (ret == null)
        {
            return null;
        }
        /*
        * Check for illegal attributes.
        */
        attr = parseFirstAttr(node);
        while (attr != null)
        {
            if (nodeNsUri(attr) == null)
            {
                if ((strEqual(nodeName(attr), "id") == 0) &&
                    (strEqual(nodeName(attr), "namespace") == 0) &&
                    (strEqual(nodeName(attr), "processContents") == 0))
                {
                    pIllegalAttrErr(ctxt,
                        XML_XSD10_SCHEMAP_S4S_ATTR_NOT_ALLOWED, null, attr);
                }
            }
            else if (strEqual(nodeNsUri(attr), XML_XSD10_NS) != 0)
            {
                pIllegalAttrErr(ctxt,
                    XML_XSD10_SCHEMAP_S4S_ATTR_NOT_ALLOWED, null, attr);
            }
            attr = nodeNext(attr);
        }
        pValAttrId(ctxt, node, "id");
        /*
        * Parse the namespace list.
        */
        if (parseWildcardNs(ctxt, schema, ret, node) != 0)
            return null;
        /*
        * And now for the children...
        */
        child = parseFirstChild(node);
        if (isSchema(child, "annotation") != 0)
        {
            ret.annot = parseAnnotation(ctxt, child, 1);
            child = parseNext(child);
        }
        if (child != null)
        {
            pContentErr(ctxt,
                XML_XSD10_SCHEMAP_S4S_ELEM_NOT_ALLOWED,
                null, node, child,
                null, "(annotation?)");
        }

        return ret;
    }

    // xml_xsd10_parse_local_attribute
    /** Port of xmlSchemaParseLocalAttribute. */
    static Xsd10BasicItem parseLocalAttribute(Xsd10ParserCtxt pctxt, Xsd10Schema schema, XmlNode node, Xsd10ItemList uses, int parentType)
    {
        String attrValue;
        String[] name = { null };
        String ns = null;
        Xsd10AttributeUse use = null;
        XmlNode child;
        XmlNode attr;
        String[] tmpNs = { null }, tmpName = { null };
        String defValue = null;
        int isRef = 0, occurs = XML_XSD10_SCHEMAS_ATTR_USE_OPTIONAL;
        int nberrors, hasForm = 0, defValueType = 0;

        /*
         * 3.2.3 Constraints on XML Representations of Attribute Declarations
         */

        if ((pctxt == null) || (schema == null) || (node == null))
            return null;
        attr = getPropNode(node, "ref");
        if (attr != null)
        {
            if (pValAttrNodeQName(pctxt, schema, null, attr, tmpNs, tmpName) != 0)
            {
                return null;
            }
            if (checkReference(pctxt, schema, node, attr, tmpNs[0]) != 0)
                return null;
            isRef = 1;
        }
        nberrors = pctxt.nberrors;
        /*
        * Check for illegal attributes.
        */
        attr = parseFirstAttr(node);
        while (attr != null)
        {
            boolean attrNext = false;
            if (nodeNsUri(attr) == null)
            {
                if (isRef != 0)
                {
                    if (strEqual(nodeName(attr), "id") != 0)
                    {
                        pValAttrNodeId(pctxt, attr);
                        attrNext = true;
                    }
                    else if (strEqual(nodeName(attr), "ref") != 0)
                    {
                        attrNext = true;
                    }
                }
                else
                {
                    if (strEqual(nodeName(attr), "name") != 0)
                    {
                        attrNext = true;
                    }
                    else if (strEqual(nodeName(attr), "id") != 0)
                    {
                        pValAttrNodeId(pctxt, attr);
                        attrNext = true;
                    }
                    else if (strEqual(nodeName(attr), "type") != 0)
                    {
                        pValAttrNodeQName(pctxt, schema, null, attr, tmpNs, tmpName);
                        attrNext = true;
                    }
                    else if (strEqual(nodeName(attr), "form") != 0)
                    {
                        /*
                        * Evaluate the target namespace
                        */
                        hasForm = 1;
                        attrValue = getNodeContent(pctxt, attr);
                        if (strEqual(attrValue, "qualified") != 0)
                        {
                            ns = pctxt.targetNamespace;
                        }
                        else if (strEqual(attrValue, "unqualified") == 0)
                        {
                            pSimpleTypeErr(pctxt,
                                XML_XSD10_SCHEMAP_S4S_ATTR_INVALID_VALUE,
                                null, attr,
                                null, "(qualified | unqualified)",
                                attrValue, null, null, null);
                        }
                        attrNext = true;
                    }
                }
                if (!attrNext)
                {
                    if (strEqual(nodeName(attr), "use") != 0)
                    {
                        attrValue = getNodeContent(pctxt, attr);
                        /* TODO: Maybe we need to normalize the value beforehand. */
                        if (strEqual(attrValue, "optional") != 0)
                            occurs = XML_XSD10_SCHEMAS_ATTR_USE_OPTIONAL;
                        else if (strEqual(attrValue, "prohibited") != 0)
                            occurs = XML_XSD10_SCHEMAS_ATTR_USE_PROHIBITED;
                        else if (strEqual(attrValue, "required") != 0)
                            occurs = XML_XSD10_SCHEMAS_ATTR_USE_REQUIRED;
                        else
                        {
                            pSimpleTypeErr(pctxt,
                                XML_XSD10_SCHEMAP_INVALID_ATTR_USE,
                                null, attr,
                                null, "(optional | prohibited | required)",
                                attrValue, null, null, null);
                        }
                        attrNext = true;
                    }
                    else if (strEqual(nodeName(attr), "default") != 0)
                    {
                        /*
                        * 3.2.3 : 1
                        * default and fixed must not both be present.
                        */
                        if (defValue != null)
                        {
                            pMutualExclAttrErr(pctxt,
                                XML_XSD10_SCHEMAP_SRC_ATTRIBUTE_1,
                                null, attr, "default", "fixed");
                        }
                        else
                        {
                            defValue = getNodeContent(pctxt, attr);
                            defValueType = XSD10_WXS_ATTR_DEF_VAL_DEFAULT;
                        }
                        attrNext = true;
                    }
                    else if (strEqual(nodeName(attr), "fixed") != 0)
                    {
                        /*
                        * 3.2.3 : 1
                        * default and fixed must not both be present.
                        */
                        if (defValue != null)
                        {
                            pMutualExclAttrErr(pctxt,
                                XML_XSD10_SCHEMAP_SRC_ATTRIBUTE_1,
                                null, attr, "default", "fixed");
                        }
                        else
                        {
                            defValue = getNodeContent(pctxt, attr);
                            defValueType = XSD10_WXS_ATTR_DEF_VAL_FIXED;
                        }
                        attrNext = true;
                    }
                }
            }
            else if (strEqual(nodeNsUri(attr), XML_XSD10_NS) == 0)
                attrNext = true;

            if (!attrNext)
                pIllegalAttrErr(pctxt,
                    XML_XSD10_SCHEMAP_S4S_ATTR_NOT_ALLOWED, null, attr);

            /* attr_next: */
            attr = nodeNext(attr);
        }
        /*
        * 3.2.3 : 2
        * If default and use are both present, use must have
        * the actual value optional.
        */
        if ((defValueType == XSD10_WXS_ATTR_DEF_VAL_DEFAULT) &&
            (occurs != XML_XSD10_SCHEMAS_ATTR_USE_OPTIONAL))
        {
            pSimpleTypeErr(pctxt,
                XML_XSD10_SCHEMAP_SRC_ATTRIBUTE_2,
                null, node, null,
                "(optional | prohibited | required)", null,
                "The value of the attribute 'use' must be 'optional' " +
                "if the attribute 'default' is present",
                null, null);
        }
        /*
        * We want correct attributes.
        */
        if (nberrors != pctxt.nberrors)
            return null;
        if (isRef == 0)
        {
            Xsd10Attribute attrDecl;

            /* TODO: move XML_SCHEMAS_QUALIF_ATTR to the parser. */
            if ((hasForm == 0) && ((schema.flags & XML_XSD10_SCHEMAS_QUALIF_ATTR) != 0))
                ns = pctxt.targetNamespace;
            /*
            * 3.2.6 Schema Component Constraint: xsi: Not Allowed
            * TODO: Move this to the component layer.
            */
            if (strEqual(ns, XML_XSD10_INSTANCE_NS) != 0)
            {
                customErr(pctxt,
                    XML_XSD10_SCHEMAP_NO_XSI,
                    node, null,
                    "The target namespace must not match '%s'",
                    XML_XSD10_INSTANCE_NS, null);
            }
            attr = getPropNode(node, "name");
            if (attr == null)
            {
                pMissingAttrErr(pctxt, XML_XSD10_SCHEMAP_S4S_ATTR_MISSING,
                    null, node, "name", null);
                return null;
            }
            if (pValAttrNode(pctxt, null, attr,
                getBuiltInType(XML_XSD10_SCHEMAS_NCNAME), name) != 0)
            {
                return null;
            }
            /*
            * 3.2.6 Schema Component Constraint: xmlns Not Allowed
            * TODO: Move this to the component layer.
            */
            if (strEqual(name[0], "xmlns") != 0)
            {
                pSimpleTypeErr(pctxt,
                    XML_XSD10_SCHEMAP_NO_XMLNS,
                    null, attr,
                    getBuiltInType(XML_XSD10_SCHEMAS_NCNAME), null, null,
                    "The value of the attribute must not match 'xmlns'",
                    null, null);
                return null;
            }
            if (occurs != XML_XSD10_SCHEMAS_ATTR_USE_PROHIBITED)
            {
                /*
                * Create the attribute use component.
                */
                use = addAttributeUse(pctxt, node);
                if (use == null)
                    return null;
                use.occurs = occurs;
                /*
                * Create the attribute declaration.
                */
                attrDecl = addAttribute(pctxt, schema, name[0], ns, node, 0);
                if (attrDecl == null)
                    return null;
                if (tmpName[0] != null)
                {
                    attrDecl.typeName = tmpName[0];
                    attrDecl.typeNs = tmpNs[0];
                }
                use.attrDecl = attrDecl;
                /*
                * Value constraint.
                */
                if (defValue != null)
                {
                    attrDecl.defValue = defValue;
                    if (defValueType == XSD10_WXS_ATTR_DEF_VAL_FIXED)
                        attrDecl.flags |= XML_XSD10_SCHEMAS_ATTR_FIXED;
                }
            }
        }
        else if (occurs != XML_XSD10_SCHEMAS_ATTR_USE_PROHIBITED)
        {
            Xsd10QNameRef ref;

            /*
            * Create the attribute use component.
            */
            use = addAttributeUse(pctxt, node);
            if (use == null)
                return null;
            /*
            * We need to resolve the reference at later stage.
            */
            wxsAddPending(pctxt, use);
            use.occurs = occurs;
            /*
            * Create a QName reference to the attribute declaration.
            */
            ref = newQNameRef(pctxt, XML_XSD10_SCHEMA_TYPE_ATTRIBUTE,
                tmpName[0], tmpNs[0]);
            if (ref == null)
                return null;
            /*
            * Assign the reference. This will be substituted for the
            * referenced attribute declaration when the QName is resolved.
            */
            use.attrDecl = ref;
            /*
            * Value constraint.
            */
            if (defValue != null)
                use.defValue = defValue;
            if (defValueType == XSD10_WXS_ATTR_DEF_VAL_FIXED)
                use.flags |= XML_XSD10_SCHEMA_ATTR_USE_FIXED;
        }

        /* check_children: */
        /*
        * And now for the children...
        */
        child = parseFirstChild(node);
        if (occurs == XML_XSD10_SCHEMAS_ATTR_USE_PROHIBITED)
        {
            Xsd10AttributeUseProhib prohib;

            if (isSchema(child, "annotation") != 0)
            {
                parseAnnotation(pctxt, child, 0);
                child = parseNext(child);
            }
            if (child != null)
            {
                pContentErr(pctxt,
                    XML_XSD10_SCHEMAP_S4S_ELEM_NOT_ALLOWED,
                    null, node, child, null,
                    "(annotation?)");
            }
            /*
            * Check for pointlessness of attribute prohibitions.
            */
            if (parentType == XML_XSD10_SCHEMA_TYPE_ATTRIBUTEGROUP)
            {
                customWarning(pctxt,
                    XML_XSD10_SCHEMAP_WARN_ATTR_POINTLESS_PROH,
                    node, null,
                    "Skipping attribute use prohibition, since it is " +
                    "pointless inside an <attributeGroup>",
                    null, null, null);
                return null;
            }
            else if (parentType == XML_XSD10_SCHEMA_TYPE_EXTENSION)
            {
                customWarning(pctxt,
                    XML_XSD10_SCHEMAP_WARN_ATTR_POINTLESS_PROH,
                    node, null,
                    "Skipping attribute use prohibition, since it is " +
                    "pointless when extending a type",
                    null, null, null);
                return null;
            }
            if (isRef == 0)
            {
                tmpName[0] = name[0];
                tmpNs[0] = ns;
            }
            /*
            * Check for duplicate attribute prohibitions.
            */
            if (uses != null)
            {
                int i;

                for (i = 0; i < uses.nbItems; i++)
                {
                    Xsd10BasicItem u = as(uses.items[i], Xsd10BasicItem.class);
                    if ((u != null) && (u.type == XML_XSD10_SCHEMA_EXTRA_ATTR_USE_PROHIB) &&
                        (u instanceof Xsd10AttributeUseProhib p) &&
                        Objects.equals(tmpName[0], p.name) &&
                        Objects.equals(tmpNs[0], p.targetNamespace))
                    {
                        String[] str = { null };

                        customWarning(pctxt,
                            XML_XSD10_SCHEMAP_WARN_ATTR_POINTLESS_PROH,
                            node, null,
                            "Skipping duplicate attribute use prohibition '%s'",
                            formatQName(str, tmpNs[0], tmpName[0]),
                            null, null);
                        return null;
                    }
                }
            }
            /*
            * Create the attribute prohibition helper component.
            */
            prohib = addAttributeUseProhib(pctxt);
            if (prohib == null)
                return null;
            prohib.node = node;
            prohib.name = tmpName[0];
            prohib.targetNamespace = tmpNs[0];
            if (isRef != 0)
            {
                /*
                * We need at least to resolve to the attribute declaration.
                */
                wxsAddPending(pctxt, prohib);
            }
            return prohib;
        }
        else
        {
            if (isSchema(child, "annotation") != 0)
            {
                /*
                * TODO: Should this go into the attr decl?
                */
                use.annot = parseAnnotation(pctxt, child, 1);
                child = parseNext(child);
            }
            if (isRef != 0)
            {
                if (child != null)
                {
                    if (isSchema(child, "simpleType") != 0)
                        /*
                        * 3.2.3 : 3.2
                        * If ref is present, then all of <simpleType>,
                        * form and type must be absent.
                        */
                        pContentErr(pctxt,
                            XML_XSD10_SCHEMAP_SRC_ATTRIBUTE_3_2,
                            null, node, child, null,
                            "(annotation?)");
                    else
                        pContentErr(pctxt,
                            XML_XSD10_SCHEMAP_S4S_ELEM_NOT_ALLOWED,
                            null, node, child, null,
                            "(annotation?)");
                }
            }
            else
            {
                if (isSchema(child, "simpleType") != 0)
                {
                    Xsd10Attribute decl = wxsAttruseDecl(use);
                    if ((decl != null) && (decl.typeName != null))
                    {
                        /*
                        * 3.2.3 : 4
                        * type and <simpleType> must not both be present.
                        */
                        pContentErr(pctxt, XML_XSD10_SCHEMAP_SRC_ATTRIBUTE_4,
                            null, node, child,
                            "The attribute 'type' and the <simpleType> child " +
                            "are mutually exclusive", null);
                    }
                    else
                    {
                        Xsd10Type st = parseSimpleType(pctxt, schema, child, 0);
                        if (decl != null)
                            decl.subtypes = st;
                    }
                    child = parseNext(child);
                }
                if (child != null)
                    pContentErr(pctxt, XML_XSD10_SCHEMAP_S4S_ELEM_NOT_ALLOWED,
                        null, node, child, null,
                        "(annotation?, simpleType?)");
            }
        }
        return use;
    }

    // xml_xsd10_parse_global_attribute
    /** Port of xmlSchemaParseGlobalAttribute. */
    static Xsd10Attribute parseGlobalAttribute(Xsd10ParserCtxt pctxt, Xsd10Schema schema, XmlNode node)
    {
        String[] attrValue = { null };
        Xsd10Attribute ret;
        XmlNode child;
        XmlNode attr;

        /*
         * Note that the w3c spec assumes the schema to be validated with schema
         * for schemas beforehand.
         *
         * 3.2.3 Constraints on XML Representations of Attribute Declarations
         */
        if ((pctxt == null) || (schema == null) || (node == null))
            return null;
        /*
        * 3.2.3 : 3.1
        * One of ref or name must be present, but not both
        */
        attr = getPropNode(node, "name");
        if (attr == null)
        {
            pMissingAttrErr(pctxt, XML_XSD10_SCHEMAP_S4S_ATTR_MISSING,
                null, node, "name", null);
            return null;
        }
        if (pValAttrNode(pctxt, null, attr,
            getBuiltInType(XML_XSD10_SCHEMAS_NCNAME), attrValue) != 0)
        {
            return null;
        }
        /*
        * 3.2.6 Schema Component Constraint: xmlns Not Allowed
        * TODO: Move this to the component layer.
        */
        if (strEqual(attrValue[0], "xmlns") != 0)
        {
            pSimpleTypeErr(pctxt,
                XML_XSD10_SCHEMAP_NO_XMLNS,
                null, attr,
                getBuiltInType(XML_XSD10_SCHEMAS_NCNAME), null, null,
                "The value of the attribute must not match 'xmlns'",
                null, null);
            return null;
        }
        /*
        * 3.2.6 Schema Component Constraint: xsi: Not Allowed
        * TODO: Move this to the component layer.
        *       Or better leave it here and add it to the component layer
        *       if we have a schema construction API.
        */
        if (strEqual(pctxt.targetNamespace, XML_XSD10_INSTANCE_NS) != 0)
        {
            customErr(pctxt,
                XML_XSD10_SCHEMAP_NO_XSI, node, null,
                "The target namespace must not match '%s'",
                XML_XSD10_INSTANCE_NS, null);
        }

        ret = addAttribute(pctxt, schema, attrValue[0],
            pctxt.targetNamespace, node, 1);
        if (ret == null)
            return null;
        ret.flags |= XML_XSD10_SCHEMAS_ATTR_GLOBAL;

        /*
        * Check for illegal attributes.
        */
        attr = parseFirstAttr(node);
        while (attr != null)
        {
            if (nodeNsUri(attr) == null)
            {
                if ((strEqual(nodeName(attr), "id") == 0) &&
                    (strEqual(nodeName(attr), "default") == 0) &&
                    (strEqual(nodeName(attr), "fixed") == 0) &&
                    (strEqual(nodeName(attr), "name") == 0) &&
                    (strEqual(nodeName(attr), "type") == 0))
                {
                    pIllegalAttrErr(pctxt,
                        XML_XSD10_SCHEMAP_S4S_ATTR_NOT_ALLOWED, null, attr);
                }
            }
            else if (strEqual(nodeNsUri(attr), XML_XSD10_NS) != 0)
            {
                pIllegalAttrErr(pctxt,
                    XML_XSD10_SCHEMAP_S4S_ATTR_NOT_ALLOWED, null, attr);
            }
            attr = nodeNext(attr);
        }
        {
            String[] u = { null }, l = { null };
            pValAttrQName(pctxt, schema, null, node, "type", u, l);
            ret.typeNs = u[0];
            ret.typeName = l[0];
        }

        pValAttrId(pctxt, node, "id");
        /*
        * Attribute "fixed".
        */
        ret.defValue = getProp(pctxt, node, "fixed");
        if (ret.defValue != null)
            ret.flags |= XML_XSD10_SCHEMAS_ATTR_FIXED;
        /*
        * Attribute "default".
        */
        attr = getPropNode(node, "default");
        if (attr != null)
        {
            /*
            * 3.2.3 : 1
            * default and fixed must not both be present.
            */
            if ((ret.flags & XML_XSD10_SCHEMAS_ATTR_FIXED) != 0)
            {
                pMutualExclAttrErr(pctxt, XML_XSD10_SCHEMAP_SRC_ATTRIBUTE_1,
                    ret, attr, "default", "fixed");
            }
            else
                ret.defValue = getNodeContent(pctxt, attr);
        }
        /*
        * And now for the children...
        */
        child = parseFirstChild(node);
        if (isSchema(child, "annotation") != 0)
        {
            ret.annot = parseAnnotation(pctxt, child, 1);
            child = parseNext(child);
        }
        if (isSchema(child, "simpleType") != 0)
        {
            if (ret.typeName != null)
            {
                /*
                * 3.2.3 : 4
                * type and <simpleType> must not both be present.
                */
                pContentErr(pctxt, XML_XSD10_SCHEMAP_SRC_ATTRIBUTE_4,
                    null, node, child,
                    "The attribute 'type' and the <simpleType> child " +
                    "are mutually exclusive", null);
            }
            else
                ret.subtypes = parseSimpleType(pctxt, schema, child, 0);
            child = parseNext(child);
        }
        if (child != null)
            pContentErr(pctxt, XML_XSD10_SCHEMAP_S4S_ELEM_NOT_ALLOWED,
                null, node, child, null,
                "(annotation?, simpleType?)");

        return ret;
    }

    // xml_xsd10_parse_attribute_group_ref
    /** Port of xmlSchemaParseAttributeGroupRef. */
    static Xsd10QNameRef parseAttributeGroupRef(Xsd10ParserCtxt pctxt, Xsd10Schema schema, XmlNode node)
    {
        Xsd10QNameRef ret;
        XmlNode child;
        XmlNode attr;
        String[] refNs = { null }, ref = { null };

        if ((pctxt == null) || (schema == null) || (node == null))
            return null;

        attr = getPropNode(node, "ref");
        if (attr == null)
        {
            pMissingAttrErr(pctxt,
                XML_XSD10_SCHEMAP_S4S_ATTR_MISSING,
                null, node, "ref", null);
            return null;
        }
        pValAttrNodeQName(pctxt, schema, null, attr, refNs, ref);
        if (checkReference(pctxt, schema, node, attr, refNs[0]) != 0)
            return null;

        /*
        * Check for illegal attributes.
        */
        attr = parseFirstAttr(node);
        while (attr != null)
        {
            if (nodeNsUri(attr) == null)
            {
                if ((strEqual(nodeName(attr), "ref") == 0) &&
                    (strEqual(nodeName(attr), "id") == 0))
                {
                    pIllegalAttrErr(pctxt,
                        XML_XSD10_SCHEMAP_S4S_ATTR_NOT_ALLOWED, null, attr);
                }
            }
            else if (strEqual(nodeNsUri(attr), XML_XSD10_NS) != 0)
            {
                pIllegalAttrErr(pctxt,
                    XML_XSD10_SCHEMAP_S4S_ATTR_NOT_ALLOWED, null, attr);
            }
            attr = nodeNext(attr);
        }
        /* Attribute ID */
        pValAttrId(pctxt, node, "id");

        /*
        * And now for the children...
        */
        child = parseFirstChild(node);
        if (isSchema(child, "annotation") != 0)
        {
            /*
            * TODO: We do not have a place to store the annotation, do we?
            */
            parseAnnotation(pctxt, child, 0);
            child = parseNext(child);
        }
        if (child != null)
        {
            pContentErr(pctxt,
                XML_XSD10_SCHEMAP_S4S_ELEM_NOT_ALLOWED,
                null, node, child, null,
                "(annotation?)");
        }

        /*
        * Handle attribute group redefinitions.
        */
        if ((pctxt.isRedefine != 0) && (pctxt.redef != null) &&
            (pctxt.redef.item != null) &&
            (pctxt.redef.item.type == XML_XSD10_SCHEMA_TYPE_ATTRIBUTEGROUP) &&
            Objects.equals(ref[0], pctxt.redef.refName) &&
            Objects.equals(refNs[0], pctxt.redef.refTargetNs))
        {
            /*
            * SPEC src-redefine:
            * (7.1) "If it has an <attributeGroup> among its contents
            * the `actual value` of whose ref [attribute] is the same
            * as the `actual value` of its own name attribute plus
            * target namespace, then it must have exactly one such group."
            */
            if (pctxt.redefCounter != 0)
            {
                String[] str = { null };

                customErr(pctxt,
                    XML_XSD10_SCHEMAP_SRC_REDEFINE, node, null,
                    "The redefining attribute group definition " +
                    "'%s' must not contain more than one " +
                    "reference to the redefined definition",
                    formatQName(str, refNs[0], ref[0]), null);
                return null;
            }
            pctxt.redefCounter++;
            /*
            * URGENT TODO: How to ensure that the reference will not be
            * handled by the normal component resolution mechanism?
            */
            ret = newQNameRef(pctxt,
                XML_XSD10_SCHEMA_TYPE_ATTRIBUTEGROUP, ref[0], refNs[0]);
            if (ret == null)
                return null;
            ret.node = node;
            pctxt.redef.reference = ret;
        }
        else
        {
            /*
            * Create a QName-reference helper component. We will substitute this
            * component for the attribute uses of the referenced attribute group
            * definition.
            */
            ret = newQNameRef(pctxt,
                XML_XSD10_SCHEMA_TYPE_ATTRIBUTEGROUP, ref[0], refNs[0]);
            if (ret == null)
                return null;
            ret.node = node;
            /* Add to pending items, to be able to resolve the reference. */
            wxsAddPending(pctxt, ret);
        }
        return ret;
    }

    // xml_xsd10_parse_attribute_group_definition
    /** Port of xmlSchemaParseAttributeGroupDefinition. */
    static Xsd10AttributeGroup parseAttributeGroupDefinition(Xsd10ParserCtxt pctxt, Xsd10Schema schema, XmlNode node)
    {
        String[] name = { null };
        Xsd10AttributeGroup ret;
        XmlNode child;
        XmlNode attr;
        int[] hasRefs = { 0 };
        int res;

        if ((pctxt == null) || (schema == null) || (node == null))
            return null;

        attr = getPropNode(node, "name");
        if (attr == null)
        {
            pMissingAttrErr(pctxt,
                XML_XSD10_SCHEMAP_S4S_ATTR_MISSING,
                null, node, "name", null);
            return null;
        }
        /*
        * The name is crucial, exit if invalid.
        */
        if (pValAttrNode(pctxt,
            null, attr,
            getBuiltInType(XML_XSD10_SCHEMAS_NCNAME), name) != 0)
        {
            return null;
        }
        ret = addAttributeGroupDefinition(pctxt, schema,
            name[0], pctxt.targetNamespace, node);
        if (ret == null)
            return null;
        /*
        * Check for illegal attributes.
        */
        attr = parseFirstAttr(node);
        while (attr != null)
        {
            if (nodeNsUri(attr) == null)
            {
                if ((strEqual(nodeName(attr), "name") == 0) &&
                    (strEqual(nodeName(attr), "id") == 0))
                {
                    pIllegalAttrErr(pctxt,
                        XML_XSD10_SCHEMAP_S4S_ATTR_NOT_ALLOWED, null, attr);
                }
            }
            else if (strEqual(nodeNsUri(attr), XML_XSD10_NS) != 0)
            {
                pIllegalAttrErr(pctxt,
                    XML_XSD10_SCHEMAP_S4S_ATTR_NOT_ALLOWED, null, attr);
            }
            attr = nodeNext(attr);
        }
        /* Attribute ID */
        pValAttrId(pctxt, node, "id");
        /*
        * And now for the children...
        */
        child = parseFirstChild(node);
        if (isSchema(child, "annotation") != 0)
        {
            ret.annot = parseAnnotation(pctxt, child, 1);
            child = parseNext(child);
        }
        /*
        * Parse contained attribute decls/refs.
        */
        {
            XmlNode[] c = { child };
            Xsd10ItemList[] l = { as(ret.attrUses, Xsd10ItemList.class) };
            res = parseLocalAttributes(pctxt, schema, c, l,
                XML_XSD10_SCHEMA_TYPE_ATTRIBUTEGROUP, hasRefs);
            ret.attrUses = l[0];
            child = c[0];
        }
        if (hasRefs[0] != 0)
            ret.flags |= XML_XSD10_SCHEMAS_ATTRGROUP_HAS_REFS;
        if (res == -1)
            return null;
        /*
        * Parse the attribute wildcard.
        */
        if (isSchema(child, "anyAttribute") != 0)
        {
            ret.attributeWildcard = parseAnyAttribute(pctxt, schema, child);
            child = parseNext(child);
        }
        if (child != null)
        {
            pContentErr(pctxt,
                XML_XSD10_SCHEMAP_S4S_ELEM_NOT_ALLOWED,
                null, node, child, null,
                "(annotation?, ((attribute | attributeGroup)*, anyAttribute?))");
        }
        return ret;
    }

    // xml_xsd10_p_val_attr_form_default
    /** Port of xmlSchemaPValAttrFormDefault. Arrays: {@code flags} is C {@code int *} (an out param: caller allocated array of length 1; a real C array: the array). */
    static int pValAttrFormDefault(String value, int[] flags, int flagQualified)
    {
        if (strEqual(value, "qualified") != 0)
        {
            if ((flags[0] & flagQualified) == 0)
                flags[0] |= flagQualified;
        }
        else if (strEqual(value, "unqualified") == 0)
            return 1;

        return 0;
    }

    // xml_xsd10_p_val_attr_block_final
    /** Port of xmlSchemaPValAttrBlockFinal. Arrays: {@code flags} is C {@code int *} (an out param: caller allocated array of length 1; a real C array: the array). */
    static int pValAttrBlockFinal(String value, int[] flags, int flagAll, int flagExtension, int flagRestriction, int flagSubstitution, int flagList, int flagUnion)
    {
        int ret = 0;

        /*
        * TODO: This does not check for duplicate entries.
        */
        if ((flags == null) || (value == null))
            return -1;
        if (value.isEmpty())
            return 0;
        if (strEqual(value, "#all") != 0)
        {
            if (flagAll != -1)
                flags[0] |= flagAll;
            else
            {
                if (flagExtension != -1)
                    flags[0] |= flagExtension;
                if (flagRestriction != -1)
                    flags[0] |= flagRestriction;
                if (flagSubstitution != -1)
                    flags[0] |= flagSubstitution;
                if (flagList != -1)
                    flags[0] |= flagList;
                if (flagUnion != -1)
                    flags[0] |= flagUnion;
            }
        }
        else
        {
            int end, cur = 0;
            String item;

            do
            {
                while (isBlankCh(ch(value, cur)))
                    cur++;
                end = cur;
                while ((ch(value, end) != 0) && (!isBlankCh(ch(value, end))))
                    end++;
                if (end == cur)
                    break;
                item = value.substring(cur, end);
                if (strEqual(item, "extension") != 0)
                {
                    if (flagExtension != -1)
                    {
                        if ((flags[0] & flagExtension) == 0)
                            flags[0] |= flagExtension;
                    }
                    else
                        ret = 1;
                }
                else if (strEqual(item, "restriction") != 0)
                {
                    if (flagRestriction != -1)
                    {
                        if ((flags[0] & flagRestriction) == 0)
                            flags[0] |= flagRestriction;
                    }
                    else
                        ret = 1;
                }
                else if (strEqual(item, "substitution") != 0)
                {
                    if (flagSubstitution != -1)
                    {
                        if ((flags[0] & flagSubstitution) == 0)
                            flags[0] |= flagSubstitution;
                    }
                    else
                        ret = 1;
                }
                else if (strEqual(item, "list") != 0)
                {
                    if (flagList != -1)
                    {
                        if ((flags[0] & flagList) == 0)
                            flags[0] |= flagList;
                    }
                    else
                        ret = 1;
                }
                else if (strEqual(item, "union") != 0)
                {
                    if (flagUnion != -1)
                    {
                        if ((flags[0] & flagUnion) == 0)
                            flags[0] |= flagUnion;
                    }
                    else
                        ret = 1;
                }
                else
                    ret = 1;
                cur = end;
            } while ((ret == 0) && (ch(value, cur) != 0));
        }

        return ret;
    }

    // xml_xsd10_check_c_selector_x_path
    /** Port of xmlSchemaCheckCSelectorXPath. */
    static int checkCSelectorXPath(Xsd10ParserCtxt ctxt, Xsd10IDC idc, Xsd10IDCSelect selector, XmlNode attr, int isField)
    {
        XmlNode node;

        /*
        * c-selector-xpath:
        * Schema Component Constraint: Selector Value OK
        *
        * TODO: 1 The {selector} must be a valid XPath expression, as defined
        * in [XPath].
        */
        if (selector == null)
        {
            pErr(ctxt, idc.node,
                XML_XSD10_SCHEMAP_INTERNAL,
                "Internal error: xmlSchemaCheckCSelectorXPath, " +
                "the selector is not specified.\n", null, null);
            return -1;
        }
        if (attr == null)
            node = idc.node;
        else
            node = attr;
        if (selector.xpath == null)
        {
            pCustomErr(ctxt,
                /* TODO: Adjust error code. */
                XML_XSD10_SCHEMAP_S4S_ATTR_INVALID_VALUE,
                null, node,
                "The XPath expression of the selector is not valid", null);
            return XML_XSD10_SCHEMAP_S4S_ATTR_INVALID_VALUE;
        }
        else
        {
            String[] nsArray = null;
            XmlNs[] nsList;
            /*
            * Compile the XPath expression.
            */
            /*
            * TODO: We need the array of in-scope namespaces for compilation.
            * TODO: Call xmlPatterncompile with different options for selector/
            * field.
            */
            if (attr == null)
                nsList = null;
            else
                nsList = treeGetNsList(nodeParent(attr));
            /*
            * Build an array of prefixes and namespaces.
            */
            if (nsList != null)
            {
                int i, count = 0;

                for (i = 0; (i < nsList.length) && (nsList[i] != null); i++)
                    count++;

                nsArray = new String[count * 2 + 1];
                for (i = 0; i < count; i++)
                {
                    nsArray[2 * i] = nsHref(nsList[i]);
                    nsArray[2 * i + 1] = nsPrefix(nsList[i]);
                }
                nsArray[count * 2] = null;
            }
            /*
            * TODO: Differentiate between "selector" and "field".
            */
            if (isField != 0)
                selector.xpathComp = XmlPattern.patternCompile(selector.xpath,
                    XmlPattern.XML_PATTERN_FLAG_XSFIELD, nsArray);
            else
                selector.xpathComp = XmlPattern.patternCompile(selector.xpath,
                    XmlPattern.XML_PATTERN_FLAG_XSSEL, nsArray);

            if (selector.xpathComp == null)
            {
                pCustomErr(ctxt,
                    /* TODO: Adjust error code? */
                    XML_XSD10_SCHEMAP_S4S_ATTR_INVALID_VALUE,
                    null, node,
                    "The XPath expression '%s' could not be " +
                    "compiled", selector.xpath);
                return XML_XSD10_SCHEMAP_S4S_ATTR_INVALID_VALUE;
            }
        }
        return 0;
    }

    // XSD10_ADD_ANNOTATION
    /** Appends annot to the annotation list head, returns the new head. */
    private static Xsd10Annot addAnnotationToList(Xsd10Annot head, Xsd10Annot annot)
    {
        if (head == null)
            return annot;
        Xsd10Annot cur = head;
        while (cur.next != null)
        {
            cur = cur.next;
        }
        cur.next = annot;
        return head;
    }

    // xml_xsd10_add_annotation
    /** Port of xmlSchemaAddAnnotation. */
    static Xsd10Annot addAnnotation(Xsd10BasicItem annItem, Xsd10Annot annot)
    {
        if ((annItem == null) || (annot == null))
            return null;
        switch (annItem.type)
        {
        case XML_XSD10_SCHEMA_TYPE_ELEMENT:
            if (annItem instanceof Xsd10Element item)
                item.annot = addAnnotationToList(item.annot, annot);
            break;
        case XML_XSD10_SCHEMA_TYPE_ATTRIBUTE:
            if (annItem instanceof Xsd10Attribute item)
                item.annot = addAnnotationToList(item.annot, annot);
            break;
        case XML_XSD10_SCHEMA_TYPE_ANY_ATTRIBUTE:
        case XML_XSD10_SCHEMA_TYPE_ANY:
            if (annItem instanceof Xsd10Wildcard item)
                item.annot = addAnnotationToList(item.annot, annot);
            break;
        case XML_XSD10_SCHEMA_TYPE_PARTICLE:
        case XML_XSD10_SCHEMA_TYPE_IDC_KEY:
        case XML_XSD10_SCHEMA_TYPE_IDC_KEYREF:
        case XML_XSD10_SCHEMA_TYPE_IDC_UNIQUE:
            if (annItem instanceof Xsd10AnnotItem item)
                item.annot = addAnnotationToList(item.annot, annot);
            break;
        case XML_XSD10_SCHEMA_TYPE_ATTRIBUTEGROUP:
            if (annItem instanceof Xsd10AttributeGroup item)
                item.annot = addAnnotationToList(item.annot, annot);
            break;
        case XML_XSD10_SCHEMA_TYPE_NOTATION:
            if (annItem instanceof Xsd10Notation item)
                item.annot = addAnnotationToList(item.annot, annot);
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
            if (annItem instanceof Xsd10Facet item)
                item.annot = addAnnotationToList(item.annot, annot);
            break;
        case XML_XSD10_SCHEMA_TYPE_SIMPLE:
        case XML_XSD10_SCHEMA_TYPE_COMPLEX:
            if (annItem instanceof Xsd10Type item)
                item.annot = addAnnotationToList(item.annot, annot);
            break;
        case XML_XSD10_SCHEMA_TYPE_GROUP:
            if (annItem instanceof Xsd10ModelGroupDef item)
                item.annot = addAnnotationToList(item.annot, annot);
            break;
        case XML_XSD10_SCHEMA_TYPE_SEQUENCE:
        case XML_XSD10_SCHEMA_TYPE_CHOICE:
        case XML_XSD10_SCHEMA_TYPE_ALL:
            if (annItem instanceof Xsd10ModelGroup item)
                item.annot = addAnnotationToList(item.annot, annot);
            break;
        default:
            pCustomErr(null,
                XML_XSD10_SCHEMAP_INTERNAL,
                null, null,
                "Internal error: xmlSchemaAddAnnotation, " +
                "The item is not a annotated schema component", null);
            break;
        }
        return annot;
    }

    // xml_xsd10_parse_idc_selector_and_field
    /** Port of xmlSchemaParseIDCSelectorAndField. */
    static Xsd10IDCSelect parseIdcSelectorAndField(Xsd10ParserCtxt ctxt, Xsd10IDC idc, XmlNode node, int isField)
    {
        Xsd10IDCSelect item;
        XmlNode child;
        XmlNode attr;

        /*
        * Check for illegal attributes.
        */
        attr = parseFirstAttr(node);
        while (attr != null)
        {
            if (nodeNsUri(attr) == null)
            {
                if ((strEqual(nodeName(attr), "id") == 0) &&
                    (strEqual(nodeName(attr), "xpath") == 0))
                {
                    pIllegalAttrErr(ctxt,
                        XML_XSD10_SCHEMAP_S4S_ATTR_NOT_ALLOWED, null, attr);
                }
            }
            else if (strEqual(nodeNsUri(attr), XML_XSD10_NS) != 0)
            {
                pIllegalAttrErr(ctxt,
                    XML_XSD10_SCHEMAP_S4S_ATTR_NOT_ALLOWED, null, attr);
            }
            attr = nodeNext(attr);
        }
        /*
        * Create the item.
        */
        item = new Xsd10IDCSelect();
        /*
        * Attribute "xpath" (mandatory).
        */
        attr = getPropNode(node, "xpath");
        if (attr == null)
        {
            pMissingAttrErr(ctxt,
                XML_XSD10_SCHEMAP_S4S_ATTR_MISSING,
                null, node,
                "name", null);
        }
        else
        {
            item.xpath = getNodeContent(ctxt, attr);
            /*
            * URGENT TODO: "field"s have an other syntax than "selector"s.
            */

            if (checkCSelectorXPath(ctxt, idc, item, attr, isField) == -1)
            {
                pErr(ctxt,
                    attr,
                    XML_XSD10_SCHEMAP_INTERNAL,
                    "Internal error: xmlSchemaParseIDCSelectorAndField, " +
                    "validating the XPath expression of a IDC selector.\n",
                    null, null);
            }

        }
        pValAttrId(ctxt, node, "id");
        /*
        * And now for the children...
        */
        child = parseFirstChild(node);
        if (isSchema(child, "annotation") != 0)
        {
            /*
            * Add the annotation to the parent IDC.
            */
            addAnnotation(idc, parseAnnotation(ctxt, child, 1));
            child = parseNext(child);
        }
        if (child != null)
        {
            pContentErr(ctxt,
                XML_XSD10_SCHEMAP_S4S_ELEM_NOT_ALLOWED,
                null, node, child,
                null, "(annotation?)");
        }

        return item;
    }

    // xml_xsd10_parse_idc
    /** Port of xmlSchemaParseIDC. */
    static Xsd10IDC parseIdc(Xsd10ParserCtxt ctxt, Xsd10Schema schema, XmlNode node, int idcCategory, String targetNamespace)
    {
        Xsd10IDC item;
        XmlNode child;
        XmlNode attr;
        String[] name = { null };
        Xsd10IDCSelect field, lastField = null;

        /*
        * Check for illegal attributes.
        */
        attr = parseFirstAttr(node);
        while (attr != null)
        {
            if (nodeNsUri(attr) == null)
            {
                if ((strEqual(nodeName(attr), "id") == 0) &&
                    (strEqual(nodeName(attr), "name") == 0) &&
                    ((idcCategory != XML_XSD10_SCHEMA_TYPE_IDC_KEYREF) ||
                     (strEqual(nodeName(attr), "refer") == 0)))
                {
                    pIllegalAttrErr(ctxt,
                        XML_XSD10_SCHEMAP_S4S_ATTR_NOT_ALLOWED, null, attr);
                }
            }
            else if (strEqual(nodeNsUri(attr), XML_XSD10_NS) != 0)
            {
                pIllegalAttrErr(ctxt,
                    XML_XSD10_SCHEMAP_S4S_ATTR_NOT_ALLOWED, null, attr);
            }
            attr = nodeNext(attr);
        }
        /*
        * Attribute "name" (mandatory).
        */
        attr = getPropNode(node, "name");
        if (attr == null)
        {
            pMissingAttrErr(ctxt,
                XML_XSD10_SCHEMAP_S4S_ATTR_MISSING,
                null, node,
                "name", null);
            return null;
        }
        else if (pValAttrNode(ctxt,
            null, attr,
            getBuiltInType(XML_XSD10_SCHEMAS_NCNAME), name) != 0)
        {
            return null;
        }
        /* Create the component. */
        item = addIdc(ctxt, schema, name[0], targetNamespace,
            idcCategory, node);
        if (item == null)
            return null;

        pValAttrId(ctxt, node, "id");
        if (idcCategory == XML_XSD10_SCHEMA_TYPE_IDC_KEYREF)
        {
            /*
            * Attribute "refer" (mandatory).
            */
            attr = getPropNode(node, "refer");
            if (attr == null)
            {
                pMissingAttrErr(ctxt,
                    XML_XSD10_SCHEMAP_S4S_ATTR_MISSING,
                    null, node,
                    "refer", null);
            }
            else
            {
                /*
                * Create a reference item.
                */
                item.ref = newQNameRef(ctxt, XML_XSD10_SCHEMA_TYPE_IDC_KEY,
                    null, null);
                if (item.ref == null)
                    return null;
                String[] u = { null }, l = { null };
                pValAttrNodeQName(ctxt, schema,
                    null, attr, u, l);
                item.ref.targetNamespace = u[0];
                item.ref.name = l[0];
                checkReference(ctxt, schema, node, attr,
                    item.ref.targetNamespace);
            }
        }
        /*
        * And now for the children...
        */
        child = parseFirstChild(node);
        if (isSchema(child, "annotation") != 0)
        {
            item.annot = parseAnnotation(ctxt, child, 1);
            child = parseNext(child);
        }
        if (child == null)
        {
            pContentErr(ctxt,
                XML_XSD10_SCHEMAP_S4S_ELEM_MISSING,
                null, node, child,
                "A child element is missing",
                "(annotation?, (selector, field+))");
        }
        /*
        * Child element <selector>.
        */
        if (isSchema(child, "selector") != 0)
        {
            item.selector = parseIdcSelectorAndField(ctxt,
                item, child, 0);
            child = parseNext(child);
            /*
            * Child elements <field>.
            */
            if (isSchema(child, "field") != 0)
            {
                do
                {
                    field = parseIdcSelectorAndField(ctxt,
                        item, child, 1);
                    if (field != null)
                    {
                        field.index = item.nbFields;
                        item.nbFields++;
                        if (lastField != null)
                            lastField.next = field;
                        else
                            item.fields = field;
                        lastField = field;
                    }
                    child = parseNext(child);
                } while (isSchema(child, "field") != 0);
            }
            else
            {
                pContentErr(ctxt,
                    XML_XSD10_SCHEMAP_S4S_ELEM_NOT_ALLOWED,
                    null, node, child,
                    null, "(annotation?, (selector, field+))");
            }
        }
        if (child != null)
        {
            pContentErr(ctxt,
                XML_XSD10_SCHEMAP_S4S_ELEM_NOT_ALLOWED,
                null, node, child,
                null, "(annotation?, (selector, field+))");
        }

        return item;
    }

    /** The return_null exit of xmlSchemaParseElement. */
    private static Xsd10BasicItem parseElementReturnNull(Xsd10Particle particle, Xsd10Element decl, Xsd10Annot annot)
    {
        if (annot != null)
        {
            if (particle != null)
                particle.annot = null;
            if (decl != null)
                decl.annot = null;
            freeAnnot(annot);
        }
        return null;
    }

    // xml_xsd10_parse_element
    /** Port of xmlSchemaParseElement. Arrays: {@code isElemRef} is C {@code int *} (an out param: caller allocated array of length 1; a real C array: the array). */
    static Xsd10BasicItem parseElement(Xsd10ParserCtxt ctxt, Xsd10Schema schema, XmlNode node, int[] isElemRef, int topLevel)
    {
        Xsd10Element decl = null;
        Xsd10Particle particle = null;
        Xsd10Annot annot = null;
        XmlNode child;
        XmlNode attr, nameAttr;
        int min, max, isRef = 0;

        /* 3.3.3 Constraints on XML Representations of Element Declarations */
        /* TODO: Complete implementation of 3.3.6 */

        if ((ctxt == null) || (schema == null) || (node == null))
            return null;

        if (isElemRef != null)
            isElemRef[0] = 0;
        /*
        * If we get a "ref" attribute on a local <element> we will assume it's
        * a reference - even if there's a "name" attribute; this seems to be more
        * robust.
        */
        nameAttr = getPropNode(node, "name");
        attr = getPropNode(node, "ref");
        if ((topLevel != 0) || (attr == null))
        {
            if (nameAttr == null)
            {
                pMissingAttrErr(ctxt,
                    XML_XSD10_SCHEMAP_S4S_ATTR_MISSING,
                    null, node, "name", null);
                return null;
            }
        }
        else
            isRef = 1;

        pValAttrId(ctxt, node, "id");
        child = parseFirstChild(node);
        if (isSchema(child, "annotation") != 0)
        {
            annot = parseAnnotation(ctxt, child, 1);
            child = parseNext(child);
        }
        /*
        * Skip particle part if a global declaration.
        */
        if (topLevel == 0)
        {
            /*
            * The particle part ==================================================
            */
            min = getMinOccurs(ctxt, node, 0, -1, 1, "xs:nonNegativeInteger");
            max = getMaxOccurs(ctxt, node, 0, XML_XSD10_UNBOUNDED, 1, "(xs:nonNegativeInteger | unbounded)");
            pCheckParticleCorrect2(ctxt, null, node, min, max);
            particle = addParticle(ctxt, node, min, max);
            if (particle == null)
                return parseElementReturnNull(particle, decl, annot);

            /* ret->flags |= XML_SCHEMAS_ELEM_REF; */

            if (isRef != 0)
            {
                String[] refNs = { null }, ref = { null };
                Xsd10QNameRef refer;
                /*
                * The reference part =============================================
                */
                if (isElemRef != null)
                    isElemRef[0] = 1;

                pValAttrNodeQName(ctxt, schema,
                    null, attr, refNs, ref);
                checkReference(ctxt, schema, node, attr, refNs[0]);
                /*
                * SPEC (3.3.3 : 2.1) "One of ref or name must be present, but not both"
                */
                if (nameAttr != null)
                {
                    pMutualExclAttrErr(ctxt,
                        XML_XSD10_SCHEMAP_SRC_ELEMENT_2_1, null, nameAttr, "ref", "name");
                }
                /*
                * Check for illegal attributes.
                */
                attr = parseFirstAttr(node);
                while (attr != null)
                {
                    if (nodeNsUri(attr) == null)
                    {
                        if ((strEqual(nodeName(attr), "ref") != 0) ||
                            (strEqual(nodeName(attr), "name") != 0) ||
                            (strEqual(nodeName(attr), "id") != 0) ||
                            (strEqual(nodeName(attr), "maxOccurs") != 0) ||
                            (strEqual(nodeName(attr), "minOccurs") != 0))
                        {
                            attr = nodeNext(attr);
                            continue;
                        }
                        else
                        {
                            /* SPEC (3.3.3 : 2.2) */
                            pCustomAttrErr(ctxt,
                                XML_XSD10_SCHEMAP_SRC_ELEMENT_2_2,
                                null, null, attr,
                                "Only the attributes 'minOccurs', 'maxOccurs' and " +
                                "'id' are allowed in addition to 'ref'");
                            break;
                        }
                    }
                    else if (strEqual(nodeNsUri(attr), XML_XSD10_NS) != 0)
                    {
                        pIllegalAttrErr(ctxt,
                            XML_XSD10_SCHEMAP_S4S_ATTR_NOT_ALLOWED, null, attr);
                    }
                    attr = nodeNext(attr);
                }
                /*
                * No children except <annotation> expected.
                */
                if (child != null)
                {
                    pContentErr(ctxt, XML_XSD10_SCHEMAP_S4S_ELEM_NOT_ALLOWED,
                        null, node, child, null, "(annotation?)");
                }
                if ((min == 0) && (max == 0))
                    return parseElementReturnNull(particle, decl, annot);
                /*
                * Create the reference item and attach it to the particle.
                */
                refer = newQNameRef(ctxt, XML_XSD10_SCHEMA_TYPE_ELEMENT,
                    ref[0], refNs[0]);
                if (refer == null)
                    return parseElementReturnNull(particle, decl, annot);
                particle.children = refer;
                particle.annot = annot;
                /*
                * Add the particle to pending components, since the reference
                * need to be resolved.
                */
                wxsAddPending(ctxt, particle);
                return particle;
            }
        }
        /*
        * The declaration part ===============================================
        */
        /* declaration_part: */
        {
            String ns = null, fixed, attrValue;
            String[] name = { null };
            Xsd10IDC curIDC = null, lastIDC = null;

            if (pValAttrNode(ctxt, null, nameAttr,
                getBuiltInType(XML_XSD10_SCHEMAS_NCNAME), name) != 0)
                return parseElementReturnNull(particle, decl, annot);
            /*
            * Evaluate the target namespace.
            */
            if (topLevel != 0)
            {
                ns = ctxt.targetNamespace;
            }
            else
            {
                attr = getPropNode(node, "form");
                if (attr != null)
                {
                    attrValue = getNodeContent(ctxt, attr);
                    if (strEqual(attrValue, "qualified") != 0)
                    {
                        ns = ctxt.targetNamespace;
                    }
                    else if (strEqual(attrValue, "unqualified") == 0)
                    {
                        pSimpleTypeErr(ctxt,
                            XML_XSD10_SCHEMAP_S4S_ATTR_INVALID_VALUE,
                            null, attr,
                            null, "(qualified | unqualified)",
                            attrValue, null, null, null);
                    }
                }
                else if ((schema.flags & XML_XSD10_SCHEMAS_QUALIF_ELEM) != 0)
                    ns = ctxt.targetNamespace;
            }
            decl = addElement(ctxt, name[0], ns, node, topLevel);
            if (decl == null)
            {
                return parseElementReturnNull(particle, decl, annot);
            }
            /*
            * Check for illegal attributes.
            */
            attr = parseFirstAttr(node);
            while (attr != null)
            {
                if (nodeNsUri(attr) == null)
                {
                    if ((strEqual(nodeName(attr), "name") == 0) &&
                        (strEqual(nodeName(attr), "type") == 0) &&
                        (strEqual(nodeName(attr), "id") == 0) &&
                        (strEqual(nodeName(attr), "default") == 0) &&
                        (strEqual(nodeName(attr), "fixed") == 0) &&
                        (strEqual(nodeName(attr), "block") == 0) &&
                        (strEqual(nodeName(attr), "nillable") == 0))
                    {
                        if (topLevel == 0)
                        {
                            if ((strEqual(nodeName(attr), "maxOccurs") == 0) &&
                                (strEqual(nodeName(attr), "minOccurs") == 0) &&
                                (strEqual(nodeName(attr), "form") == 0))
                            {
                                pIllegalAttrErr(ctxt,
                                    XML_XSD10_SCHEMAP_S4S_ATTR_NOT_ALLOWED, null, attr);
                            }
                        }
                        else if ((strEqual(nodeName(attr), "final") == 0) &&
                            (strEqual(nodeName(attr), "abstract") == 0) &&
                            (strEqual(nodeName(attr), "substitutionGroup") == 0))
                        {

                            pIllegalAttrErr(ctxt,
                                XML_XSD10_SCHEMAP_S4S_ATTR_NOT_ALLOWED, null, attr);
                        }
                    }
                }
                else if (strEqual(nodeNsUri(attr), XML_XSD10_NS) != 0)
                {

                    pIllegalAttrErr(ctxt,
                        XML_XSD10_SCHEMAP_S4S_ATTR_NOT_ALLOWED, null, attr);
                }
                attr = nodeNext(attr);
            }
            /*
            * Extract/validate attributes.
            */
            if (topLevel != 0)
            {
                /*
                * Process top attributes of global element declarations here.
                */
                decl.flags |= XML_XSD10_SCHEMAS_ELEM_GLOBAL;
                decl.flags |= XML_XSD10_SCHEMAS_ELEM_TOPLEVEL;
                {
                    String[] u = { null }, l = { null };
                    pValAttrQName(ctxt, schema,
                        null, node, "substitutionGroup", u, l);
                    decl.substGroupNs = u[0];
                    decl.substGroup = l[0];
                }
                if (getBooleanProp(ctxt, node, "abstract", 0) != 0)
                    decl.flags |= XML_XSD10_SCHEMAS_ELEM_ABSTRACT;
                /*
                * Attribute "final".
                */
                attr = getPropNode(node, "final");
                if (attr == null)
                {
                    if ((schema.flags & XML_XSD10_SCHEMAS_FINAL_DEFAULT_EXTENSION) != 0)
                        decl.flags |= XML_XSD10_SCHEMAS_ELEM_FINAL_EXTENSION;
                    if ((schema.flags & XML_XSD10_SCHEMAS_FINAL_DEFAULT_RESTRICTION) != 0)
                        decl.flags |= XML_XSD10_SCHEMAS_ELEM_FINAL_RESTRICTION;
                }
                else
                {
                    attrValue = getNodeContent(ctxt, attr);
                    int[] fl = { decl.flags };
                    int r = pValAttrBlockFinal(attrValue, fl,
                        -1,
                        XML_XSD10_SCHEMAS_ELEM_FINAL_EXTENSION,
                        XML_XSD10_SCHEMAS_ELEM_FINAL_RESTRICTION, -1, -1, -1);
                    decl.flags = fl[0];
                    if (r != 0)
                    {
                        pSimpleTypeErr(ctxt,
                            XML_XSD10_SCHEMAP_S4S_ATTR_INVALID_VALUE,
                            null, attr,
                            null, "(#all | List of (extension | restriction))",
                            attrValue, null, null, null);
                    }
                }
            }
            /*
            * Attribute "block".
            */
            attr = getPropNode(node, "block");
            if (attr == null)
            {
                /*
                * Apply default "block" values.
                */
                if ((schema.flags & XML_XSD10_SCHEMAS_BLOCK_DEFAULT_RESTRICTION) != 0)
                    decl.flags |= XML_XSD10_SCHEMAS_ELEM_BLOCK_RESTRICTION;
                if ((schema.flags & XML_XSD10_SCHEMAS_BLOCK_DEFAULT_EXTENSION) != 0)
                    decl.flags |= XML_XSD10_SCHEMAS_ELEM_BLOCK_EXTENSION;
                if ((schema.flags & XML_XSD10_SCHEMAS_BLOCK_DEFAULT_SUBSTITUTION) != 0)
                    decl.flags |= XML_XSD10_SCHEMAS_ELEM_BLOCK_SUBSTITUTION;
            }
            else
            {
                attrValue = getNodeContent(ctxt, attr);
                int[] fl = { decl.flags };
                int r = pValAttrBlockFinal(attrValue, fl,
                    -1,
                    XML_XSD10_SCHEMAS_ELEM_BLOCK_EXTENSION,
                    XML_XSD10_SCHEMAS_ELEM_BLOCK_RESTRICTION,
                    XML_XSD10_SCHEMAS_ELEM_BLOCK_SUBSTITUTION, -1, -1);
                decl.flags = fl[0];
                if (r != 0)
                {
                    pSimpleTypeErr(ctxt,
                        XML_XSD10_SCHEMAP_S4S_ATTR_INVALID_VALUE,
                        null, attr,
                        null, "(#all | List of (extension | " +
                        "restriction | substitution))", attrValue,
                        null, null, null);
                }
            }
            if (getBooleanProp(ctxt, node, "nillable", 0) != 0)
                decl.flags |= XML_XSD10_SCHEMAS_ELEM_NILLABLE;

            attr = getPropNode(node, "type");
            if (attr != null)
            {
                String[] u = { null }, l = { null };
                pValAttrNodeQName(ctxt, schema,
                    null, attr, u, l);
                decl.namedTypeNs = u[0];
                decl.namedType = l[0];
                checkReference(ctxt, schema, node,
                    attr, decl.namedTypeNs);
            }
            decl.value = getProp(ctxt, node, "default");
            attr = getPropNode(node, "fixed");
            if (attr != null)
            {
                fixed = getNodeContent(ctxt, attr);
                if (decl.value != null)
                {
                    /*
                    * 3.3.3 : 1
                    * default and fixed must not both be present.
                    */
                    pMutualExclAttrErr(ctxt,
                        XML_XSD10_SCHEMAP_SRC_ELEMENT_1,
                        null, attr, "default", "fixed");
                }
                else
                {
                    decl.flags |= XML_XSD10_SCHEMAS_ELEM_FIXED;
                    decl.value = fixed;
                }
            }
            /*
            * And now for the children...
            */
            if (isSchema(child, "complexType") != 0)
            {
                /*
                * 3.3.3 : 3
                * "type" and either <simpleType> or <complexType> are mutually
                * exclusive
                */
                if (decl.namedType != null)
                {
                    pContentErr(ctxt,
                        XML_XSD10_SCHEMAP_SRC_ELEMENT_3,
                        null, node, child,
                        "The attribute 'type' and the <complexType> child are " +
                        "mutually exclusive", null);
                }
                else
                    decl.subtypes = parseComplexType(ctxt, schema, child, 0);
                child = parseNext(child);
            }
            else if (isSchema(child, "simpleType") != 0)
            {
                /*
                * 3.3.3 : 3
                * "type" and either <simpleType> or <complexType> are
                * mutually exclusive
                */
                if (decl.namedType != null)
                {
                    pContentErr(ctxt,
                        XML_XSD10_SCHEMAP_SRC_ELEMENT_3,
                        null, node, child,
                        "The attribute 'type' and the <simpleType> child are " +
                        "mutually exclusive", null);
                }
                else
                    decl.subtypes = parseSimpleType(ctxt, schema, child, 0);
                child = parseNext(child);
            }
            while ((isSchema(child, "unique") != 0) ||
                   (isSchema(child, "key") != 0) || (isSchema(child, "keyref") != 0))
            {
                if (isSchema(child, "unique") != 0)
                {
                    curIDC = parseIdc(ctxt, schema, child,
                        XML_XSD10_SCHEMA_TYPE_IDC_UNIQUE, decl.targetNamespace);
                }
                else if (isSchema(child, "key") != 0)
                {
                    curIDC = parseIdc(ctxt, schema, child,
                        XML_XSD10_SCHEMA_TYPE_IDC_KEY, decl.targetNamespace);
                }
                else if (isSchema(child, "keyref") != 0)
                {
                    curIDC = parseIdc(ctxt, schema, child,
                        XML_XSD10_SCHEMA_TYPE_IDC_KEYREF, decl.targetNamespace);
                }
                if (lastIDC != null)
                    lastIDC.next = curIDC;
                else
                    decl.idcs = curIDC;
                lastIDC = curIDC;
                child = parseNext(child);
            }
            if (child != null)
            {
                pContentErr(ctxt,
                    XML_XSD10_SCHEMAP_S4S_ELEM_NOT_ALLOWED,
                    null, node, child,
                    null, "(annotation?, ((simpleType | complexType)?, " +
                    "(unique | key | keyref)*))");
            }
            decl.annot = annot;
        }
        /*
        * NOTE: Element Declaration Representation OK 4. will be checked at a
        * different layer.
        */
        if (topLevel != 0)
            return decl;
        else
        {
            particle.children = decl;
            return particle;
        }
    }

    // xml_xsd10_parse_union
    /** Port of xmlSchemaParseUnion. */
    static int parseUnion(Xsd10ParserCtxt ctxt, Xsd10Schema schema, XmlNode node)
    {
        Xsd10Type type;
        XmlNode child;
        XmlNode attr;
        String cur;

        if ((ctxt == null) || (schema == null) || (node == null))
            return -1;
        /* Not a component, don't create it. */
        type = ctxt.ctxtType;
        if (type == null)
            return -1;
        /*
        * Mark the simple type as being of variety "union".
        */
        type.flags |= XML_XSD10_SCHEMAS_TYPE_VARIETY_UNION;
        /*
        * SPEC (Base type) (2) "If the <list> or <union> alternative is chosen,
        * then the `simple ur-type definition`."
        */
        type.baseType = getBuiltInType(XML_XSD10_SCHEMAS_ANYSIMPLETYPE);
        /*
        * Check for illegal attributes.
        */
        attr = parseFirstAttr(node);
        while (attr != null)
        {
            if (nodeNsUri(attr) == null)
            {
                if ((strEqual(nodeName(attr), "id") == 0) &&
                    (strEqual(nodeName(attr), "memberTypes") == 0))
                {
                    pIllegalAttrErr(ctxt,
                        XML_XSD10_SCHEMAP_S4S_ATTR_NOT_ALLOWED, null, attr);
                }
            }
            else if (strEqual(nodeNsUri(attr), XML_XSD10_NS) != 0)
            {
                pIllegalAttrErr(ctxt,
                    XML_XSD10_SCHEMAP_S4S_ATTR_NOT_ALLOWED, null, attr);
            }
            attr = nodeNext(attr);
        }
        pValAttrId(ctxt, node, "id");
        /*
        * Attribute "memberTypes". This is a list of QNames.
        * TODO: Check the value to contain anything.
        */
        attr = getPropNode(node, "memberTypes");
        if (attr != null)
        {
            int end, pos;
            String tmp;
            String[] localName = { null }, nsName = { null };
            Xsd10TypeLink link, lastLink = null;
            Xsd10QNameRef ref;

            cur = getNodeContent(ctxt, attr);
            if (cur == null)
                return -1;
            type.base = cur;
            pos = 0;
            do
            {
                while (isBlankCh(ch(cur, pos)))
                    pos++;
                end = pos;
                while ((ch(cur, end) != 0) && (!isBlankCh(ch(cur, end))))
                    end++;
                if (end == pos)
                    break;
                tmp = cur.substring(pos, end);
                if (pValAttrNodeQNameValue(ctxt, schema,
                    null, attr, tmp, nsName, localName) == 0)
                {
                    /*
                    * Create the member type link.
                    */
                    link = new Xsd10TypeLink();
                    link.type = null;
                    link.next = null;
                    /*
                    * Create a reference item.
                    */
                    ref = newQNameRef(ctxt, XML_XSD10_SCHEMA_TYPE_SIMPLE,
                        localName[0], nsName[0]);
                    if (ref == null)
                    {
                        pErrMemory(ctxt);
                        return -1;
                    }
                    /*
                    * Assign the reference to the link, it will be resolved
                    * later during fixup of the union simple type.
                    */
                    link.type = ref;

                    if (lastLink == null)
                        type.memberTypes = link;
                    else
                        lastLink.next = link;
                    lastLink = link;
                }
                pos = end;
            } while (ch(cur, pos) != 0);

        }
        /*
        * And now for the children...
        */
        child = parseFirstChild(node);
        if (isSchema(child, "annotation") != 0)
        {
            /*
            * Add the annotation to the simple type ancestor.
            */
            addAnnotation(type, parseAnnotation(ctxt, child, 1));
            child = parseNext(child);
        }
        if (isSchema(child, "simpleType") != 0)
        {
            Xsd10Type subtype, last = null;

            /*
            * Anchor the member types in the "subtypes" field of the
            * simple type.
            */
            while (isSchema(child, "simpleType") != 0)
            {
                subtype = parseSimpleType(ctxt, schema, child, 0);
                if (subtype != null)
                {
                    if (last == null)
                    {
                        type.subtypes = subtype;
                        last = subtype;
                    }
                    else
                    {
                        last.next = subtype;
                        last = subtype;
                    }
                    last.next = null;
                }
                child = parseNext(child);
            }
        }
        if (child != null)
        {
            pContentErr(ctxt,
                XML_XSD10_SCHEMAP_S4S_ELEM_NOT_ALLOWED,
                null, node, child, null, "(annotation?, simpleType*)");
        }
        if ((attr == null) && (type.subtypes == null))
        {
            /*
            * src-union-memberTypes-or-simpleTypes
            * Either the memberTypes [attribute] of the <union> element must
            * be non-empty or there must be at least one simpleType [child].
            */
            pCustomErr(ctxt,
                XML_XSD10_SCHEMAP_SRC_UNION_MEMBERTYPES_OR_SIMPLETYPES,
                null, node,
                "Either the attribute 'memberTypes' or " +
                "at least one <simpleType> child must be present", null);
        }
        return 0;
    }

    // xml_xsd10_parse_list
    /** Port of xmlSchemaParseList. */
    static Xsd10Type parseList(Xsd10ParserCtxt ctxt, Xsd10Schema schema, XmlNode node)
    {
        Xsd10Type type;
        XmlNode child;
        XmlNode attr;

        if ((ctxt == null) || (schema == null) || (node == null))
            return null;
        /* Not a component, don't create it. */
        type = ctxt.ctxtType;
        if (type == null)
            return null;
        /*
        * Mark the type as being of variety "list".
        */
        type.flags |= XML_XSD10_SCHEMAS_TYPE_VARIETY_LIST;
        /*
        * SPEC (Base type) (2) "If the <list> or <union> alternative is chosen,
        * then the `simple ur-type definition`."
        */
        type.baseType = getBuiltInType(XML_XSD10_SCHEMAS_ANYSIMPLETYPE);
        /*
        * Check for illegal attributes.
        */
        attr = parseFirstAttr(node);
        while (attr != null)
        {
            if (nodeNsUri(attr) == null)
            {
                if ((strEqual(nodeName(attr), "id") == 0) &&
                    (strEqual(nodeName(attr), "itemType") == 0))
                {
                    pIllegalAttrErr(ctxt,
                        XML_XSD10_SCHEMAP_S4S_ATTR_NOT_ALLOWED, null, attr);
                }
            }
            else if (strEqual(nodeNsUri(attr), XML_XSD10_NS) != 0)
            {
                pIllegalAttrErr(ctxt,
                    XML_XSD10_SCHEMAP_S4S_ATTR_NOT_ALLOWED, null, attr);
            }
            attr = nodeNext(attr);
        }

        pValAttrId(ctxt, node, "id");

        /*
        * Attribute "itemType". NOTE that we will use the "ref" and "refNs"
        * fields for holding the reference to the itemType.
        *
        * REVAMP TODO: Use the "base" and "baseNs" fields, since we will remove
        * the "ref" fields.
        */
        {
            String[] u = { null }, l = { null };
            pValAttrQName(ctxt, schema, null,
                node, "itemType", u, l);
            type.baseNs = u[0];
            type.base = l[0];
        }
        /*
        * And now for the children...
        */
        child = parseFirstChild(node);
        if (isSchema(child, "annotation") != 0)
        {
            addAnnotation(type, parseAnnotation(ctxt, child, 1));
            child = parseNext(child);
        }
        if (isSchema(child, "simpleType") != 0)
        {
            /*
            * src-list-itemType-or-simpleType
            * Either the itemType [attribute] or the <simpleType> [child] of
            * the <list> element must be present, but not both.
            */
            if (type.base != null)
            {
                pCustomErr(ctxt,
                    XML_XSD10_SCHEMAP_SRC_SIMPLE_TYPE_1,
                    null, node,
                    "The attribute 'itemType' and the <simpleType> child " +
                    "are mutually exclusive", null);
            }
            else
            {
                type.subtypes = parseSimpleType(ctxt, schema, child, 0);
            }
            child = parseNext(child);
        }
        else if (type.base == null)
        {
            pCustomErr(ctxt,
                XML_XSD10_SCHEMAP_SRC_SIMPLE_TYPE_1,
                null, node,
                "Either the attribute 'itemType' or the <simpleType> child " +
                "must be present", null);
        }
        if (child != null)
        {
            pContentErr(ctxt,
                XML_XSD10_SCHEMAP_S4S_ELEM_NOT_ALLOWED,
                null, node, child, null, "(annotation?, simpleType?)");
        }
        if ((type.base == null) &&
            (type.subtypes == null) &&
            (getPropNode(node, "itemType") == null))
        {
            pCustomErr(ctxt,
                XML_XSD10_SCHEMAP_SRC_SIMPLE_TYPE_1,
                null, node,
                "Either the attribute 'itemType' or the <simpleType> child " +
                "must be present", null);
        }
        return null;
    }

    // xml_xsd10_parse_simple_type
    /** Port of xmlSchemaParseSimpleType. */
    static Xsd10Type parseSimpleType(Xsd10ParserCtxt ctxt, Xsd10Schema schema, XmlNode node, int topLevel)
    {
        Xsd10Type type, oldCtxtType;
        XmlNode child;
        String[] attrValue = { null };
        XmlNode attr;
        int hasRestriction = 0;

        if ((ctxt == null) || (schema == null) || (node == null))
            return null;

        if (topLevel != 0)
        {
            attr = getPropNode(node, "name");
            if (attr == null)
            {
                pMissingAttrErr(ctxt,
                    XML_XSD10_SCHEMAP_S4S_ATTR_MISSING,
                    null, node,
                    "name", null);
                return null;
            }
            else
            {
                if (pValAttrNode(ctxt,
                    null, attr,
                    getBuiltInType(XML_XSD10_SCHEMAS_NCNAME), attrValue) != 0)
                    return null;
                /*
                * Skip built-in types.
                */
                if (ctxt.isS4S != 0)
                {
                    Xsd10Type biType;

                    if (ctxt.isRedefine != 0)
                    {
                        /*
                        * REDEFINE: Disallow redefinition of built-in-types.
                        * TODO: It seems that the spec does not say anything
                        * about this case.
                        */
                        pCustomErr(ctxt, XML_XSD10_SCHEMAP_SRC_REDEFINE,
                            null, node,
                            "Redefinition of built-in simple types is not " +
                            "supported", null);
                        return null;
                    }
                    biType = getPredefinedType(attrValue[0], XML_XSD10_NS);
                    if (biType != null)
                        return biType;
                }
            }
        }
        /*
        * TargetNamespace:
        * SPEC "The `actual value` of the targetNamespace [attribute]
        * of the <schema> ancestor element information item if present,
        * otherwise `absent`.
        */
        if (topLevel == 0)
        {
            /*
            * Parse as local simple type definition.
            */
            type = addType(ctxt, schema,
                XML_XSD10_SCHEMA_TYPE_SIMPLE,
                null, ctxt.targetNamespace, node, 0);
            if (type == null)
                return null;
            type.type = XML_XSD10_SCHEMA_TYPE_SIMPLE;
            type.contentType = XML_XSD10_SCHEMA_CONTENT_SIMPLE;
            /*
            * Check for illegal attributes.
            */
            attr = parseFirstAttr(node);
            while (attr != null)
            {
                if (nodeNsUri(attr) == null)
                {
                    if (strEqual(nodeName(attr), "id") == 0)
                    {
                        pIllegalAttrErr(ctxt,
                            XML_XSD10_SCHEMAP_S4S_ATTR_NOT_ALLOWED, null, attr);
                    }
                }
                else if (strEqual(nodeNsUri(attr), XML_XSD10_NS) != 0)
                {
                    pIllegalAttrErr(ctxt,
                        XML_XSD10_SCHEMAP_S4S_ATTR_NOT_ALLOWED, null, attr);
                }
                attr = nodeNext(attr);
            }
        }
        else
        {
            /*
            * Parse as global simple type definition.
            *
            * Note that attrValue is the value of the attribute "name" here.
            */
            type = addType(ctxt, schema, XML_XSD10_SCHEMA_TYPE_SIMPLE,
                attrValue[0], ctxt.targetNamespace, node, 1);
            if (type == null)
                return null;
            type.type = XML_XSD10_SCHEMA_TYPE_SIMPLE;
            type.contentType = XML_XSD10_SCHEMA_CONTENT_SIMPLE;
            type.flags |= XML_XSD10_SCHEMAS_TYPE_GLOBAL;
            /*
            * Check for illegal attributes.
            */
            attr = parseFirstAttr(node);
            while (attr != null)
            {
                if (nodeNsUri(attr) == null)
                {
                    if ((strEqual(nodeName(attr), "id") == 0) &&
                        (strEqual(nodeName(attr), "name") == 0) &&
                        (strEqual(nodeName(attr), "final") == 0))
                    {
                        pIllegalAttrErr(ctxt,
                            XML_XSD10_SCHEMAP_S4S_ATTR_NOT_ALLOWED, null, attr);
                    }
                }
                else if (strEqual(nodeNsUri(attr), XML_XSD10_NS) != 0)
                {
                    pIllegalAttrErr(ctxt,
                        XML_XSD10_SCHEMAP_S4S_ATTR_NOT_ALLOWED, null, attr);
                }
                attr = nodeNext(attr);
            }
            /*
            * Attribute "final".
            */
            attr = getPropNode(node, "final");
            if (attr == null)
            {
                if ((schema.flags & XML_XSD10_SCHEMAS_FINAL_DEFAULT_RESTRICTION) != 0)
                    type.flags |= XML_XSD10_SCHEMAS_TYPE_FINAL_RESTRICTION;
                if ((schema.flags & XML_XSD10_SCHEMAS_FINAL_DEFAULT_LIST) != 0)
                    type.flags |= XML_XSD10_SCHEMAS_TYPE_FINAL_LIST;
                if ((schema.flags & XML_XSD10_SCHEMAS_FINAL_DEFAULT_UNION) != 0)
                    type.flags |= XML_XSD10_SCHEMAS_TYPE_FINAL_UNION;
            }
            else
            {
                attrValue[0] = getProp(ctxt, node, "final");
                int[] fl = { type.flags };
                int r = pValAttrBlockFinal(attrValue[0], fl,
                    -1, -1, XML_XSD10_SCHEMAS_TYPE_FINAL_RESTRICTION, -1,
                    XML_XSD10_SCHEMAS_TYPE_FINAL_LIST,
                    XML_XSD10_SCHEMAS_TYPE_FINAL_UNION);
                type.flags = fl[0];
                if (r != 0)
                {

                    pSimpleTypeErr(ctxt,
                        XML_XSD10_SCHEMAP_S4S_ATTR_INVALID_VALUE,
                        type, attr,
                        null, "(#all | List of (list | union | restriction)",
                        attrValue[0], null, null, null);
                }
            }
        }
        type.targetNamespace = ctxt.targetNamespace;
        pValAttrId(ctxt, node, "id");
        /*
        * And now for the children...
        */
        oldCtxtType = ctxt.ctxtType;

        ctxt.ctxtType = type;

        child = parseFirstChild(node);
        if (isSchema(child, "annotation") != 0)
        {
            type.annot = parseAnnotation(ctxt, child, 1);
            child = parseNext(child);
        }
        if (child == null)
        {
            pContentErr(ctxt, XML_XSD10_SCHEMAP_S4S_ELEM_MISSING,
                null, node, child, null,
                "(annotation?, (restriction | list | union))");
        }
        else if (isSchema(child, "restriction") != 0)
        {
            parseRestriction(ctxt, schema, child,
                XML_XSD10_SCHEMA_TYPE_SIMPLE);
            hasRestriction = 1;
            child = parseNext(child);
        }
        else if (isSchema(child, "list") != 0)
        {
            parseList(ctxt, schema, child);
            child = parseNext(child);
        }
        else if (isSchema(child, "union") != 0)
        {
            parseUnion(ctxt, schema, child);
            child = parseNext(child);
        }
        if (child != null)
        {
            pContentErr(ctxt, XML_XSD10_SCHEMAP_S4S_ELEM_NOT_ALLOWED,
                null, node, child, null,
                "(annotation?, (restriction | list | union))");
        }
        /*
        * REDEFINE: SPEC src-redefine (5)
        * "Within the [children], each <simpleType> must have a
        * <restriction> among its [children] ... the `actual value` of whose
        * base [attribute] must be the same as the `actual value` of its own
        * name attribute plus target namespace;"
        */
        if ((topLevel != 0) && (ctxt.isRedefine != 0) && (hasRestriction == 0))
        {
            pCustomErr(ctxt, XML_XSD10_SCHEMAP_SRC_REDEFINE,
                null, node, "This is a redefinition, thus the " +
                "<simpleType> must have a <restriction> child", null);
        }

        ctxt.ctxtType = oldCtxtType;
        return type;
    }

    // xml_xsd10_parse_model_group_def_ref
    /** Port of xmlSchemaParseModelGroupDefRef. */
    static Xsd10TreeItem parseModelGroupDefRef(Xsd10ParserCtxt ctxt, Xsd10Schema schema, XmlNode node)
    {
        Xsd10Particle item;
        XmlNode child;
        XmlNode attr;
        String[] ref = { null }, refNs = { null };
        int min, max;

        if ((ctxt == null) || (schema == null) || (node == null))
            return null;

        attr = getPropNode(node, "ref");
        if (attr == null)
        {
            pMissingAttrErr(ctxt,
                XML_XSD10_SCHEMAP_S4S_ATTR_MISSING,
                null, node, "ref", null);
            return null;
        }
        else if (pValAttrNodeQName(ctxt, schema, null,
            attr, refNs, ref) != 0)
        {
            return null;
        }
        checkReference(ctxt, schema, node, attr, refNs[0]);
        min = getMinOccurs(ctxt, node, 0, -1, 1, "xs:nonNegativeInteger");
        max = getMaxOccurs(ctxt, node, 0, XML_XSD10_UNBOUNDED, 1,
            "(xs:nonNegativeInteger | unbounded)");
        /*
        * Check for illegal attributes.
        */
        attr = parseFirstAttr(node);
        while (attr != null)
        {
            if (nodeNsUri(attr) == null)
            {
                if ((strEqual(nodeName(attr), "ref") == 0) &&
                    (strEqual(nodeName(attr), "id") == 0) &&
                    (strEqual(nodeName(attr), "minOccurs") == 0) &&
                    (strEqual(nodeName(attr), "maxOccurs") == 0))
                {
                    pIllegalAttrErr(ctxt,
                        XML_XSD10_SCHEMAP_S4S_ATTR_NOT_ALLOWED, null, attr);
                }
            }
            else if (strEqual(nodeNsUri(attr), XML_XSD10_NS) != 0)
            {
                pIllegalAttrErr(ctxt,
                    XML_XSD10_SCHEMAP_S4S_ATTR_NOT_ALLOWED, null, attr);
            }
            attr = nodeNext(attr);
        }
        pValAttrId(ctxt, node, "id");
        item = addParticle(ctxt, node, min, max);
        if (item == null)
            return null;
        /*
        * Create a qname-reference and set as the term; it will be substituted
        * for the model group after the reference has been resolved.
        */
        item.children = newQNameRef(ctxt, XML_XSD10_SCHEMA_TYPE_GROUP, ref[0], refNs[0]);
        pCheckParticleCorrect2(ctxt, item, node, min, max);
        /*
        * And now for the children...
        */
        child = parseFirstChild(node);
        /* TODO: Is annotation even allowed for a model group reference? */
        if (isSchema(child, "annotation") != 0)
        {
            /*
            * TODO: What to do exactly with the annotation?
            */
            item.annot = parseAnnotation(ctxt, child, 1);
            child = parseNext(child);
        }
        if (child != null)
        {
            pContentErr(ctxt,
                XML_XSD10_SCHEMAP_S4S_ELEM_NOT_ALLOWED,
                null, node, child, null,
                "(annotation?)");
        }
        /*
        * Corresponds to no component at all if minOccurs==maxOccurs==0.
        */
        if ((min == 0) && (max == 0))
            return null;

        return item;
    }

    // xml_xsd10_parse_model_group_definition
    /** Port of xmlSchemaParseModelGroupDefinition. */
    static Xsd10ModelGroupDef parseModelGroupDefinition(Xsd10ParserCtxt ctxt, Xsd10Schema schema, XmlNode node)
    {
        Xsd10ModelGroupDef item;
        XmlNode child;
        XmlNode attr;
        String[] name = { null };

        if ((ctxt == null) || (schema == null) || (node == null))
            return null;

        attr = getPropNode(node, "name");
        if (attr == null)
        {
            pMissingAttrErr(ctxt,
                XML_XSD10_SCHEMAP_S4S_ATTR_MISSING,
                null, node,
                "name", null);
            return null;
        }
        else if (pValAttrNode(ctxt, null, attr,
            getBuiltInType(XML_XSD10_SCHEMAS_NCNAME), name) != 0)
        {
            return null;
        }
        item = addModelGroupDefinition(ctxt, schema, name[0],
            ctxt.targetNamespace, node);
        if (item == null)
            return null;
        /*
        * Check for illegal attributes.
        */
        attr = parseFirstAttr(node);
        while (attr != null)
        {
            if (nodeNsUri(attr) == null)
            {
                if ((strEqual(nodeName(attr), "name") == 0) &&
                    (strEqual(nodeName(attr), "id") == 0))
                {
                    pIllegalAttrErr(ctxt,
                        XML_XSD10_SCHEMAP_S4S_ATTR_NOT_ALLOWED, null, attr);
                }
            }
            else if (strEqual(nodeNsUri(attr), XML_XSD10_NS) != 0)
            {
                pIllegalAttrErr(ctxt,
                    XML_XSD10_SCHEMAP_S4S_ATTR_NOT_ALLOWED, null, attr);
            }
            attr = nodeNext(attr);
        }
        pValAttrId(ctxt, node, "id");
        /*
        * And now for the children...
        */
        child = parseFirstChild(node);
        if (isSchema(child, "annotation") != 0)
        {
            item.annot = parseAnnotation(ctxt, child, 1);
            child = parseNext(child);
        }
        if (isSchema(child, "all") != 0)
        {
            item.children = parseModelGroup(ctxt, schema, child,
                XML_XSD10_SCHEMA_TYPE_ALL, 0);
            child = parseNext(child);
        }
        else if (isSchema(child, "choice") != 0)
        {
            item.children = parseModelGroup(ctxt, schema, child,
                XML_XSD10_SCHEMA_TYPE_CHOICE, 0);
            child = parseNext(child);
        }
        else if (isSchema(child, "sequence") != 0)
        {
            item.children = parseModelGroup(ctxt, schema, child,
                XML_XSD10_SCHEMA_TYPE_SEQUENCE, 0);
            child = parseNext(child);
        }

        if (child != null)
        {
            pContentErr(ctxt,
                XML_XSD10_SCHEMAP_S4S_ELEM_NOT_ALLOWED,
                null, node, child, null,
                "(annotation?, (all | choice | sequence)?)");
        }
        return item;
    }

    // xml_xsd10_cleanup_doc
    /** Port of xmlSchemaCleanupDoc. */
    static void cleanupDoc(Xsd10ParserCtxt ctxt, XmlNode root)
    {
        /*
        * libxml2 removes all the blank text nodes, the comments, the processing
        * instructions and the entity references of the tree here (it does not
        * descend into elements whose first child is an entity reference).
        * The native port never modifies a schema document: the parsing functions
        * skip the nodes that would have been removed instead, see
        * xsd10_parse_first_child and xsd10_parse_next.
        */
    }

    // xml_xsd10_clear_schema_defaults
    /** Port of xmlSchemaClearSchemaDefaults. */
    static void clearSchemaDefaults(Xsd10Schema schema)
    {
        if ((schema.flags & XML_XSD10_SCHEMAS_QUALIF_ELEM) != 0)
            schema.flags ^= XML_XSD10_SCHEMAS_QUALIF_ELEM;

        if ((schema.flags & XML_XSD10_SCHEMAS_QUALIF_ATTR) != 0)
            schema.flags ^= XML_XSD10_SCHEMAS_QUALIF_ATTR;

        if ((schema.flags & XML_XSD10_SCHEMAS_FINAL_DEFAULT_EXTENSION) != 0)
            schema.flags ^= XML_XSD10_SCHEMAS_FINAL_DEFAULT_EXTENSION;
        if ((schema.flags & XML_XSD10_SCHEMAS_FINAL_DEFAULT_RESTRICTION) != 0)
            schema.flags ^= XML_XSD10_SCHEMAS_FINAL_DEFAULT_RESTRICTION;
        if ((schema.flags & XML_XSD10_SCHEMAS_FINAL_DEFAULT_LIST) != 0)
            schema.flags ^= XML_XSD10_SCHEMAS_FINAL_DEFAULT_LIST;
        if ((schema.flags & XML_XSD10_SCHEMAS_FINAL_DEFAULT_UNION) != 0)
            schema.flags ^= XML_XSD10_SCHEMAS_FINAL_DEFAULT_UNION;

        if ((schema.flags & XML_XSD10_SCHEMAS_BLOCK_DEFAULT_EXTENSION) != 0)
            schema.flags ^= XML_XSD10_SCHEMAS_BLOCK_DEFAULT_EXTENSION;
        if ((schema.flags & XML_XSD10_SCHEMAS_BLOCK_DEFAULT_RESTRICTION) != 0)
            schema.flags ^= XML_XSD10_SCHEMAS_BLOCK_DEFAULT_RESTRICTION;
        if ((schema.flags & XML_XSD10_SCHEMAS_BLOCK_DEFAULT_SUBSTITUTION) != 0)
            schema.flags ^= XML_XSD10_SCHEMAS_BLOCK_DEFAULT_SUBSTITUTION;
    }

    // xml_xsd10_parse_schema_element
    /** Port of xmlSchemaParseSchemaElement. */
    static int parseSchemaElement(Xsd10ParserCtxt ctxt, Xsd10Schema schema, XmlNode node)
    {
        XmlNode attr;
        String val;
        int res, oldErrs = ctxt.nberrors;
        int[] fl = new int[1];

        /*
        * Those flags should be moved to the parser context flags,
        * since they are not visible at the component level. I.e.
        * they are used if processing schema *documents* only.
        */
        res = pValAttrId(ctxt, node, "id");
        if (res == -1) return -1;

        exit:
        {
            /*
            * Since the version is of type xs:token, we won't bother to
            * check it.
            */
            attr = getPropNode(node, "targetNamespace");
            if (attr != null)
            {
                res = pValAttrNode(ctxt, null, attr,
                    getBuiltInType(XML_XSD10_SCHEMAS_ANYURI), null);
                if (res == -1) return -1;
                if (res != 0)
                {
                    ctxt.stop = XML_XSD10_SCHEMAP_S4S_ATTR_INVALID_VALUE;
                    break exit;
                }
            }
            attr = getPropNode(node, "elementFormDefault");
            if (attr != null)
            {
                val = getNodeContent(ctxt, attr);
                fl[0] = schema.flags;
                res = pValAttrFormDefault(val, fl,
                    XML_XSD10_SCHEMAS_QUALIF_ELEM);
                schema.flags = fl[0];
                if (res == -1) return -1;
                if (res != 0)
                {
                    pSimpleTypeErr(ctxt,
                        XML_XSD10_SCHEMAP_ELEMFORMDEFAULT_VALUE,
                        null, attr, null,
                        "(qualified | unqualified)", val, null, null, null);
                }
            }
            attr = getPropNode(node, "attributeFormDefault");
            if (attr != null)
            {
                val = getNodeContent(ctxt, attr);
                fl[0] = schema.flags;
                res = pValAttrFormDefault(val, fl,
                    XML_XSD10_SCHEMAS_QUALIF_ATTR);
                schema.flags = fl[0];
                if (res == -1) return -1;
                if (res != 0)
                {
                    pSimpleTypeErr(ctxt,
                        XML_XSD10_SCHEMAP_ATTRFORMDEFAULT_VALUE,
                        null, attr, null,
                        "(qualified | unqualified)", val, null, null, null);
                }
            }
            attr = getPropNode(node, "finalDefault");
            if (attr != null)
            {
                val = getNodeContent(ctxt, attr);
                fl[0] = schema.flags;
                res = pValAttrBlockFinal(val, fl, -1,
                    XML_XSD10_SCHEMAS_FINAL_DEFAULT_EXTENSION,
                    XML_XSD10_SCHEMAS_FINAL_DEFAULT_RESTRICTION,
                    -1,
                    XML_XSD10_SCHEMAS_FINAL_DEFAULT_LIST,
                    XML_XSD10_SCHEMAS_FINAL_DEFAULT_UNION);
                schema.flags = fl[0];
                if (res == -1) return -1;
                if (res != 0)
                {
                    pSimpleTypeErr(ctxt,
                        XML_XSD10_SCHEMAP_S4S_ATTR_INVALID_VALUE,
                        null, attr, null,
                        "(#all | List of (extension | restriction | list | union))",
                        val, null, null, null);
                }
            }
            attr = getPropNode(node, "blockDefault");
            if (attr != null)
            {
                val = getNodeContent(ctxt, attr);
                fl[0] = schema.flags;
                res = pValAttrBlockFinal(val, fl, -1,
                    XML_XSD10_SCHEMAS_BLOCK_DEFAULT_EXTENSION,
                    XML_XSD10_SCHEMAS_BLOCK_DEFAULT_RESTRICTION,
                    XML_XSD10_SCHEMAS_BLOCK_DEFAULT_SUBSTITUTION, -1, -1);
                schema.flags = fl[0];
                if (res == -1) return -1;
                if (res != 0)
                {
                    pSimpleTypeErr(ctxt,
                        XML_XSD10_SCHEMAP_S4S_ATTR_INVALID_VALUE,
                        null, attr, null,
                        "(#all | List of (extension | restriction | substitution))",
                        val, null, null, null);
                }
            }
        }

        /* exit: */
        if (oldErrs != ctxt.nberrors)
            res = ctxt.err;
        return res;
    }

    // xml_xsd10_parse_schema_top_level
    /** Port of xmlSchemaParseSchemaTopLevel. */
    static int parseSchemaTopLevel(Xsd10ParserCtxt ctxt, Xsd10Schema schema, XmlNode nodes)
    {
        XmlNode child;
        Xsd10Annot annot;
        int res = 0, oldErrs, tmpOldErrs;

        if ((ctxt == null) || (schema == null) || (nodes == null))
            return -1;

        oldErrs = ctxt.nberrors;
        child = nodes;
        exit:
        {
            while ((isSchema(child, "include") != 0) ||
                   (isSchema(child, "import") != 0) ||
                   (isSchema(child, "redefine") != 0) ||
                   (isSchema(child, "annotation") != 0))
            {
                if (isSchema(child, "annotation") != 0)
                {
                    annot = parseAnnotation(ctxt, child, 1);
                    if (schema.annot == null)
                        schema.annot = annot;
                    else
                        freeAnnot(annot);
                }
                else if (isSchema(child, "import") != 0)
                {
                    tmpOldErrs = ctxt.nberrors;
                    res = parseImport(ctxt, schema, child);
                    if (res == -1) return -1;
                    if (ctxt.stop != 0) break exit;
                    if (tmpOldErrs != ctxt.nberrors)
                        break exit;
                }
                else if (isSchema(child, "include") != 0)
                {
                    tmpOldErrs = ctxt.nberrors;
                    res = parseInclude(ctxt, schema, child);
                    if (res == -1) return -1;
                    if (ctxt.stop != 0) break exit;
                    if (tmpOldErrs != ctxt.nberrors)
                        break exit;
                }
                else if (isSchema(child, "redefine") != 0)
                {
                    tmpOldErrs = ctxt.nberrors;
                    res = parseRedefine(ctxt, schema, child);
                    if (res == -1) return -1;
                    if (ctxt.stop != 0) break exit;
                    if (tmpOldErrs != ctxt.nberrors)
                        break exit;
                }
                child = parseNext(child);
            }
            /*
            * URGENT TODO: Change the functions to return int results.
            * We need especially to catch internal errors.
            */
            while (child != null)
            {
                if (isSchema(child, "complexType") != 0)
                {
                    parseComplexType(ctxt, schema, child, 1);
                    child = parseNext(child);
                }
                else if (isSchema(child, "simpleType") != 0)
                {
                    parseSimpleType(ctxt, schema, child, 1);
                    child = parseNext(child);
                }
                else if (isSchema(child, "element") != 0)
                {
                    parseElement(ctxt, schema, child, null, 1);
                    child = parseNext(child);
                }
                else if (isSchema(child, "attribute") != 0)
                {
                    parseGlobalAttribute(ctxt, schema, child);
                    child = parseNext(child);
                }
                else if (isSchema(child, "attributeGroup") != 0)
                {
                    parseAttributeGroupDefinition(ctxt, schema, child);
                    child = parseNext(child);
                }
                else if (isSchema(child, "group") != 0)
                {
                    parseModelGroupDefinition(ctxt, schema, child);
                    child = parseNext(child);
                }
                else if (isSchema(child, "notation") != 0)
                {
                    parseNotation(ctxt, schema, child);
                    child = parseNext(child);
                }
                else
                {
                    pContentErr(ctxt,
                        XML_XSD10_SCHEMAP_S4S_ELEM_NOT_ALLOWED,
                        null, nodeParent(child), child,
                        null, "((include | import | redefine | annotation)*, " +
                        "(((simpleType | complexType | group | attributeGroup) " +
                        "| element | attribute | notation), annotation*)*)");
                    child = parseNext(child);
                }
                while (isSchema(child, "annotation") != 0)
                {
                    /*
                    * TODO: We should add all annotations.
                    */
                    annot = parseAnnotation(ctxt, child, 1);
                    if (schema.annot == null)
                        schema.annot = annot;
                    else
                        freeAnnot(annot);
                    child = parseNext(child);
                }
            }
        }
        /* exit: */
        ctxt.ctxtType = null;
        if (oldErrs != ctxt.nberrors)
            res = ctxt.err;
        return res;
    }

    // xml_xsd10_schema_relation_create
    /** Port of xmlSchemaSchemaRelationCreate. */
    static Xsd10SchemaRelation schemaRelationCreate()
    {
        return new Xsd10SchemaRelation();
    }

    // xml_xsd10_get_schema_bucket
    /** Port of xmlSchemaGetSchemaBucket. */
    static Xsd10Bucket getSchemaBucket(Xsd10ParserCtxt pctxt, String schemaLocation)
    {
        Xsd10Bucket cur;
        Xsd10ItemList list;

        list = pctxt.constructor.buckets;
        if ((list == null) || (list.nbItems == 0))
            return null;
        else
        {
            int i;
            for (i = 0; i < list.nbItems; i++)
            {
                cur = as(list.items[i], Xsd10Bucket.class);
                /* Pointer comparison! */
                if ((cur != null) && Objects.equals(cur.schemaLocation, schemaLocation))
                    return cur;
            }
        }
        return null;
    }

    // xml_xsd10_get_chameleon_schema_bucket
    /** Port of xmlSchemaGetChameleonSchemaBucket. */
    static Xsd10Bucket getChameleonSchemaBucket(Xsd10ParserCtxt pctxt, String schemaLocation, String targetNamespace)
    {
        Xsd10Bucket cur;
        Xsd10ItemList list;

        list = pctxt.constructor.buckets;
        if ((list == null) || (list.nbItems == 0))
            return null;
        else
        {
            int i;
            for (i = 0; i < list.nbItems; i++)
            {
                cur = as(list.items[i], Xsd10Bucket.class);
                /* Pointer comparison! */
                if ((cur != null) && (cur.origTargetNamespace == null) &&
                    Objects.equals(cur.schemaLocation, schemaLocation) &&
                    Objects.equals(cur.targetNamespace, targetNamespace))
                    return cur;
            }
        }
        return null;
    }

    // XSD10_IS_BAD_SCHEMA_DOC
    private static boolean isBadSchemaDoc(Xsd10Bucket b)
    {
        return (b.doc == null) && (b.schemaLocation != null);
    }

    // xml_xsd10_get_schema_bucket_by_tns
    /** Port of xmlSchemaGetSchemaBucketByTNS. */
    static Xsd10Bucket getSchemaBucketByTns(Xsd10ParserCtxt pctxt, String targetNamespace, int imported)
    {
        Xsd10Bucket cur;
        Xsd10ItemList list;

        list = pctxt.constructor.buckets;
        if ((list == null) || (list.nbItems == 0))
            return null;
        else
        {
            int i;
            for (i = 0; i < list.nbItems; i++)
            {
                cur = as(list.items[i], Xsd10Bucket.class);
                if ((cur != null) && (!isBadSchemaDoc(cur)) &&
                    Objects.equals(cur.origTargetNamespace, targetNamespace) &&
                    (((imported != 0) && (cur.imported != 0)) ||
                     ((imported == 0) && (cur.imported == 0))))
                    return cur;
            }
        }
        return null;
    }

    // xml_xsd10_parse_new_doc_with_context
    /** Port of xmlSchemaParseNewDocWithContext. */
    static int parseNewDocWithContext(Xsd10ParserCtxt pctxt, Xsd10Schema schema, Xsd10Bucket bucket)
    {
        int oldFlags;
        XmlDocument oldDoc;
        XmlNode node;
        int ret, oldErrs;
        Xsd10Bucket oldbucket = pctxt.constructor.bucket;

        /*
        * Save old values; reset the *main* schema.
        * URGENT TODO: This is not good; move the per-document information
        * to the parser. Get rid of passing the main schema to the
        * parsing functions.
        */
        oldFlags = schema.flags;
        oldDoc = schema.doc;
        if (schema.flags != 0)
            clearSchemaDefaults(schema);
        schema.doc = bucket.doc;
        pctxt.schema = schema;
        /*
        * Keep the current target namespace on the parser *not* on the
        * main schema.
        */
        pctxt.targetNamespace = bucket.targetNamespace;
        wxsConstructor(pctxt).bucket = bucket;

        if ((bucket.targetNamespace != null) &&
            (strEqual(bucket.targetNamespace, XML_XSD10_NS) != 0))
        {
            /*
            * We are parsing the schema for schemas!
            */
            pctxt.isS4S = 1;
        }
        /* Mark it as parsed, even if parsing fails. */
        bucket.parsed++;
        /* Compile the schema doc. */
        node = (bucket.doc == null) ? null : domRootElement(bucket.doc);
        exit:
        {
            ret = parseSchemaElement(pctxt, schema, node);
            if (ret != 0)
                break exit;
            /* An empty schema; just get out. */
            if (parseFirstChild(node) == null)
                break exit;
            oldErrs = pctxt.nberrors;
            ret = parseSchemaTopLevel(pctxt, schema, parseFirstChild(node));
            if (ret != 0)
                break exit;
            /*
            * TODO: Not nice, but I'm not 100% sure we will get always an error
            * as a result of the above functions; so better rely on pctxt->err
            * as well.
            */
            if ((ret == 0) && (oldErrs != pctxt.nberrors))
            {
                ret = pctxt.err;
                break exit;
            }
        }

        /* exit: */
        wxsConstructor(pctxt).bucket = oldbucket;
        /* Restore schema values. */
        schema.doc = oldDoc;
        schema.flags = oldFlags;
        return ret;
    }

    // xml_xsd10_parse_new_doc
    /** Port of xmlSchemaParseNewDoc. */
    static int parseNewDoc(Xsd10ParserCtxt pctxt, Xsd10Schema schema, Xsd10Bucket bucket)
    {
        Xsd10ParserCtxt newpctxt;
        int res;

        if (bucket == null)
            return 0;
        if (bucket.parsed != 0)
        {
            perrorInt(pctxt, "xmlSchemaParseNewDoc",
                "reparsing a schema doc");
            return -1;
        }
        if (bucket.doc == null)
        {
            perrorInt(pctxt, "xmlSchemaParseNewDoc",
                "parsing a schema doc, but there's no doc");
            return -1;
        }
        if (pctxt.constructor == null)
        {
            perrorInt(pctxt, "xmlSchemaParseNewDoc",
                "no constructor");
            return -1;
        }
        /* Create and init the temporary parser context. */
        newpctxt = newParserCtxtUseDict(bucket.schemaLocation, pctxt.dict);
        if (newpctxt == null)
            return -1;
        newpctxt.constructor = pctxt.constructor;
        /*
        * TODO: Can we avoid that the parser knows about the main schema?
        * It would be better if he knows about the current schema bucket
        * only.
        */
        newpctxt.schema = schema;
        setParserErrors(newpctxt, pctxt.error, pctxt.warning,
            pctxt.errCtxt);
        setParserStructuredErrors(newpctxt, pctxt.serror,
            pctxt.errCtxt);
        newpctxt.counter = pctxt.counter;
        /* Not in libxml2 (its temporary context loads with the default resource
           loader): the native loader replaces xmlResourceLoader, keep using it for
           the imports/includes of the nested documents. */
        newpctxt.resourceLoader = pctxt.resourceLoader;
        newpctxt.resourceCtxt = pctxt.resourceCtxt;

        res = parseNewDocWithContext(newpctxt, schema, bucket);

        /* Channel back errors and cleanup the temporary parser context. */
        if (res != 0)
            pctxt.err = res;
        pctxt.nberrors += newpctxt.nberrors;
        pctxt.counter = newpctxt.counter;
        newpctxt.constructor = null;
        /* Free the parser context. */
        freeParserCtxt(newpctxt);
        return res;
    }

    // xml_xsd10_schema_relation_add_child
    /** Port of xmlSchemaSchemaRelationAddChild. */
    static void schemaRelationAddChild(Xsd10Bucket bucket, Xsd10SchemaRelation rel)
    {
        Xsd10SchemaRelation cur = bucket.relations;

        if (cur == null)
        {
            bucket.relations = rel;
            return;
        }
        while (cur.next != null)
            cur = cur.next;
        cur.next = rel;
    }

    // xml_xsd10_build_absolute_uri
    /** Port of xmlSchemaBuildAbsoluteURI. */
    static String buildAbsoluteUri(Xsd10Dict dict, String location, XmlNode ctxtNode, String baseURI)
    {
        /*
        * Build an absolute location URI.
        */
        if (location != null)
        {
            String URI;

            if (ctxtNode != null)
            {
                String base;

                base = treeNodeGetBase(nodeDom(ctxtNode), ctxtNode);
                if (base == null)
                {
                    XmlDocument d = nodeDom(ctxtNode);
                    URI = XmlUri.build(location, (d == null) ? null : domUrl(d));
                }
                else
                {
                    URI = XmlUri.build(location, base);
                }
            }
            else if (baseURI != null)
            {
                URI = XmlUri.build(location, baseURI);
            }
            else
            {
                return location;
            }
            if (URI != null)
            {
                return dictLookup(dict, URI, -1);
            }
        }
        return null;
    }

    private static final int ASD_EXIT = 0;
    private static final int ASD_ERROR = 1;
    private static final int ASD_FAILURE = 2;

    // xml_xsd10_add_schema_doc
    /** Port of xmlSchemaAddSchemaDoc. Arrays: {@code bucket} is C {@code XmlXsd10Bucket **} (an out param: caller allocated array of length 1; a real C array: the array). */
    static int addSchemaDoc(Xsd10ParserCtxt pctxt, int type, String schemaLocation, XmlDocument schemaDoc, String schemaBuffer, int schemaBufferLen, XmlNode invokingNode, String sourceTargetNamespace, String importNamespace, Xsd10Bucket[] bucket)
    {
        String targetNamespace = null;
        Xsd10SchemaRelation relation = null;
        XmlDocument doc = null;
        int res = 0, err = 0, located = 0, preserveDoc = 0;
        Xsd10Bucket bkt = null;
        int outcome = ASD_EXIT;

        if (bucket != null)
            bucket[0] = null;

        switch (type)
        {
        case XML_XSD10_SCHEMA_SCHEMA_IMPORT:
        case XML_XSD10_SCHEMA_SCHEMA_MAIN:
            err = XML_XSD10_SCHEMAP_SRC_IMPORT;
            break;
        case XML_XSD10_SCHEMA_SCHEMA_INCLUDE:
            err = XML_XSD10_SCHEMAP_SRC_INCLUDE;
            break;
        case XML_XSD10_SCHEMA_SCHEMA_REDEFINE:
            err = XML_XSD10_SCHEMAP_SRC_REDEFINE;
            break;
        default:
            break;
        }

        done:
        {
            docLoad:
            {
                /* Special handling for the main schema:
                * skip the location and relation logic and just parse the doc.
                * We need just a bucket to be returned in this case.
                */
                if ((type == XML_XSD10_SCHEMA_SCHEMA_MAIN) || (wxsHasBuckets(pctxt) == 0))
                    break docLoad;

                /* Note that we expect the location to be an absolute URI. */
                if (schemaLocation != null)
                {
                    bkt = getSchemaBucket(pctxt, schemaLocation);
                    if ((bkt != null) &&
                        (pctxt.constructor.bucket == bkt))
                    {
                        /* Report self-imports/inclusions/redefinitions. */

                        customErr(pctxt, err,
                            invokingNode, null,
                            "The schema must not import/include/redefine itself",
                            null, null);
                        outcome = ASD_EXIT;
                        break done;
                    }
                }
                /*
                * Create a relation for the graph of schemas.
                */
                relation = schemaRelationCreate();
                if (relation == null)
                    return -1;
                schemaRelationAddChild(pctxt.constructor.bucket, relation);
                relation.type = type;

                /*
                * Save the namespace import information.
                */
                if (wxsIsBucketImpmain(type) != 0)
                {
                    relation.importNamespace = importNamespace;
                    if (schemaLocation == null)
                    {
                        /*
                        * No location; this is just an import of the namespace.
                        * Note that we don't assign a bucket to the relation
                        * in this case.
                        */
                        outcome = ASD_EXIT;
                        break done;
                    }
                    targetNamespace = importNamespace;
                }

                /* Did we already fetch the doc? */
                if (bkt != null)
                {
                    if ((wxsIsBucketImpmain(type) != 0) && (bkt.imported == 0))
                    {
                        /*
                        * We included/redefined and then try to import a schema,
                        * but the new location provided for import was different.
                        */
                        if (schemaLocation == null)
                            schemaLocation = "in_memory_buffer";
                        if (strEqual(schemaLocation, bkt.schemaLocation) == 0)
                        {
                            customErr(pctxt, err,
                                invokingNode, null,
                                "The schema document '%s' cannot be imported, since " +
                                "it was already included or redefined",
                                schemaLocation, null);
                            outcome = ASD_EXIT;
                            break done;
                        }
                    }
                    else if ((wxsIsBucketImpmain(type) == 0) && (bkt.imported != 0))
                    {
                        /*
                        * We imported and then try to include/redefine a schema,
                        * but the new location provided for the include/redefine
                        * was different.
                        */
                        if (schemaLocation == null)
                            schemaLocation = "in_memory_buffer";
                        if (strEqual(schemaLocation, bkt.schemaLocation) == 0)
                        {
                            customErr(pctxt, err,
                                invokingNode, null,
                                "The schema document '%s' cannot be included or " +
                                "redefined, since it was already imported",
                                schemaLocation, null);
                            outcome = ASD_EXIT;
                            break done;
                        }
                    }
                }

                if (wxsIsBucketImpmain(type) != 0)
                {
                    /*
                    * Given that the schemaLocation [attribute] is only a hint, it is open
                    * to applications to ignore all but the first <import> for a given
                    * namespace, regardless of the `actual value` of schemaLocation, but
                    * such a strategy risks missing useful information when new
                    * schemaLocations are offered.
                    *
                    * We will use the first <import> that comes with a location.
                    * Further <import>s *with* a location, will result in an error.
                    * TODO: Better would be to just report a warning here, but
                    * we'll try it this way until someone complains.
                    *
                    * Schema Document Location Strategy:
                    * 3 Based on the namespace name, identify an existing schema document,
                    * either as a resource which is an XML document or a <schema> element
                    * information item, in some local schema repository;
                    * 5 Attempt to resolve the namespace name to locate such a resource.
                    *
                    * NOTE: (3) and (5) are not supported.
                    */
                    if (bkt != null)
                    {
                        relation.bucket = bkt;
                        outcome = ASD_EXIT;
                        break done;
                    }
                    bkt = getSchemaBucketByTns(pctxt, importNamespace, 1);

                    if (bkt != null)
                    {
                        relation.bucket = bkt;
                        if (bkt.schemaLocation == null)
                        {
                            /* First given location of the schema; load the doc. */
                            bkt.schemaLocation = schemaLocation;
                        }
                        else
                        {
                            if (strEqual(schemaLocation, bkt.schemaLocation) == 0)
                            {
                                /*
                                * Additional location given; just skip it.
                                * URGENT TODO: We should report a warning here.
                                * res = XML_SCHEMAP_SRC_IMPORT;
                                */
                                if (schemaLocation == null)
                                    schemaLocation = "in_memory_buffer";

                                customWarning(pctxt,
                                    XML_XSD10_SCHEMAP_WARN_SKIP_SCHEMA,
                                    invokingNode, null,
                                    "Skipping import of schema located at '%s' for the " +
                                    "namespace '%s', since this namespace was already " +
                                    "imported with the schema located at '%s'",
                                    schemaLocation, importNamespace, bkt.schemaLocation);
                            }
                            outcome = ASD_EXIT;
                            break done;
                        }
                    }
                    /*
                    * No bucket + first location: load the doc and create a
                    * bucket.
                    */
                }
                else
                {
                    /* <include> and <redefine> */
                    if (bkt != null)
                    {

                        if ((bkt.origTargetNamespace == null) &&
                            !Objects.equals(bkt.targetNamespace, sourceTargetNamespace))
                        {
                            Xsd10Bucket chamel;

                            /*
                            * Chameleon include/redefine: skip loading only if it was
                            * already build for the targetNamespace of the including
                            * schema.
                            */
                            /*
                            * URGENT TODO: If the schema is a chameleon-include then copy
                            * the components into the including schema and modify the
                            * targetNamespace of those components, do nothing otherwise.
                            * NOTE: This is currently worked-around by compiling the
                            * chameleon for every distinct including targetNamespace; thus
                            * not performant at the moment.
                            * TODO: Check when the namespace in wildcards for chameleons
                            * needs to be converted: before we built wildcard intersections
                            * or after.
                            *   Answer: after!
                            */
                            chamel = getChameleonSchemaBucket(pctxt,
                                schemaLocation, sourceTargetNamespace);
                            if (chamel != null)
                            {
                                /* A fitting chameleon was already parsed; NOP. */
                                relation.bucket = chamel;
                                outcome = ASD_EXIT;
                                break done;
                            }
                            /*
                            * We need to parse the chameleon again for a different
                            * targetNamespace.
                            * CHAMELEON TODO: Optimize this by only parsing the
                            * chameleon once, and then copying the components to
                            * the new targetNamespace.
                            */
                            bkt = null;
                        }
                        else
                        {
                            relation.bucket = bkt;
                            outcome = ASD_EXIT;
                            break done;
                        }
                    }
                }
                if ((bkt != null) && (bkt.doc != null))
                {
                    perrorInt(pctxt, "xmlSchemaAddSchemaDoc",
                        "trying to load a schema doc, but a doc is already " +
                        "assigned to the schema bucket");
                    outcome = ASD_FAILURE;
                    break done;
                }
            }

            /* doc_load: */
            /*
            * Load the document.
            */
            if (schemaDoc != null)
            {
                doc = schemaDoc;
                /* Don' free this one, since it was provided by the caller. */
                preserveDoc = 1;
                /* TODO: Does the context or the doc hold the location? */
                if (domUrl(schemaDoc) != null)
                    schemaLocation = dictLookup(pctxt.dict, domUrl(schemaDoc), -1);
                else
                    schemaLocation = "in_memory_buffer";
            }
            else if ((schemaLocation != null) || (schemaBuffer != null))
            {
                /* The loading via xmlNewParserCtxt/xmlCtxtReadFile/xmlCtxtReadMemory (with the
                   structured error handler and the resource loader of pctxt) is done by
                   xml_xsd10_p_read_file/xml_xsd10_p_read_memory, notLocated replaces the check
                   of the last error for the XML_FROM_IO domain below. */
                int[] notLocated = { 0 };

                if (schemaLocation != null)
                {
                    /* Parse from file. */
                    doc = pReadFile(pctxt, schemaLocation, notLocated);
                }
                else if (schemaBuffer != null)
                {
                    /* Parse from memory buffer. */
                    doc = pReadMemory(pctxt, schemaBuffer, schemaBufferLen,
                        notLocated);
                    schemaLocation = "in_memory_buffer";
                    if (doc != null)
                        domSetUrl(doc, schemaLocation);
                }
                /*
                * For <import>:
                * 2.1 The referent is (a fragment of) a resource which is an
                * XML document (see clause 1.1), which in turn corresponds to
                * a <schema> element information item in a well-formed information
                * set, which in turn corresponds to a valid schema.
                * TODO: (2.1) fragments of XML documents are not supported.
                *
                * 2.2 The referent is a <schema> element information item in
                * a well-formed information set, which in turn corresponds
                * to a valid schema.
                * TODO: (2.2) is not supported.
                */
                if (doc == null)
                {
                    /*
                    * Check if this a parser error, or if the document could
                    * just not be located.
                    * TODO: Try to find specific error codes to react only on
                    * localisation failures.
                    */
                    if (notLocated[0] == 0)
                    {
                        /*
                        * We assume a parser error here.
                        */
                        located = 1;
                        /* TODO: Error code ?? */
                        res = XML_XSD10_SCHEMAP_SRC_IMPORT_2_1;
                        customErr(pctxt, res,
                            invokingNode, null,
                            "Failed to parse the XML resource '%s'",
                            schemaLocation, null);
                    }
                }
                if ((doc == null) && (located != 0))
                {
                    outcome = ASD_ERROR;
                    break done;
                }
            }
            else
            {
                pErr(pctxt, null,
                    XML_XSD10_SCHEMAP_NOTHING_TO_PARSE,
                    "No information for parsing was provided with the " +
                    "given schema parser context.\n",
                    null, null);
                outcome = ASD_FAILURE;
                break done;
            }
            /*
            * Preprocess the document.
            */
            if (doc != null)
            {
                XmlNode docElem;

                located = 1;
                docElem = domRootElement(doc);
                if (docElem == null)
                {
                    customErr(pctxt, XML_XSD10_SCHEMAP_NOROOT,
                        invokingNode, null,
                        "The document '%s' has no document element",
                        schemaLocation, null);
                    outcome = ASD_ERROR;
                    break done;
                }
                /*
                * Remove all the blank text nodes.
                */
                cleanupDoc(pctxt, docElem);
                /*
                * Check the schema's top level element.
                */
                if (isSchema(docElem, "schema") == 0)
                {
                    customErr(pctxt, XML_XSD10_SCHEMAP_NOT_SCHEMA,
                        invokingNode, null,
                        "The XML document '%s' is not a schema document",
                        schemaLocation, null);
                    outcome = ASD_ERROR;
                    break done;
                }
                /*
                * Note that we don't apply a type check for the
                * targetNamespace value here.
                */
                targetNamespace = getProp(pctxt, docElem,
                    "targetNamespace");
            }

            /* after_doc_loading: */
            if ((bkt == null) && (located != 0))
            {
                /* Only create a bucket if the schema was located. */
                bkt = bucketCreate(pctxt, type,
                    targetNamespace);
                if (bkt == null)
                {
                    outcome = ASD_FAILURE;
                    break done;
                }
            }
            if (bkt != null)
            {
                bkt.schemaLocation = schemaLocation;
                bkt.located = located;
                if (doc != null)
                {
                    bkt.doc = doc;
                    bkt.targetNamespace = targetNamespace;
                    bkt.origTargetNamespace = targetNamespace;
                    if (preserveDoc != 0)
                        bkt.preserveDoc = 1;
                }
                if (wxsIsBucketImpmain(type) != 0)
                    bkt.imported++;
                /*
                * Add it to the graph of schemas.
                */
                if (relation != null)
                    relation.bucket = bkt;
            }
            outcome = ASD_EXIT;
        }

        switch (outcome)
        {
        case ASD_EXIT:
        default:
            /*
            * Return the bucket explicitly; this is needed for the
            * main schema.
            */
            if (bucket != null)
                bucket[0] = bkt;
            return 0;
        case ASD_ERROR:
            if ((doc != null) && (preserveDoc == 0))
            {
                /* xml_dom_free(doc): the java dom is garbage collected. */
                if (bkt != null)
                    bkt.doc = null;
            }
            return pctxt.err;
        case ASD_FAILURE:
            if ((doc != null) && (preserveDoc == 0))
            {
                /* xml_dom_free(doc): the java dom is garbage collected. */
                if (bkt != null)
                    bkt.doc = null;
            }
            return -1;
        }
    }

    // xml_xsd10_parse_import
    /** Port of xmlSchemaParseImport. */
    static int parseImport(Xsd10ParserCtxt pctxt, Xsd10Schema schema, XmlNode node)
    {
        XmlNode child;
        String[] namespaceName = { null }, schemaLocation = { null };
        String thisTargetNamespace;
        XmlNode attr;
        int ret;
        Xsd10Bucket[] bucket = { null };

        if ((pctxt == null) || (schema == null) || (node == null))
            return -1;

        /*
        * Check for illegal attributes.
        */
        attr = parseFirstAttr(node);
        while (attr != null)
        {
            if (nodeNsUri(attr) == null)
            {
                if ((strEqual(nodeName(attr), "id") == 0) &&
                    (strEqual(nodeName(attr), "namespace") == 0) &&
                    (strEqual(nodeName(attr), "schemaLocation") == 0))
                {
                    pIllegalAttrErr(pctxt,
                        XML_XSD10_SCHEMAP_S4S_ATTR_NOT_ALLOWED, null, attr);
                }
            }
            else if (strEqual(nodeNsUri(attr), XML_XSD10_NS) != 0)
            {
                pIllegalAttrErr(pctxt,
                    XML_XSD10_SCHEMAP_S4S_ATTR_NOT_ALLOWED, null, attr);
            }
            attr = nodeNext(attr);
        }
        /*
        * Extract and validate attributes.
        */
        if (pValAttr(pctxt, null, node,
            "namespace", getBuiltInType(XML_XSD10_SCHEMAS_ANYURI),
            namespaceName) != 0)
        {
            pSimpleTypeErr(pctxt,
                XML_XSD10_SCHEMAP_S4S_ATTR_INVALID_VALUE,
                null, node,
                getBuiltInType(XML_XSD10_SCHEMAS_ANYURI),
                null, namespaceName[0], null, null, null);
            return pctxt.err;
        }

        if (pValAttr(pctxt, null, node,
            "schemaLocation", getBuiltInType(XML_XSD10_SCHEMAS_ANYURI),
            schemaLocation) != 0)
        {
            pSimpleTypeErr(pctxt,
                XML_XSD10_SCHEMAP_S4S_ATTR_INVALID_VALUE,
                null, node,
                getBuiltInType(XML_XSD10_SCHEMAS_ANYURI),
                null, schemaLocation[0], null, null, null);
            return pctxt.err;
        }
        /*
        * And now for the children...
        */
        child = parseFirstChild(node);
        if (isSchema(child, "annotation") != 0)
        {
            /*
             * the annotation here is simply discarded ...
             * TODO: really?
             */
            child = parseNext(child);
        }
        if (child != null)
        {
            pContentErr(pctxt,
                XML_XSD10_SCHEMAP_S4S_ELEM_NOT_ALLOWED,
                null, node, child, null,
                "(annotation?)");
        }
        /*
        * Apply additional constraints.
        *
        * Note that it is important to use the original @targetNamespace
        * (or none at all), to rule out imports of schemas _with_ a
        * @targetNamespace if the importing schema is a chameleon schema
        * (with no @targetNamespace).
        */
        thisTargetNamespace = wxsBucket(pctxt).origTargetNamespace;
        if (namespaceName[0] != null)
        {
            /*
            * 1.1 If the namespace [attribute] is present, then its `actual value`
            * must not match the `actual value` of the enclosing <schema>'s
            * targetNamespace [attribute].
            */
            if (strEqual(thisTargetNamespace, namespaceName[0]) != 0)
            {
                pCustomErr(pctxt,
                    XML_XSD10_SCHEMAP_SRC_IMPORT_1_1,
                    null, node,
                    "The value of the attribute 'namespace' must not match " +
                    "the target namespace '%s' of the importing schema",
                    thisTargetNamespace);
                return pctxt.err;
            }
        }
        else
        {
            /*
            * 1.2 If the namespace [attribute] is not present, then the enclosing
            * <schema> must have a targetNamespace [attribute].
            */
            if (thisTargetNamespace == null)
            {
                pCustomErr(pctxt,
                    XML_XSD10_SCHEMAP_SRC_IMPORT_1_2,
                    null, node,
                    "The attribute 'namespace' must be existent if " +
                    "the importing schema has no target namespace",
                    null);
                return pctxt.err;
            }
        }
        /*
        * Locate and acquire the schema document.
        */
        if (schemaLocation[0] != null)
            schemaLocation[0] = buildAbsoluteUri(pctxt.dict,
                schemaLocation[0], node, null);
        ret = addSchemaDoc(pctxt, XML_XSD10_SCHEMA_SCHEMA_IMPORT,
            schemaLocation[0], null, null, 0, node, thisTargetNamespace,
            namespaceName[0], bucket);

        if (ret != 0)
            return ret;

        /*
        * For <import>: "It is *not* an error for the application
        * schema reference strategy to fail."
        * So just don't parse if no schema document was found.
        * Note that we will get no bucket if the schema could not be
        * located or if there was no schemaLocation.
        */
        if ((bucket[0] == null) && (schemaLocation[0] != null))
        {
            customWarning(pctxt,
                XML_XSD10_SCHEMAP_WARN_UNLOCATED_SCHEMA,
                node, null,
                "Failed to locate a schema at location '%s'. " +
                "Skipping the import", schemaLocation[0], null, null);
        }

        if ((bucket[0] != null) && (canParseSchema(bucket[0]) != 0))
        {
            ret = parseNewDoc(pctxt, schema, bucket[0]);
        }

        return ret;
    }

    // xml_xsd10_parse_include_or_redefine_attrs
    /** Port of xmlSchemaParseIncludeOrRedefineAttrs. Arrays: {@code schemaLocation} is C {@code char **} (an out param: caller allocated array of length 1; a real C array: the array). */
    static int parseIncludeOrRedefineAttrs(Xsd10ParserCtxt pctxt, Xsd10Schema schema, XmlNode node, String[] schemaLocation, int type)
    {
        XmlNode attr;

        if ((pctxt == null) || (schema == null) || (node == null) ||
            (schemaLocation == null))
            return -1;

        schemaLocation[0] = null;
        /*
        * Check for illegal attributes.
        * Applies for both <include> and <redefine>.
        */
        attr = parseFirstAttr(node);
        while (attr != null)
        {
            if (nodeNsUri(attr) == null)
            {
                if ((strEqual(nodeName(attr), "id") == 0) &&
                    (strEqual(nodeName(attr), "schemaLocation") == 0))
                {
                    pIllegalAttrErr(pctxt,
                        XML_XSD10_SCHEMAP_S4S_ATTR_NOT_ALLOWED, null, attr);
                }
            }
            else if (strEqual(nodeNsUri(attr), XML_XSD10_NS) != 0)
            {
                pIllegalAttrErr(pctxt,
                    XML_XSD10_SCHEMAP_S4S_ATTR_NOT_ALLOWED, null, attr);
            }
            attr = nodeNext(attr);
        }
        pValAttrId(pctxt, node, "id");
        /*
        * Preliminary step, extract the URI-Reference and make an URI
        * from the base.
        */
        /*
        * Attribute "schemaLocation" is mandatory.
        */
        attr = getPropNode(node, "schemaLocation");
        if (attr != null)
        {
            String base;
            String uri;

            if (pValAttrNode(pctxt, null, attr,
                getBuiltInType(XML_XSD10_SCHEMAS_ANYURI),
                schemaLocation) != 0)
                return pctxt.err;
            base = treeNodeGetBase(nodeDom(node), node);
            if (base == null)
            {
                XmlDocument d = nodeDom(node);
                uri = XmlUri.build(schemaLocation[0], (d == null) ? null : domUrl(d));
            }
            else
            {
                uri = XmlUri.build(schemaLocation[0], base);
            }
            if (uri == null)
            {
                perrorInt(pctxt, "xmlSchemaParseIncludeOrRedefine",
                    "could not build an URI from the schemaLocation");
                return -1;
            }
            schemaLocation[0] = dictLookup(pctxt.dict, uri, -1);
        }
        else
        {
            pMissingAttrErr(pctxt,
                XML_XSD10_SCHEMAP_S4S_ATTR_MISSING,
                null, node, "schemaLocation", null);
            return pctxt.err;
        }
        /*
        * Report self-inclusion and self-redefinition.
        */
        if (strEqual(schemaLocation[0], pctxt.URL) != 0)
        {
            if (type == XML_XSD10_SCHEMA_SCHEMA_REDEFINE)
            {
                pCustomErr(pctxt,
                    XML_XSD10_SCHEMAP_SRC_REDEFINE,
                    null, node,
                    "The schema document '%s' cannot redefine itself.",
                    schemaLocation[0]);
            }
            else
            {
                pCustomErr(pctxt,
                    XML_XSD10_SCHEMAP_SRC_INCLUDE,
                    null, node,
                    "The schema document '%s' cannot include itself.",
                    schemaLocation[0]);
            }
            return pctxt.err;
        }

        return 0;
    }

    // xml_xsd10_parse_include_or_redefine
    /** Port of xmlSchemaParseIncludeOrRedefine. */
    static int parseIncludeOrRedefine(Xsd10ParserCtxt pctxt, Xsd10Schema schema, XmlNode node, int type)
    {
        XmlNode child;
        String[] schemaLocation = { null };
        int res; /* hasRedefinitions = 0 */
        int isChameleon = 0, wasChameleon = 0;
        Xsd10Bucket[] bucketOut = { null };
        Xsd10Bucket bucket;

        if ((pctxt == null) || (schema == null) || (node == null))
            return -1;

        /*
        * Parse attributes. Note that the returned schemaLocation will
        * be already converted to an absolute URI.
        */
        res = parseIncludeOrRedefineAttrs(pctxt, schema,
            node, schemaLocation, type);
        if (res != 0)
            return res;
        /*
        * Load and add the schema document.
        */
        res = addSchemaDoc(pctxt, type, schemaLocation[0], null,
            null, 0, node, pctxt.targetNamespace, null, bucketOut);
        if (res != 0)
            return res;
        bucket = bucketOut[0];
        /*
        * If we get no schema bucket back, then this means that the schema
        * document could not be located or was broken XML or was not
        * a schema document.
        */
        if ((bucket == null) || (bucket.doc == null))
        {
            if (type == XML_XSD10_SCHEMA_SCHEMA_INCLUDE)
            {
                /*
                * WARNING for <include>:
                * We will raise an error if the schema cannot be located
                * for inclusions, since the that was the feedback from the
                * schema people. I.e. the following spec piece will *not* be
                * satisfied:
                * SPEC src-include: "It is not an error for the `actual value` of the
                * schemaLocation [attribute] to fail to resolve it all, in which
                * case no corresponding inclusion is performed.
                * So do we need a warning report here?"
                */
                res = XML_XSD10_SCHEMAP_SRC_INCLUDE;
                customErr(pctxt, res,
                    node, null,
                    "Failed to load the document '%s' for inclusion",
                    schemaLocation[0], null);
            }
            else
            {
                /*
                * NOTE: This was changed to raise an error even if no redefinitions
                * are specified.
                *
                * SPEC src-redefine (1)
                * "If there are any element information items among the [children]
                * other than <annotation> then the `actual value` of the
                * schemaLocation [attribute] must successfully resolve."
                * TODO: Ask the WG if a the location has always to resolve
                * here as well!
                */
                res = XML_XSD10_SCHEMAP_SRC_REDEFINE;
                customErr(pctxt, res,
                    node, null,
                    "Failed to load the document '%s' for redefinition",
                    schemaLocation[0], null);
            }
        }
        else
        {
            /*
            * Check targetNamespace sanity before parsing the new schema.
            * TODO: Note that we won't check further content if the
            * targetNamespace was bad.
            */
            if (bucket.origTargetNamespace != null)
            {
                /*
                * SPEC src-include (2.1)
                * "SII has a targetNamespace [attribute], and its `actual
                * value` is identical to the `actual value` of the targetNamespace
                * [attribute] of SII' (which must have such an [attribute])."
                */
                if (pctxt.targetNamespace == null)
                {
                    customErr(pctxt,
                        XML_XSD10_SCHEMAP_SRC_INCLUDE,
                        node, null,
                        "The target namespace of the included/redefined schema " +
                        "'%s' has to be absent, since the including/redefining " +
                        "schema has no target namespace",
                        schemaLocation[0], null);
                    return pctxt.err;
                }
                else if (strEqual(bucket.origTargetNamespace,
                    pctxt.targetNamespace) == 0)
                {
                    /* TODO: Change error function. */
                    pCustomErrExt(pctxt,
                        XML_XSD10_SCHEMAP_SRC_INCLUDE,
                        null, node,
                        "The target namespace '%s' of the included/redefined " +
                        "schema '%s' differs from '%s' of the " +
                        "including/redefining schema",
                        bucket.origTargetNamespace, schemaLocation[0],
                        pctxt.targetNamespace);
                    return pctxt.err;
                }
            }
            else if (pctxt.targetNamespace != null)
            {
                /*
                * Chameleons: the original target namespace will
                * differ from the resulting namespace.
                */
                isChameleon = 1;
                bucket.targetNamespace = pctxt.targetNamespace;
            }
        }
        /*
        * Parse the schema.
        */
        if ((bucket != null) && (bucket.parsed == 0) && (bucket.doc != null))
        {
            if (isChameleon != 0)
            {
                /* TODO: Get rid of this flag on the schema itself. */
                if ((schema.flags & XML_XSD10_SCHEMAS_INCLUDING_CONVERT_NS) == 0)
                {
                    schema.flags |= XML_XSD10_SCHEMAS_INCLUDING_CONVERT_NS;
                }
                else
                    wasChameleon = 1;
            }
            parseNewDoc(pctxt, schema, bucket);
            /* Restore chameleon flag. */
            if ((isChameleon != 0) && (wasChameleon == 0))
                schema.flags ^= XML_XSD10_SCHEMAS_INCLUDING_CONVERT_NS;
        }
        /*
        * And now for the children...
        */
        child = parseFirstChild(node);
        if (type == XML_XSD10_SCHEMA_SCHEMA_REDEFINE)
        {
            /*
            * Parse (simpleType | complexType | group | attributeGroup))*
            */
            pctxt.redefined = bucket;
            /*
            * How to proceed if the redefined schema was not located?
            */
            pctxt.isRedefine = 1;
            while ((isSchema(child, "annotation") != 0) ||
                   (isSchema(child, "simpleType") != 0) ||
                   (isSchema(child, "complexType") != 0) ||
                   (isSchema(child, "group") != 0) ||
                   (isSchema(child, "attributeGroup") != 0))
            {
                if (isSchema(child, "annotation") != 0)
                {
                    /*
                    * TODO: discard or not?
                    */
                }
                else if (isSchema(child, "simpleType") != 0)
                {
                    parseSimpleType(pctxt, schema, child, 1);
                }
                else if (isSchema(child, "complexType") != 0)
                {
                    parseComplexType(pctxt, schema, child, 1);
                    /* hasRedefinitions = 1; */
                }
                else if (isSchema(child, "group") != 0)
                {
                    /* hasRedefinitions = 1; */
                    parseModelGroupDefinition(pctxt,
                        schema, child);
                }
                else if (isSchema(child, "attributeGroup") != 0)
                {
                    /* hasRedefinitions = 1; */
                    parseAttributeGroupDefinition(pctxt, schema,
                        child);
                }
                child = parseNext(child);
            }
            pctxt.redefined = null;
            pctxt.isRedefine = 0;
        }
        else
        {
            if (isSchema(child, "annotation") != 0)
            {
                /*
                * TODO: discard or not?
                */
                child = parseNext(child);
            }
        }
        if (child != null)
        {
            res = XML_XSD10_SCHEMAP_S4S_ELEM_NOT_ALLOWED;
            if (type == XML_XSD10_SCHEMA_SCHEMA_REDEFINE)
            {
                pContentErr(pctxt, res,
                    null, node, child, null,
                    "(annotation | (simpleType | complexType | group | attributeGroup))*");
            }
            else
            {
                pContentErr(pctxt, res,
                    null, node, child, null,
                    "(annotation?)");
            }
        }
        return res;
    }

    // xml_xsd10_parse_redefine
    /** Port of xmlSchemaParseRedefine. */
    static int parseRedefine(Xsd10ParserCtxt pctxt, Xsd10Schema schema, XmlNode node)
    {
        int res;

        res = parseIncludeOrRedefine(pctxt, schema, node,
            XML_XSD10_SCHEMA_SCHEMA_REDEFINE);
        if (res != 0)
            return res;
        return 0;
    }

    // xml_xsd10_parse_include
    /** Port of xmlSchemaParseInclude. */
    static int parseInclude(Xsd10ParserCtxt pctxt, Xsd10Schema schema, XmlNode node)
    {
        int res;

        res = parseIncludeOrRedefine(pctxt, schema, node,
            XML_XSD10_SCHEMA_SCHEMA_INCLUDE);
        if (res != 0)
            return res;
        return 0;
    }

    // xml_xsd10_parse_model_group
    /** Port of xmlSchemaParseModelGroup. */
    static Xsd10TreeItem parseModelGroup(Xsd10ParserCtxt ctxt, Xsd10Schema schema, XmlNode node, int type, int withParticle)
    {
        Xsd10ModelGroup item;
        Xsd10Particle particle = null;
        XmlNode child;
        XmlNode attr;
        int min = 1, max = 1, hasRefs = 0;
        int[] isElemRef = { 0 };

        if ((ctxt == null) || (schema == null) || (node == null))
            return null;
        /*
        * Create a model group with the given compositor.
        */
        item = addModelGroup(ctxt, schema, type, node);
        if (item == null)
            return null;

        if (withParticle != 0)
        {
            if (type == XML_XSD10_SCHEMA_TYPE_ALL)
            {
                min = getMinOccurs(ctxt, node, 0, 1, 1, "(0 | 1)");
                max = getMaxOccurs(ctxt, node, 1, 1, 1, "1");
            }
            else
            {
                /* choice + sequence */
                min = getMinOccurs(ctxt, node, 0, -1, 1, "xs:nonNegativeInteger");
                max = getMaxOccurs(ctxt, node, 0, XML_XSD10_UNBOUNDED, 1,
                    "(xs:nonNegativeInteger | unbounded)");
            }
            pCheckParticleCorrect2(ctxt, null, node, min, max);
            /*
            * Create a particle
            */
            particle = addParticle(ctxt, node, min, max);
            if (particle == null)
                return null;
            particle.children = item;
            /*
            * Check for illegal attributes.
            */
            attr = parseFirstAttr(node);
            while (attr != null)
            {
                if (nodeNsUri(attr) == null)
                {
                    if ((strEqual(nodeName(attr), "id") == 0) &&
                        (strEqual(nodeName(attr), "maxOccurs") == 0) &&
                        (strEqual(nodeName(attr), "minOccurs") == 0))
                    {
                        pIllegalAttrErr(ctxt,
                            XML_XSD10_SCHEMAP_S4S_ATTR_NOT_ALLOWED, null, attr);
                    }
                }
                else if (strEqual(nodeNsUri(attr), XML_XSD10_NS) != 0)
                {
                    pIllegalAttrErr(ctxt,
                        XML_XSD10_SCHEMAP_S4S_ATTR_NOT_ALLOWED, null, attr);
                }
                attr = nodeNext(attr);
            }
        }
        else
        {
            /*
            * Check for illegal attributes.
            */
            attr = parseFirstAttr(node);
            while (attr != null)
            {
                if (nodeNsUri(attr) == null)
                {
                    if (strEqual(nodeName(attr), "id") == 0)
                    {
                        pIllegalAttrErr(ctxt,
                            XML_XSD10_SCHEMAP_S4S_ATTR_NOT_ALLOWED, null, attr);
                    }
                }
                else if (strEqual(nodeNsUri(attr), XML_XSD10_NS) != 0)
                {
                    pIllegalAttrErr(ctxt,
                        XML_XSD10_SCHEMAP_S4S_ATTR_NOT_ALLOWED, null, attr);
                }
                attr = nodeNext(attr);
            }
        }

        /*
        * Extract and validate attributes.
        */
        pValAttrId(ctxt, node, "id");
        /*
        * And now for the children...
        */
        child = parseFirstChild(node);
        if (isSchema(child, "annotation") != 0)
        {
            item.annot = parseAnnotation(ctxt, child, 1);
            child = parseNext(child);
        }
        if (type == XML_XSD10_SCHEMA_TYPE_ALL)
        {
            Xsd10Particle part, last = null;

            while (isSchema(child, "element") != 0)
            {
                part = as(parseElement(ctxt, schema, child, isElemRef, 0), Xsd10Particle.class);
                /*
                * SPEC cos-all-limited (2)
                * "The {max occurs} of all the particles in the {particles}
                * of the ('all') group must be 0 or 1.
                */
                if (part != null)
                {
                    if (isElemRef[0] != 0)
                        hasRefs++;
                    if (part.minOccurs > 1)
                    {
                        pCustomErr(ctxt,
                            XML_XSD10_SCHEMAP_COS_ALL_LIMITED,
                            null, child,
                            "Invalid value for minOccurs (must be 0 or 1)",
                            null);
                        /* Reset to 1. */
                        part.minOccurs = 1;
                    }
                    if (part.maxOccurs > 1)
                    {
                        pCustomErr(ctxt,
                            XML_XSD10_SCHEMAP_COS_ALL_LIMITED,
                            null, child,
                            "Invalid value for maxOccurs (must be 0 or 1)",
                            null);
                        /* Reset to 1. */
                        part.maxOccurs = 1;
                    }
                    if (last == null)
                        item.children = part;
                    else
                        last.next = part;
                    last = part;
                }
                child = parseNext(child);
            }
            if (child != null)
            {
                pContentErr(ctxt,
                    XML_XSD10_SCHEMAP_S4S_ELEM_NOT_ALLOWED,
                    null, node, child, null,
                    "(annotation?, (annotation?, element*)");
            }
        }
        else
        {
            /* choice + sequence */
            Xsd10TreeItem part = null, last = null;

            while ((isSchema(child, "element") != 0) ||
                   (isSchema(child, "group") != 0) ||
                   (isSchema(child, "any") != 0) ||
                   (isSchema(child, "choice") != 0) ||
                   (isSchema(child, "sequence") != 0))
            {

                if (isSchema(child, "element") != 0)
                {
                    part = as(parseElement(ctxt, schema, child, isElemRef, 0), Xsd10TreeItem.class);
                    if ((part != null) && (isElemRef[0] != 0))
                        hasRefs++;
                }
                else if (isSchema(child, "group") != 0)
                {
                    part = parseModelGroupDefRef(ctxt, schema, child);
                    if (part != null)
                        hasRefs++;
                    /*
                    * Handle redefinitions.
                    */
                    if ((ctxt.isRedefine != 0) && (ctxt.redef != null) &&
                        (ctxt.redef.item != null) &&
                        (ctxt.redef.item.type == XML_XSD10_SCHEMA_TYPE_GROUP) &&
                        (part != null) && (part.children instanceof Xsd10QNameRef))
                    {
                        if (Objects.equals(getQNameRefName(part.children), ctxt.redef.refName) &&
                            Objects.equals(getQNameRefTargetNs(part.children), ctxt.redef.refTargetNs))
                        {
                            /*
                            * SPEC src-redefine:
                            * (6.1) "If it has a <group> among its contents at
                            * some level the `actual value` of whose ref
                            * [attribute] is the same as the `actual value` of
                            * its own name attribute plus target namespace, then
                            * all of the following must be true:"
                            * (6.1.1) "It must have exactly one such group."
                            */
                            if (ctxt.redefCounter != 0)
                            {
                                String[] str = { null };

                                customErr(ctxt,
                                    XML_XSD10_SCHEMAP_SRC_REDEFINE, child, null,
                                    "The redefining model group definition " +
                                    "'%s' must not contain more than one " +
                                    "reference to the redefined definition",
                                    formatQName(str,
                                        ctxt.redef.refTargetNs,
                                        ctxt.redef.refName),
                                    null);
                                part = null;
                            }
                            else if ((wxsParticle(part).minOccurs != 1) ||
                                     (wxsParticle(part).maxOccurs != 1))
                            {
                                String[] str = { null };
                                /*
                                * SPEC src-redefine:
                                * (6.1.2) "The `actual value` of both that
                                * group's minOccurs and maxOccurs [attribute]
                                * must be 1 (or `absent`).
                                */
                                customErr(ctxt,
                                    XML_XSD10_SCHEMAP_SRC_REDEFINE, child, null,
                                    "The redefining model group definition " +
                                    "'%s' must not contain a reference to the " +
                                    "redefined definition with a " +
                                    "maxOccurs/minOccurs other than 1",
                                    formatQName(str,
                                        ctxt.redef.refTargetNs,
                                        ctxt.redef.refName),
                                    null);
                                part = null;
                            }
                            ctxt.redef.reference = part;
                            ctxt.redefCounter++;
                        }
                    }
                }
                else if (isSchema(child, "any") != 0)
                {
                    part = parseAny(ctxt, schema, child);
                }
                else if (isSchema(child, "choice") != 0)
                {
                    part = parseModelGroup(ctxt, schema, child,
                        XML_XSD10_SCHEMA_TYPE_CHOICE, 1);
                }
                else if (isSchema(child, "sequence") != 0)
                {
                    part = parseModelGroup(ctxt, schema, child,
                        XML_XSD10_SCHEMA_TYPE_SEQUENCE, 1);
                }
                if (part != null)
                {
                    if (last == null)
                        item.children = part;
                    else
                        last.next = part;
                    last = part;
                }
                child = parseNext(child);
            }
            if (child != null)
            {
                pContentErr(ctxt,
                    XML_XSD10_SCHEMAP_S4S_ELEM_NOT_ALLOWED,
                    null, node, child, null,
                    "(annotation?, (element | group | choice | sequence | any)*)");
            }
        }
        if ((max == 0) && (min == 0))
            return null;
        if (hasRefs != 0)
        {
            /*
            * We need to resolve references.
            */
            wxsAddPending(ctxt, item);
        }
        if (withParticle != 0)
            return particle;
        else
            return item;
    }

    // xml_xsd10_parse_restriction
    /** Port of xmlSchemaParseRestriction. */
    static Xsd10Type parseRestriction(Xsd10ParserCtxt ctxt, Xsd10Schema schema, XmlNode node, int parentType)
    {
        Xsd10Type type;
        XmlNode child;
        XmlNode attr;

        if ((ctxt == null) || (schema == null) || (node == null))
            return null;
        /* Not a component, don't create it. */
        type = ctxt.ctxtType;
        if (type == null)
            return null;
        type.flags |= XML_XSD10_SCHEMAS_TYPE_DERIVATION_METHOD_RESTRICTION;

        /*
        * Check for illegal attributes.
        */
        attr = parseFirstAttr(node);
        while (attr != null)
        {
            if (nodeNsUri(attr) == null)
            {
                if ((strEqual(nodeName(attr), "id") == 0) &&
                    (strEqual(nodeName(attr), "base") == 0))
                {
                    pIllegalAttrErr(ctxt,
                        XML_XSD10_SCHEMAP_S4S_ATTR_NOT_ALLOWED, null, attr);
                }
            }
            else if (strEqual(nodeNsUri(attr), XML_XSD10_NS) != 0)
            {
                pIllegalAttrErr(ctxt,
                    XML_XSD10_SCHEMAP_S4S_ATTR_NOT_ALLOWED, null, attr);
            }
            attr = nodeNext(attr);
        }
        /*
        * Extract and validate attributes.
        */
        pValAttrId(ctxt, node, "id");
        /*
        * Attribute
        */
        /*
        * Extract the base type. The "base" attribute is mandatory if inside
        * a complex type or if redefining.
        *
        * SPEC (1.2) "...otherwise (<restriction> has no <simpleType> "
        * among its [children]), the simple type definition which is
        * the {content type} of the type definition `resolved` to by
        * the `actual value` of the base [attribute]"
        */
        String[] bns = { null }, bname = { null };
        int qres = pValAttrQName(ctxt, schema, null, node, "base", bns, bname);
        type.baseNs = bns[0];
        type.base = bname[0];
        if (qres == 0)
        {
            if ((type.base == null) && (type.type == XML_XSD10_SCHEMA_TYPE_COMPLEX))
            {
                pMissingAttrErr(ctxt,
                    XML_XSD10_SCHEMAP_S4S_ATTR_MISSING,
                    null, node, "base", null);
            }
            else if ((ctxt.isRedefine != 0) &&
                     ((type.flags & XML_XSD10_SCHEMAS_TYPE_GLOBAL) != 0))
            {
                if (type.base == null)
                {
                    pMissingAttrErr(ctxt,
                        XML_XSD10_SCHEMAP_S4S_ATTR_MISSING,
                        null, node, "base", null);
                }
                else if ((strEqual(type.base, type.name) == 0) ||
                         (strEqual(type.baseNs, type.targetNamespace) == 0))
                {
                    String[] str1 = { null }, str2 = { null };
                    /*
                    * REDEFINE: SPEC src-redefine (5)
                    * "Within the [children], each <simpleType> must have a
                    * <restriction> among its [children] ... the `actual value` of
                    * whose base [attribute] must be the same as the `actual value`
                    * of its own name attribute plus target namespace;"
                    */
                    pCustomErrExt(ctxt, XML_XSD10_SCHEMAP_SRC_REDEFINE,
                        null, node, "This is a redefinition, but the QName " +
                        "value '%s' of the 'base' attribute does not match the " +
                        "type's designation '%s'",
                        formatQName(str1, type.baseNs, type.base),
                        formatQName(str2, type.targetNamespace,
                            type.name), null);
                    /* Avoid confusion and erase the values. */
                    type.base = null;
                    type.baseNs = null;
                }
            }
        }
        /*
        * And now for the children...
        */
        child = parseFirstChild(node);
        if (isSchema(child, "annotation") != 0)
        {
            /*
            * Add the annotation to the simple type ancestor.
            */
            addAnnotation(type, parseAnnotation(ctxt, child, 1));
            child = parseNext(child);
        }
        if (parentType == XML_XSD10_SCHEMA_TYPE_SIMPLE)
        {
            /*
            * Corresponds to <simpleType><restriction><simpleType>.
            */
            if (isSchema(child, "simpleType") != 0)
            {
                if (type.base != null)
                {
                    /*
                    * src-restriction-base-or-simpleType
                    * Either the base [attribute] or the simpleType [child] of the
                    * <restriction> element must be present, but not both.
                    */
                    pContentErr(ctxt,
                        XML_XSD10_SCHEMAP_SRC_RESTRICTION_BASE_OR_SIMPLETYPE,
                        null, node, child,
                        "The attribute 'base' and the <simpleType> child are " +
                        "mutually exclusive", null);
                }
                else
                {
                    type.baseType = parseSimpleType(ctxt, schema, child, 0);
                }
                child = parseNext(child);
            }
            else if (type.base == null)
            {
                pContentErr(ctxt,
                    XML_XSD10_SCHEMAP_SRC_RESTRICTION_BASE_OR_SIMPLETYPE,
                    null, node, child,
                    "Either the attribute 'base' or a <simpleType> child " +
                    "must be present", null);
            }
        }
        else if (parentType == XML_XSD10_SCHEMA_TYPE_COMPLEX_CONTENT)
        {
            /*
            * Corresponds to <complexType><complexContent><restriction>...
            * followed by:
            *
            * Model groups <all>, <choice> and <sequence>.
            */
            if (isSchema(child, "all") != 0)
            {
                type.subtypes = parseModelGroup(ctxt, schema, child,
                    XML_XSD10_SCHEMA_TYPE_ALL, 1);
                child = parseNext(child);
            }
            else if (isSchema(child, "choice") != 0)
            {
                type.subtypes = parseModelGroup(ctxt,
                    schema, child, XML_XSD10_SCHEMA_TYPE_CHOICE, 1);
                child = parseNext(child);
            }
            else if (isSchema(child, "sequence") != 0)
            {
                type.subtypes = parseModelGroup(ctxt, schema, child,
                    XML_XSD10_SCHEMA_TYPE_SEQUENCE, 1);
                child = parseNext(child);
                /*
                * Model group reference <group>.
                */
            }
            else if (isSchema(child, "group") != 0)
            {
                type.subtypes = parseModelGroupDefRef(ctxt, schema, child);
                /*
                * Note that the reference will be resolved in
                * xmlSchemaResolveTypeReferences();
                */
                child = parseNext(child);
            }
        }
        else if (parentType == XML_XSD10_SCHEMA_TYPE_SIMPLE_CONTENT)
        {
            /*
            * Corresponds to <complexType><simpleContent><restriction>...
            *
            * "1.1 the simple type definition corresponding to the <simpleType>
            * among the [children] of <restriction> if there is one;"
            */
            if (isSchema(child, "simpleType") != 0)
            {
                /*
                * We will store the to-be-restricted simple type in
                * type->contentTypeDef *temporarily*.
                */
                type.contentTypeDef = parseSimpleType(ctxt, schema, child, 0);
                if (type.contentTypeDef == null)
                    return null;
                child = parseNext(child);
            }
        }

        if ((parentType == XML_XSD10_SCHEMA_TYPE_SIMPLE) ||
            (parentType == XML_XSD10_SCHEMA_TYPE_SIMPLE_CONTENT))
        {
            Xsd10Facet facet, lastfacet = null;
            /*
            * Corresponds to <complexType><simpleContent><restriction>...
            * <simpleType><restriction>...
            */

            /*
            * Add the facets to the simple type ancestor.
            */
            /*
            * TODO: Datatypes: 4.1.3 Constraints on XML Representation of
            * Simple Type Definition Schema Representation Constraint:
            * *Single Facet Value*
            */
            while ((isSchema(child, "minInclusive") != 0) ||
                   (isSchema(child, "minExclusive") != 0) ||
                   (isSchema(child, "maxInclusive") != 0) ||
                   (isSchema(child, "maxExclusive") != 0) ||
                   (isSchema(child, "totalDigits") != 0) ||
                   (isSchema(child, "fractionDigits") != 0) ||
                   (isSchema(child, "pattern") != 0) ||
                   (isSchema(child, "enumeration") != 0) ||
                   (isSchema(child, "whiteSpace") != 0) ||
                   (isSchema(child, "length") != 0) ||
                   (isSchema(child, "maxLength") != 0) ||
                   (isSchema(child, "minLength") != 0))
            {
                facet = parseFacet(ctxt, schema, child);
                if (facet != null)
                {
                    if (lastfacet == null)
                        type.facets = facet;
                    else
                        lastfacet.next = facet;
                    lastfacet = facet;
                    lastfacet.next = null;
                }
                child = parseNext(child);
            }
            /*
            * Create links for derivation and validation.
            */
            if (type.facets != null)
            {
                Xsd10FacetLink facetLink, lastFacetLink = null;

                facet = type.facets;
                do
                {
                    facetLink = new Xsd10FacetLink();
                    facetLink.facet = facet;
                    facetLink.next = null;
                    if (lastFacetLink == null)
                        type.facetSet = facetLink;
                    else
                        lastFacetLink.next = facetLink;
                    lastFacetLink = facetLink;
                    facet = facet.next;
                } while (facet != null);
            }
        }
        if (type.type == XML_XSD10_SCHEMA_TYPE_COMPLEX)
        {
            /*
            * Attribute uses/declarations.
            */
            XmlNode[] c = { child };
            int r = parseLocalAttributesOfType(ctxt, schema, c, type,
                XML_XSD10_SCHEMA_TYPE_RESTRICTION);
            child = c[0];
            if (r == -1)
                return null;
            /*
            * Attribute wildcard.
            */
            if (isSchema(child, "anyAttribute") != 0)
            {
                type.attributeWildcard =
                    parseAnyAttribute(ctxt, schema, child);
                child = parseNext(child);
            }
        }
        if (child != null)
        {
            if (parentType == XML_XSD10_SCHEMA_TYPE_COMPLEX_CONTENT)
            {
                pContentErr(ctxt,
                    XML_XSD10_SCHEMAP_S4S_ELEM_NOT_ALLOWED,
                    null, node, child, null,
                    "annotation?, (group | all | choice | sequence)?, " +
                    "((attribute | attributeGroup)*, anyAttribute?))");
            }
            else if (parentType == XML_XSD10_SCHEMA_TYPE_SIMPLE_CONTENT)
            {
                pContentErr(ctxt,
                    XML_XSD10_SCHEMAP_S4S_ELEM_NOT_ALLOWED,
                    null, node, child, null,
                    "(annotation?, (simpleType?, (minExclusive | minInclusive | " +
                    "maxExclusive | maxInclusive | totalDigits | fractionDigits | " +
                    "length | minLength | maxLength | enumeration | whiteSpace | " +
                    "pattern)*)?, ((attribute | attributeGroup)*, anyAttribute?))");
            }
            else
            {
                /* Simple type */
                pContentErr(ctxt,
                    XML_XSD10_SCHEMAP_S4S_ELEM_NOT_ALLOWED,
                    null, node, child, null,
                    "(annotation?, (simpleType?, (minExclusive | minInclusive | " +
                    "maxExclusive | maxInclusive | totalDigits | fractionDigits | " +
                    "length | minLength | maxLength | enumeration | whiteSpace | " +
                    "pattern)*))");
            }
        }
        return null;
    }

    // xml_xsd10_parse_extension
    /** Port of xmlSchemaParseExtension. */
    static Xsd10Type parseExtension(Xsd10ParserCtxt ctxt, Xsd10Schema schema, XmlNode node, int parentType)
    {
        Xsd10Type type;
        XmlNode child;
        XmlNode attr;

        if ((ctxt == null) || (schema == null) || (node == null))
            return null;
        /* Not a component, don't create it. */
        type = ctxt.ctxtType;
        if (type == null)
            return null;
        type.flags |= XML_XSD10_SCHEMAS_TYPE_DERIVATION_METHOD_EXTENSION;

        /*
        * Check for illegal attributes.
        */
        attr = parseFirstAttr(node);
        while (attr != null)
        {
            if (nodeNsUri(attr) == null)
            {
                if ((strEqual(nodeName(attr), "id") == 0) &&
                    (strEqual(nodeName(attr), "base") == 0))
                {
                    pIllegalAttrErr(ctxt,
                        XML_XSD10_SCHEMAP_S4S_ATTR_NOT_ALLOWED, null, attr);
                }
            }
            else if (strEqual(nodeNsUri(attr), XML_XSD10_NS) != 0)
            {
                pIllegalAttrErr(ctxt,
                    XML_XSD10_SCHEMAP_S4S_ATTR_NOT_ALLOWED, null, attr);
            }
            attr = nodeNext(attr);
        }

        pValAttrId(ctxt, node, "id");

        /*
        * Attribute "base" - mandatory.
        */
        String[] bns = { null }, bname = { null };
        int qres = pValAttrQName(ctxt, schema, null, node,
            "base", bns, bname);
        type.baseNs = bns[0];
        type.base = bname[0];
        if ((qres == 0) && (type.base == null))
        {
            pMissingAttrErr(ctxt,
                XML_XSD10_SCHEMAP_S4S_ATTR_MISSING,
                null, node, "base", null);
        }
        /*
        * And now for the children...
        */
        child = parseFirstChild(node);
        if (isSchema(child, "annotation") != 0)
        {
            /*
            * Add the annotation to the type ancestor.
            */
            addAnnotation(type, parseAnnotation(ctxt, child, 1));
            child = parseNext(child);
        }
        if (parentType == XML_XSD10_SCHEMA_TYPE_COMPLEX_CONTENT)
        {
            /*
            * Corresponds to <complexType><complexContent><extension>... and:
            *
            * Model groups <all>, <choice>, <sequence> and <group>.
            */
            if (isSchema(child, "all") != 0)
            {
                type.subtypes = parseModelGroup(ctxt, schema,
                    child, XML_XSD10_SCHEMA_TYPE_ALL, 1);
                child = parseNext(child);
            }
            else if (isSchema(child, "choice") != 0)
            {
                type.subtypes = parseModelGroup(ctxt, schema,
                    child, XML_XSD10_SCHEMA_TYPE_CHOICE, 1);
                child = parseNext(child);
            }
            else if (isSchema(child, "sequence") != 0)
            {
                type.subtypes = parseModelGroup(ctxt, schema,
                    child, XML_XSD10_SCHEMA_TYPE_SEQUENCE, 1);
                child = parseNext(child);
            }
            else if (isSchema(child, "group") != 0)
            {
                type.subtypes = parseModelGroupDefRef(ctxt, schema, child);
                /*
                * Note that the reference will be resolved in
                * xmlSchemaResolveTypeReferences();
                */
                child = parseNext(child);
            }
        }
        if (child != null)
        {
            /*
            * Attribute uses/declarations.
            */
            XmlNode[] c = { child };
            int r = parseLocalAttributesOfType(ctxt, schema, c, type,
                XML_XSD10_SCHEMA_TYPE_EXTENSION);
            child = c[0];
            if (r == -1)
                return null;
            /*
            * Attribute wildcard.
            */
            if (isSchema(child, "anyAttribute") != 0)
            {
                ctxt.ctxtType.attributeWildcard =
                    parseAnyAttribute(ctxt, schema, child);
                child = parseNext(child);
            }
        }
        if (child != null)
        {
            if (parentType == XML_XSD10_SCHEMA_TYPE_COMPLEX_CONTENT)
            {
                /* Complex content extension. */
                pContentErr(ctxt,
                    XML_XSD10_SCHEMAP_S4S_ELEM_NOT_ALLOWED,
                    null, node, child, null,
                    "(annotation?, ((group | all | choice | sequence)?, " +
                    "((attribute | attributeGroup)*, anyAttribute?)))");
            }
            else
            {
                /* Simple content extension. */
                pContentErr(ctxt,
                    XML_XSD10_SCHEMAP_S4S_ELEM_NOT_ALLOWED,
                    null, node, child, null,
                    "(annotation?, ((attribute | attributeGroup)*, " +
                    "anyAttribute?))");
            }
        }
        return null;
    }

    // xml_xsd10_parse_simple_content
    /** Port of xmlSchemaParseSimpleContent. Arrays: {@code hasRestrictionOrExtension} is C {@code int *} (an out param: caller allocated array of length 1; a real C array: the array). */
    static int parseSimpleContent(Xsd10ParserCtxt ctxt, Xsd10Schema schema, XmlNode node, int[] hasRestrictionOrExtension)
    {
        Xsd10Type type;
        XmlNode child;
        XmlNode attr;

        if ((ctxt == null) || (schema == null) || (node == null) ||
            (hasRestrictionOrExtension == null))
            return -1;
        hasRestrictionOrExtension[0] = 0;
        /* Not a component, don't create it. */
        type = ctxt.ctxtType;
        if (type == null)
            return -1;
        type.contentType = XML_XSD10_SCHEMA_CONTENT_SIMPLE;
        /*
        * Check for illegal attributes.
        */
        attr = parseFirstAttr(node);
        while (attr != null)
        {
            if (nodeNsUri(attr) == null)
            {
                if (strEqual(nodeName(attr), "id") == 0)
                {
                    pIllegalAttrErr(ctxt,
                        XML_XSD10_SCHEMAP_S4S_ATTR_NOT_ALLOWED, null, attr);
                }
            }
            else if (strEqual(nodeNsUri(attr), XML_XSD10_NS) != 0)
            {
                pIllegalAttrErr(ctxt,
                    XML_XSD10_SCHEMAP_S4S_ATTR_NOT_ALLOWED, null, attr);
            }
            attr = nodeNext(attr);
        }

        pValAttrId(ctxt, node, "id");

        /*
        * And now for the children...
        */
        child = parseFirstChild(node);
        if (isSchema(child, "annotation") != 0)
        {
            /*
            * Add the annotation to the complex type ancestor.
            */
            addAnnotation(type, parseAnnotation(ctxt, child, 1));
            child = parseNext(child);
        }
        if (child == null)
        {
            pContentErr(ctxt,
                XML_XSD10_SCHEMAP_S4S_ELEM_MISSING,
                null, node, null, null,
                "(annotation?, (restriction | extension))");
        }
        if (child == null)
        {
            pContentErr(ctxt,
                XML_XSD10_SCHEMAP_S4S_ELEM_MISSING,
                null, node, null, null,
                "(annotation?, (restriction | extension))");
        }
        if (isSchema(child, "restriction") != 0)
        {
            parseRestriction(ctxt, schema, child,
                XML_XSD10_SCHEMA_TYPE_SIMPLE_CONTENT);
            hasRestrictionOrExtension[0] = 1;
            child = parseNext(child);
        }
        else if (isSchema(child, "extension") != 0)
        {
            parseExtension(ctxt, schema, child,
                XML_XSD10_SCHEMA_TYPE_SIMPLE_CONTENT);
            hasRestrictionOrExtension[0] = 1;
            child = parseNext(child);
        }
        if (child != null)
        {
            pContentErr(ctxt,
                XML_XSD10_SCHEMAP_S4S_ELEM_NOT_ALLOWED,
                null, node, child, null,
                "(annotation?, (restriction | extension))");
        }
        return 0;
    }

    // xml_xsd10_parse_complex_content
    /** Port of xmlSchemaParseComplexContent. Arrays: {@code hasRestrictionOrExtension} is C {@code int *} (an out param: caller allocated array of length 1; a real C array: the array). */
    static int parseComplexContent(Xsd10ParserCtxt ctxt, Xsd10Schema schema, XmlNode node, int[] hasRestrictionOrExtension)
    {
        Xsd10Type type;
        XmlNode child;
        XmlNode attr;

        if ((ctxt == null) || (schema == null) || (node == null) ||
            (hasRestrictionOrExtension == null))
            return -1;
        hasRestrictionOrExtension[0] = 0;
        /* Not a component, don't create it. */
        type = ctxt.ctxtType;
        if (type == null)
            return -1;
        /*
        * Check for illegal attributes.
        */
        attr = parseFirstAttr(node);
        while (attr != null)
        {
            if (nodeNsUri(attr) == null)
            {
                if ((strEqual(nodeName(attr), "id") == 0) &&
                    (strEqual(nodeName(attr), "mixed") == 0))
                {
                    pIllegalAttrErr(ctxt,
                        XML_XSD10_SCHEMAP_S4S_ATTR_NOT_ALLOWED, null, attr);
                }
            }
            else if (strEqual(nodeNsUri(attr), XML_XSD10_NS) != 0)
            {
                pIllegalAttrErr(ctxt,
                    XML_XSD10_SCHEMAP_S4S_ATTR_NOT_ALLOWED, null, attr);
            }
            attr = nodeNext(attr);
        }

        pValAttrId(ctxt, node, "id");

        /*
        * Set the 'mixed' on the complex type ancestor.
        */
        if (getBooleanProp(ctxt, node, "mixed", 0) != 0)
        {
            if ((type.flags & XML_XSD10_SCHEMAS_TYPE_MIXED) == 0)
                type.flags |= XML_XSD10_SCHEMAS_TYPE_MIXED;
        }
        child = parseFirstChild(node);
        if (isSchema(child, "annotation") != 0)
        {
            /*
            * Add the annotation to the complex type ancestor.
            */
            addAnnotation(type, parseAnnotation(ctxt, child, 1));
            child = parseNext(child);
        }
        if (child == null)
        {
            pContentErr(ctxt,
                XML_XSD10_SCHEMAP_S4S_ELEM_MISSING,
                null, node, null,
                null, "(annotation?, (restriction | extension))");
        }
        if (child == null)
        {
            pContentErr(ctxt,
                XML_XSD10_SCHEMAP_S4S_ELEM_MISSING,
                null, node, null,
                null, "(annotation?, (restriction | extension))");
        }
        if (isSchema(child, "restriction") != 0)
        {
            parseRestriction(ctxt, schema, child,
                XML_XSD10_SCHEMA_TYPE_COMPLEX_CONTENT);
            hasRestrictionOrExtension[0] = 1;
            child = parseNext(child);
        }
        else if (isSchema(child, "extension") != 0)
        {
            parseExtension(ctxt, schema, child,
                XML_XSD10_SCHEMA_TYPE_COMPLEX_CONTENT);
            hasRestrictionOrExtension[0] = 1;
            child = parseNext(child);
        }
        if (child != null)
        {
            pContentErr(ctxt,
                XML_XSD10_SCHEMAP_S4S_ELEM_NOT_ALLOWED,
                null, node, child,
                null, "(annotation?, (restriction | extension))");
        }
        return 0;
    }

    // xml_xsd10_parse_complex_type
    /** Port of xmlSchemaParseComplexType. */
    static Xsd10Type parseComplexType(Xsd10ParserCtxt ctxt, Xsd10Schema schema, XmlNode node, int topLevel)
    {
        Xsd10Type type, ctxtType;
        XmlNode child;
        String[] name = { null };
        XmlNode attr;
        String attrValue;
        int final_ = 0, block = 0;
        int[] hasRestrictionOrExtension = { 0 };

        if ((ctxt == null) || (schema == null) || (node == null))
            return null;

        ctxtType = ctxt.ctxtType;

        if (topLevel != 0)
        {
            attr = getPropNode(node, "name");
            if (attr == null)
            {
                pMissingAttrErr(ctxt,
                    XML_XSD10_SCHEMAP_S4S_ATTR_MISSING, null, node, "name", null);
                return null;
            }
            else if (pValAttrNode(ctxt, null, attr,
                getBuiltInType(XML_XSD10_SCHEMAS_NCNAME), name) != 0)
            {
                return null;
            }
        }

        if (topLevel == 0)
        {
            /*
            * Parse as local complex type definition.
            */
            type = addType(ctxt, schema,
                XML_XSD10_SCHEMA_TYPE_COMPLEX,
                null, ctxt.targetNamespace, node, 0);
            if (type == null)
                return null;
            name[0] = type.name;
            type.node = node;
            type.type = XML_XSD10_SCHEMA_TYPE_COMPLEX;
            /*
            * TODO: We need the target namespace.
            */
        }
        else
        {
            /*
            * Parse as global complex type definition.
            */
            type = addType(ctxt, schema,
                XML_XSD10_SCHEMA_TYPE_COMPLEX,
                name[0], ctxt.targetNamespace, node, 1);
            if (type == null)
                return null;
            type.node = node;
            type.type = XML_XSD10_SCHEMA_TYPE_COMPLEX;
            type.flags |= XML_XSD10_SCHEMAS_TYPE_GLOBAL;
        }
        type.targetNamespace = ctxt.targetNamespace;
        /*
        * Handle attributes.
        */
        attr = parseFirstAttr(node);
        while (attr != null)
        {
            if (nodeNsUri(attr) == null)
            {
                if (strEqual(nodeName(attr), "id") != 0)
                {
                    /*
                    * Attribute "id".
                    */
                    pValAttrId(ctxt, node, "id");
                }
                else if (strEqual(nodeName(attr), "mixed") != 0)
                {
                    /*
                    * Attribute "mixed".
                    */
                    if (pGetBoolNodeValue(ctxt, null, attr) != 0)
                        type.flags |= XML_XSD10_SCHEMAS_TYPE_MIXED;
                }
                else if (topLevel != 0)
                {
                    /*
                    * Attributes of global complex type definitions.
                    */
                    if (strEqual(nodeName(attr), "name") != 0)
                    {
                        /* Pass. */
                    }
                    else if (strEqual(nodeName(attr), "abstract") != 0)
                    {
                        /*
                        * Attribute "abstract".
                        */
                        if (pGetBoolNodeValue(ctxt, null, attr) != 0)
                            type.flags |= XML_XSD10_SCHEMAS_TYPE_ABSTRACT;
                    }
                    else if (strEqual(nodeName(attr), "final") != 0)
                    {
                        /*
                        * Attribute "final".
                        */
                        attrValue = getNodeContent(ctxt, attr);
                        int[] fl = { type.flags };
                        int r = pValAttrBlockFinal(attrValue,
                            fl,
                            -1,
                            XML_XSD10_SCHEMAS_TYPE_FINAL_EXTENSION,
                            XML_XSD10_SCHEMAS_TYPE_FINAL_RESTRICTION,
                            -1, -1, -1);
                        type.flags = fl[0];
                        if (r != 0)
                        {
                            pSimpleTypeErr(ctxt,
                                XML_XSD10_SCHEMAP_S4S_ATTR_INVALID_VALUE,
                                null, attr, null,
                                "(#all | List of (extension | restriction))",
                                attrValue, null, null, null);
                        }
                        else
                            final_ = 1;
                    }
                    else if (strEqual(nodeName(attr), "block") != 0)
                    {
                        /*
                        * Attribute "block".
                        */
                        attrValue = getNodeContent(ctxt, attr);
                        int[] fl = { type.flags };
                        int r = pValAttrBlockFinal(attrValue, fl,
                            -1,
                            XML_XSD10_SCHEMAS_TYPE_BLOCK_EXTENSION,
                            XML_XSD10_SCHEMAS_TYPE_BLOCK_RESTRICTION,
                            -1, -1, -1);
                        type.flags = fl[0];
                        if (r != 0)
                        {
                            pSimpleTypeErr(ctxt,
                                XML_XSD10_SCHEMAP_S4S_ATTR_INVALID_VALUE,
                                null, attr, null,
                                "(#all | List of (extension | restriction)) ",
                                attrValue, null, null, null);
                        }
                        else
                            block = 1;
                    }
                    else
                    {
                        pIllegalAttrErr(ctxt,
                            XML_XSD10_SCHEMAP_S4S_ATTR_NOT_ALLOWED, null, attr);
                    }
                }
                else
                {
                    pIllegalAttrErr(ctxt,
                        XML_XSD10_SCHEMAP_S4S_ATTR_NOT_ALLOWED, null, attr);
                }
            }
            else if (strEqual(nodeNsUri(attr), XML_XSD10_NS) != 0)
            {
                pIllegalAttrErr(ctxt,
                    XML_XSD10_SCHEMAP_S4S_ATTR_NOT_ALLOWED, null, attr);
            }
            attr = nodeNext(attr);
        }
        if (block == 0)
        {
            /*
            * Apply default "block" values.
            */
            if ((schema.flags & XML_XSD10_SCHEMAS_BLOCK_DEFAULT_RESTRICTION) != 0)
                type.flags |= XML_XSD10_SCHEMAS_TYPE_BLOCK_RESTRICTION;
            if ((schema.flags & XML_XSD10_SCHEMAS_BLOCK_DEFAULT_EXTENSION) != 0)
                type.flags |= XML_XSD10_SCHEMAS_TYPE_BLOCK_EXTENSION;
        }
        if (final_ == 0)
        {
            /*
            * Apply default "block" values.
            */
            if ((schema.flags & XML_XSD10_SCHEMAS_FINAL_DEFAULT_RESTRICTION) != 0)
                type.flags |= XML_XSD10_SCHEMAS_TYPE_FINAL_RESTRICTION;
            if ((schema.flags & XML_XSD10_SCHEMAS_FINAL_DEFAULT_EXTENSION) != 0)
                type.flags |= XML_XSD10_SCHEMAS_TYPE_FINAL_EXTENSION;
        }
        /*
        * And now for the children...
        */
        child = parseFirstChild(node);
        if (isSchema(child, "annotation") != 0)
        {
            type.annot = parseAnnotation(ctxt, child, 1);
            child = parseNext(child);
        }
        ctxt.ctxtType = type;
        if (isSchema(child, "simpleContent") != 0)
        {
            /*
            * <complexType><simpleContent>...
            * 3.4.3 : 2.2
            * Specifying mixed='true' when the <simpleContent>
            * alternative is chosen has no effect
            */
            if ((type.flags & XML_XSD10_SCHEMAS_TYPE_MIXED) != 0)
                type.flags ^= XML_XSD10_SCHEMAS_TYPE_MIXED;
            parseSimpleContent(ctxt, schema, child,
                hasRestrictionOrExtension);
            child = parseNext(child);
        }
        else if (isSchema(child, "complexContent") != 0)
        {
            /*
            * <complexType><complexContent>...
            */
            type.contentType = XML_XSD10_SCHEMA_CONTENT_EMPTY;
            parseComplexContent(ctxt, schema, child,
                hasRestrictionOrExtension);
            child = parseNext(child);
        }
        else
        {
            /*
            * E.g <complexType><sequence>... or <complexType><attribute>... etc.
            *
            * SPEC
            * "...the third alternative (neither <simpleContent> nor
            * <complexContent>) is chosen. This case is understood as shorthand
            * for complex content restricting the `ur-type definition`, and the
            * details of the mappings should be modified as necessary.
            */
            type.baseType = getBuiltInType(XML_XSD10_SCHEMAS_ANYTYPE);
            type.flags |= XML_XSD10_SCHEMAS_TYPE_DERIVATION_METHOD_RESTRICTION;
            /*
            * Parse model groups.
            */
            if (isSchema(child, "all") != 0)
            {
                type.subtypes = parseModelGroup(ctxt, schema, child,
                    XML_XSD10_SCHEMA_TYPE_ALL, 1);
                child = parseNext(child);
            }
            else if (isSchema(child, "choice") != 0)
            {
                type.subtypes = parseModelGroup(ctxt, schema, child,
                    XML_XSD10_SCHEMA_TYPE_CHOICE, 1);
                child = parseNext(child);
            }
            else if (isSchema(child, "sequence") != 0)
            {
                type.subtypes = parseModelGroup(ctxt, schema, child,
                    XML_XSD10_SCHEMA_TYPE_SEQUENCE, 1);
                child = parseNext(child);
            }
            else if (isSchema(child, "group") != 0)
            {
                type.subtypes = parseModelGroupDefRef(ctxt, schema, child);
                /*
                * Note that the reference will be resolved in
                * xmlSchemaResolveTypeReferences();
                */
                child = parseNext(child);
            }
            /*
            * Parse attribute decls/refs.
            */
            XmlNode[] c = { child };
            int r = parseLocalAttributesOfType(ctxt, schema, c, type,
                XML_XSD10_SCHEMA_TYPE_RESTRICTION);
            child = c[0];
            if (r == -1)
                return null;
            /*
            * Parse attribute wildcard.
            */
            if (isSchema(child, "anyAttribute") != 0)
            {
                type.attributeWildcard = parseAnyAttribute(ctxt, schema, child);
                child = parseNext(child);
            }
        }
        if (child != null)
        {
            pContentErr(ctxt,
                XML_XSD10_SCHEMAP_S4S_ELEM_NOT_ALLOWED,
                null, node, child,
                null, "(annotation?, (simpleContent | complexContent | " +
                "((group | all | choice | sequence)?, ((attribute | " +
                "attributeGroup)*, anyAttribute?))))");
        }
        /*
        * REDEFINE: SPEC src-redefine (5)
        */
        if ((topLevel != 0) && (ctxt.isRedefine != 0) && (hasRestrictionOrExtension[0] == 0))
        {
            pCustomErr(ctxt, XML_XSD10_SCHEMAP_SRC_REDEFINE,
                null, node, "This is a redefinition, thus the " +
                "<complexType> must have a <restriction> or <extension> " +
                "grand-child", null);
        }
        ctxt.ctxtType = ctxtType;
        return type;
    }
}
