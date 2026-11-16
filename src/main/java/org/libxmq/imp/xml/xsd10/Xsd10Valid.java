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

import org.libxmq.imp.xml.XmlDocument;
import org.libxmq.imp.xml.XmlNode;

import static org.libxmq.imp.xml.XmlDomApi.*;
import static org.libxmq.imp.xml.xsd10.Xsd10Const.*;
import static org.libxmq.imp.xml.xsd10.Xsd10Schemas.*;
import static org.libxmq.imp.xml.xsd10.Xsd10Parse.*;
import static org.libxmq.imp.xml.xsd10.Xsd10Build.*;
import static org.libxmq.imp.xml.xsd10.Xsd10Types.*;


import java.util.Arrays;
import java.util.Objects;

import org.libxmq.imp.xml.XmlAttr;
import org.libxmq.imp.xml.XmlNs;
import org.libxmq.imp.xml.XmlPattern;
import org.libxmq.imp.xml.XmlRegExecCtxt;
import org.libxmq.imp.xml.XmlRegexp;
import org.libxmq.imp.xml.XmlStreamCtxt;

/** Port of xml_xsd10_valid.c: simple type validation, dom validation and the identity constraints. */
public final class Xsd10Valid
{
    private Xsd10Valid()
    {
    }

    /** Size of a C pointer, used for the grow capacity computations. */
    private static final long PTR_SIZE = 8;

    // XSD10_VALID_IS_BLANK_CH
    private static boolean validIsBlankCh(int c)
    {
        return (c == 0x20) || ((0x9 <= c) && (c <= 0xa)) || (c == 0xd);
    }

    /** C *p on a String: the char at i or 0 past the end. */
    private static int at(String s, int i)
    {
        return (i < s.length()) ? s.charAt(i) : 0;
    }

    /* ***********************************************************************
     *                                                                      *
     *                      DOM Validation code                             *
     *                                                                      *
     ************************************************************************/

    // xml_xsd10_assemble_by_location
    /** Port of xml_xsd10_assemble_by_location. Expands an existing schema by an additional schema. */
    static int assembleByLocation(Xsd10ValidCtxt vctxt, Xsd10Schema schema, XmlNode node, String nsName, String location)
    {
        int ret = 0;
        Xsd10ParserCtxt pctxt;
        Xsd10Bucket[] bucket = new Xsd10Bucket[] { null };
        String baseURI = null;

        if ((vctxt == null) || (schema == null))
        {
            return -1;
        }

        if (vctxt.pctxt == null)
        {
            verrorInt(vctxt, "xmlSchemaAssembleByLocation", "no parser context available");
            return -1;
        }
        pctxt = vctxt.pctxt;
        if (pctxt.constructor == null)
        {
            perrorInt(pctxt, "xmlSchemaAssembleByLocation", "no constructor");
            return -1;
        }
        /*
        * Acquire the schema document.
        * (The streaming modes are not ported: node is never NULL here.)
        */
        location = buildAbsoluteUri(pctxt.dict, location, node, baseURI);
        /*
        * Note that we pass XML_SCHEMA_SCHEMA_IMPORT here;
        * the process will automatically change this to
        * XML_SCHEMA_SCHEMA_MAIN if it is the first schema document.
        */
        ret = addSchemaDoc(pctxt, XML_XSD10_SCHEMA_SCHEMA_IMPORT, location, null, null, 0, node, null, nsName, bucket);
        if (ret != 0)
        {
            return ret;
        }
        if (bucket[0] == null)
        {
            /*
            * Generate a warning that the document could not be located.
            */
            customWarning(vctxt, XML_XSD10_SCHEMAV_MISC, node, null,
                "The document at location '%s' could not be acquired",
                location, null, null);
            return ret;
        }
        /*
        * The first located schema will be handled as if all other
        * schemas imported by XSI were imported by this first schema.
        */
        if ((bucket[0] != null) && (wxsConstructor(pctxt).bucket == null))
        {
            wxsConstructor(pctxt).bucket = bucket[0];
        }
        /*
        * TODO: Is this handled like an import? I.e. is it not an error
        * if the schema cannot be located?
        */
        if ((bucket[0] == null) || (canParseSchema(bucket[0]) == 0))
        {
            return 0;
        }
        /*
        * We will reuse the parser context for every schema imported
        * directly via XSI. So reset the context.
        */
        pctxt.nberrors = 0;
        pctxt.err = 0;
        pctxt.doc = bucket[0].doc;

        ret = parseNewDocWithContext(pctxt, schema, bucket[0]);
        if (ret == -1)
        {
            pctxt.doc = null;
            pctxt.doc = null;
            return -1;
        }
        /* Paranoid error channelling. */
        if ((ret == 0) && (pctxt.nberrors != 0))
        {
            ret = pctxt.err;
        }
        if (pctxt.nberrors == 0)
        {
            /*
            * Only bother to fixup pending components, if there was
            * no error yet.
            */
            fixupComponents(pctxt, bucket[0]);
            ret = pctxt.err;
            /*
            * Not nice, but we need somehow to channel the schema parser
            * error to the validation context.
            */
            if ((ret != 0) && (vctxt.err == 0))
            {
                vctxt.err = ret;
            }
            vctxt.nberrors += pctxt.nberrors;
        }
        else
        {
            /* Add to validation error sum. */
            vctxt.nberrors += pctxt.nberrors;
        }
        pctxt.doc = null;
        return ret;
    }

    // xml_xsd10_get_meta_attr_info
    /** Port of xmlSchemaGetMetaAttrInfo. */
    static Xsd10AttrInfo getMetaAttrInfo(Xsd10ValidCtxt vctxt, int metaType)
    {
        if (vctxt.nbAttrInfos == 0)
        {
            return null;
        }
        for (int i = 0; i < vctxt.nbAttrInfos; i++)
        {
            Xsd10AttrInfo iattr = vctxt.attrInfos[i];
            if (iattr.metaType == metaType)
            {
                return iattr;
            }
        }
        return null;
    }

    // xml_xsd10_assemble_by_xsi
    /** Port of xmlSchemaAssembleByXSI. */
    static int assembleByXsi(Xsd10ValidCtxt vctxt)
    {
        int cur;
        int end;
        String nsname = null;
        String location;
        int ret = 0;
        Xsd10AttrInfo iattr;

        /*
        * Parse the value; we will assume an even number of values
        * to be given (this is how Xerces and XSV work).
        */
        iattr = getMetaAttrInfo(vctxt, XML_XSD10_SCHEMA_ATTR_INFO_META_XSI_SCHEMA_LOC);
        if (iattr == null)
        {
            iattr = getMetaAttrInfo(vctxt, XML_XSD10_SCHEMA_ATTR_INFO_META_XSI_NO_NS_SCHEMA_LOC);
        }
        if (iattr == null)
        {
            return 0;
        }
        String v = (iattr.value == null) ? "" : iattr.value;
        cur = 0;
        do
        {
            if (iattr.metaType == XML_XSD10_SCHEMA_ATTR_INFO_META_XSI_SCHEMA_LOC)
            {
                /*
                * Get the namespace name.
                */
                while (validIsBlankCh(at(v, cur)))
                {
                    cur++;
                }
                end = cur;
                while ((at(v, end) != 0) && (!validIsBlankCh(at(v, end))))
                {
                    end++;
                }
                if (end == cur)
                {
                    break;
                }
                /* TODO: Don't use the schema's dict. */
                nsname = dictLookup(vctxt.schema.dict, v.substring(cur, end), -1);
                cur = end;
            }
            /*
            * Get the URI.
            */
            while (validIsBlankCh(at(v, cur)))
            {
                cur++;
            }
            end = cur;
            while ((at(v, end) != 0) && (!validIsBlankCh(at(v, end))))
            {
                end++;
            }
            if (end == cur)
            {
                if (iattr.metaType == XML_XSD10_SCHEMA_ATTR_INFO_META_XSI_SCHEMA_LOC)
                {
                    /*
                    * If using @schemaLocation then tuples are expected.
                    * I.e. the namespace name *and* the document's URI.
                    */
                    customWarning(vctxt, XML_XSD10_SCHEMAV_MISC, iattr.node, null,
                        "The value must consist of tuples: the target namespace " +
                        "name and the document's URI", null, null, null);
                }
                break;
            }
            /* TODO: Don't use the schema's dict. */
            location = dictLookup(vctxt.schema.dict, v.substring(cur, end), -1);
            cur = end;
            ret = assembleByLocation(vctxt, vctxt.schema, iattr.node, nsname, location);
            if (ret == -1)
            {
                verrorInt(vctxt, "xmlSchemaAssembleByXSI", "assembling schemata");
                return -1;
            }
        } while (at(v, cur) != 0);
        return ret;
    }

    // xml_xsd10_lookup_namespace
    /** Port of xmlSchemaLookupNamespace. */
    static String lookupNamespace(Xsd10ValidCtxt vctxt, String prefix)
    {
        /* The SAX (nsBindings) and reader branches are not ported. */
        XmlNs ns;

        if ((vctxt.inode.node == null) || (nodeDom(vctxt.inode.node) == null))
        {
            verrorInt(vctxt, "xmlSchemaLookupNamespace", "no node or node's doc available");
            return null;
        }
        ns = treeSearchNs(vctxt.inode.node, prefix);
        if (ns != null)
        {
            return nsHref(ns);
        }
        return null;
    }

    /*
    * This one works on the schema of the validation context.
    */
    // xml_xsd10_validate_notation
    /** Port of xmlSchemaValidateNotation. */
    static int validateNotation(Xsd10ValidCtxt vctxt, Xsd10Schema schema, XmlNode node, String value, Xsd10Val[] val, int valNeeded)
    {
        int ret;

        if ((vctxt != null) && (vctxt.schema == null))
        {
            verrorInt(vctxt, "xmlSchemaValidateNotation", "a schema is needed on the validation context");
            return -1;
        }
        ret = treeValidateQname(value, 1);
        if (ret != 0)
        {
            return ret;
        }
        String localName;
        String[] prefix = new String[] { null };

        localName = treeSplitQname2(value, prefix);
        if (prefix[0] != null)
        {
            String nsName = null;

            if (vctxt != null)
            {
                nsName = lookupNamespace(vctxt, prefix[0]);
            }
            else if (node != null)
            {
                XmlNs ns = treeSearchNs(node, prefix[0]);
                if (ns != null)
                {
                    nsName = nsHref(ns);
                }
            }
            else
            {
                return 1;
            }
            if (nsName == null)
            {
                return 1;
            }
            if (getNotation(schema, localName, nsName) != null)
            {
                if ((valNeeded != 0) && (val != null))
                {
                    val[0] = newNotationValue(strdup(localName), strdup(nsName));
                    if (val[0] == null)
                    {
                        ret = -1;
                    }
                }
            }
            else
            {
                ret = 1;
            }
        }
        else
        {
            if (getNotation(schema, value, null) != null)
            {
                if ((valNeeded != 0) && (val != null))
                {
                    val[0] = newNotationValue(strdup(value), null);
                    if (val[0] == null)
                    {
                        ret = -1;
                    }
                }
            }
            else
            {
                return 1;
            }
        }
        return ret;
    }

    // xml_xsd10_v_add_node_q_name
    /** Port of xmlSchemaVAddNodeQName. */
    static int vAddNodeQName(Xsd10ValidCtxt vctxt, String lname, String nsname)
    {
        int i;

        lname = dictLookup(vctxt.dict, lname, -1);
        if (lname == null)
        {
            return -1;
        }
        if (nsname != null)
        {
            nsname = dictLookup(vctxt.dict, nsname, -1);
            if (nsname == null)
            {
                return -1;
            }
        }
        for (i = 0; i < vctxt.nodeQNames.nbItems; i += 2)
        {
            if (Objects.equals(vctxt.nodeQNames.items[i], lname) &&
                Objects.equals(vctxt.nodeQNames.items[i + 1], nsname))
            {
                /* Already there */
                return i;
            }
        }
        /* Add new entry. */
        i = vctxt.nodeQNames.nbItems;
        if (itemListAdd(vctxt.nodeQNames, lname) < 0)
        {
            vErrMemory(vctxt);
            return -1;
        }
        if (itemListAdd(vctxt.nodeQNames, nsname) < 0)
        {
            vctxt.nodeQNames.nbItems--;
            vErrMemory(vctxt);
            return -1;
        }
        return i;
    }

    /* ***********************************************************************
     *                                                                      *
     *  Validation of identity-constraints (IDC)                            *
     *                                                                      *
     ************************************************************************/

    // xml_xsd10_augment_idc
    /** Port of xmlSchemaAugmentIDC. Creates an augmented IDC definition item. */
    static void augmentIdc(Object payload, Object data, String name)
    {
        Xsd10IDC idcDef = (Xsd10IDC) payload;
        Xsd10ValidCtxt vctxt = (Xsd10ValidCtxt) data;
        Xsd10IDCAug aidc;

        aidc = new Xsd10IDCAug();
        aidc.keyrefDepth = -1;
        aidc.def = idcDef;
        aidc.next = null;
        if (vctxt.aidcs == null)
        {
            vctxt.aidcs = aidc;
        }
        else
        {
            aidc.next = vctxt.aidcs;
            vctxt.aidcs = aidc;
        }
        /*
        * Save if we have keyrefs at all.
        */
        if ((vctxt.hasKeyrefs == 0) && (idcDef.type == XML_XSD10_SCHEMA_TYPE_IDC_KEYREF))
        {
            vctxt.hasKeyrefs = 1;
        }
    }

    // xml_xsd10_augment_imported_idc
    /** Port of xmlSchemaAugmentImportedIDC. Creates an augmented IDC definition for the imported schema. */
    static void augmentImportedIdc(Object payload, Object data, String name)
    {
        Xsd10Import imported = (Xsd10Import) payload;
        Xsd10ValidCtxt vctxt = (Xsd10ValidCtxt) data;
        if (imported.schema.idcDef != null)
        {
            hashScan(imported.schema.idcDef, Xsd10Valid::augmentIdc, vctxt);
        }
    }

    // xml_xsd10_idc_new_binding
    /** Port of xmlSchemaIDCNewBinding. Creates a new IDC binding. */
    static Xsd10PSVIIDCBinding idcNewBinding(Xsd10IDC idcDef)
    {
        Xsd10PSVIIDCBinding ret = new Xsd10PSVIIDCBinding();
        ret.definition = idcDef;
        return ret;
    }

    // xml_xsd10_idc_store_node_table_item
    /** Port of xmlSchemaIDCStoreNodeTableItem. */
    static int idcStoreNodeTableItem(Xsd10ValidCtxt vctxt, Xsd10PSVIIDCNode item)
    {
        /*
        * Add to global list.
        */
        if (vctxt.sizeIdcNodes <= vctxt.nbIdcNodes)
        {
            int newSize = growCapacity(vctxt.sizeIdcNodes, PTR_SIZE, 20, XML_XSD10_MAX_ITEMS);
            if (newSize < 0)
            {
                vErrMemory(vctxt);
                return -1;
            }
            vctxt.idcNodes = (vctxt.idcNodes == null) ? new Xsd10PSVIIDCNode[newSize] : Arrays.copyOf(vctxt.idcNodes, newSize);
            vctxt.sizeIdcNodes = newSize;
        }

        vctxt.idcNodes[vctxt.nbIdcNodes++] = item;
        return 0;
    }

    // xml_xsd10_idc_store_key
    /** Port of xmlSchemaIDCStoreKey. */
    static int idcStoreKey(Xsd10ValidCtxt vctxt, Xsd10PSVIIDCKey key)
    {
        /*
        * Add to global list.
        */
        if (vctxt.sizeIdcKeys <= vctxt.nbIdcKeys)
        {
            int newSize = growCapacity(vctxt.sizeIdcKeys, PTR_SIZE, 20, XML_XSD10_MAX_ITEMS);
            if (newSize < 0)
            {
                vErrMemory(vctxt);
                return -1;
            }
            vctxt.idcKeys = (vctxt.idcKeys == null) ? new Xsd10PSVIIDCKey[newSize] : Arrays.copyOf(vctxt.idcKeys, newSize);
            vctxt.sizeIdcKeys = newSize;
        }

        vctxt.idcKeys[vctxt.nbIdcKeys++] = key;
        return 0;
    }

    // xml_xsd10_idc_append_node_table_item
    /** Port of xmlSchemaIDCAppendNodeTableItem. */
    static int idcAppendNodeTableItem(Xsd10PSVIIDCBinding bind, Xsd10PSVIIDCNode ntItem)
    {
        if (bind.sizeNodes <= bind.nbNodes)
        {
            int newSize = growCapacity(bind.sizeNodes, PTR_SIZE, 10, XML_XSD10_MAX_ITEMS);
            if (newSize < 0)
            {
                vErrMemory(null);
                return -1;
            }
            bind.nodeTable = (bind.nodeTable == null) ? new Xsd10PSVIIDCNode[newSize] : Arrays.copyOf(bind.nodeTable, newSize);
            bind.sizeNodes = newSize;
        }

        bind.nodeTable[bind.nbNodes++] = ntItem;
        return 0;
    }

    // xml_xsd10_idc_acquire_binding
    /** Port of xmlSchemaIDCAcquireBinding. */
    static Xsd10PSVIIDCBinding idcAcquireBinding(Xsd10ValidCtxt vctxt, Xsd10IDCMatcher matcher)
    {
        Xsd10NodeInfo ielem;

        ielem = vctxt.elemInfos[matcher.depth];

        if (ielem.idcTable == null)
        {
            ielem.idcTable = idcNewBinding(matcher.aidc.def);
            if (ielem.idcTable == null)
            {
                return null;
            }
            return ielem.idcTable;
        }
        else
        {
            Xsd10PSVIIDCBinding bind = ielem.idcTable;
            do
            {
                if (bind.definition == matcher.aidc.def)
                {
                    return bind;
                }
                if (bind.next == null)
                {
                    bind.next = idcNewBinding(matcher.aidc.def);
                    if (bind.next == null)
                    {
                        return null;
                    }
                    return bind.next;
                }
                bind = bind.next;
            } while (bind != null);
        }
        return null;
    }

    // xml_xsd10_idc_acquire_target_list
    /** Port of xmlSchemaIDCAcquireTargetList. */
    static Xsd10ItemList idcAcquireTargetList(Xsd10ValidCtxt vctxt, Xsd10IDCMatcher matcher)
    {
        if (matcher.targets == null)
        {
            matcher.targets = itemListCreate();
            if (matcher.targets == null)
            {
                vErrMemory(vctxt);
            }
        }
        return matcher.targets;
    }

    // xml_xsd10_idc_free_key
    /** Port of xmlSchemaIDCFreeKey. Frees an IDC key together with its compiled value. */
    static void idcFreeKey(Xsd10PSVIIDCKey key)
    {
        if (key.val != null)
        {
            freeValue(key.val);
        }
    }

    // xml_xsd10_idc_free_binding
    /** Port of xmlSchemaIDCFreeBinding. Note that the node table-items are not freed. */
    static void idcFreeBinding(Xsd10PSVIIDCBinding bind)
    {
        bind.nodeTable = null;
        if (bind.dupls != null)
        {
            itemListFree(bind.dupls);
        }
    }

    // xml_xsd10_idc_free_idc_table
    /** Port of xmlSchemaIDCFreeIDCTable. Frees all the IDC bindings in the list. */
    static void idcFreeIdcTable(Xsd10PSVIIDCBinding bind)
    {
        Xsd10PSVIIDCBinding prev;

        while (bind != null)
        {
            prev = bind;
            bind = bind.next;
            idcFreeBinding(prev);
        }
    }

    // xml_xsd10_free_idc_hash_entry
    /** Port of xmlFreeIDCHashEntry. */
    static void freeIdcHashEntry(Object payload, String name)
    {
        /* Memory is managed by the garbage collector. */
    }

    // xml_xsd10_idc_free_matcher_list
    /** Port of xmlSchemaIDCFreeMatcherList. Frees a list of IDC matchers. */
    static void idcFreeMatcherList(Xsd10IDCMatcher matcher)
    {
        Xsd10IDCMatcher next;

        while (matcher != null)
        {
            next = matcher.next;
            matcher.keySeqs = null;
            if (matcher.targets != null)
            {
                itemListFree(matcher.targets);
            }
            if (matcher.htab != null)
            {
                hashFree(matcher.htab, Xsd10Valid::freeIdcHashEntry);
            }
            matcher = next;
        }
    }

    // xml_xsd10_idc_release_matcher_list
    /** Port of xmlSchemaIDCReleaseMatcherList. Caches a list of IDC matchers for reuse. */
    static void idcReleaseMatcherList(Xsd10ValidCtxt vctxt, Xsd10IDCMatcher matcher)
    {
        Xsd10IDCMatcher next;

        while (matcher != null)
        {
            next = matcher.next;
            if (matcher.keySeqs != null)
            {
                /*
                * Don't free the array, but only the content.
                */
                for (int i = 0; i < matcher.sizeKeySeqs; i++)
                {
                    matcher.keySeqs[i] = null;
                }
            }
            if (matcher.targets != null)
            {
                itemListFree(matcher.targets);
                matcher.targets = null;
            }
            if (matcher.htab != null)
            {
                hashFree(matcher.htab, Xsd10Valid::freeIdcHashEntry);
                matcher.htab = null;
            }
            matcher.next = null;
            /*
            * Cache the matcher.
            */
            if (vctxt.idcMatcherCache != null)
            {
                matcher.nextCached = vctxt.idcMatcherCache;
            }
            vctxt.idcMatcherCache = matcher;

            matcher = next;
        }
    }

    // xml_xsd10_idc_add_state_object
    /** Port of xmlSchemaIDCAddStateObject. */
    static int idcAddStateObject(Xsd10ValidCtxt vctxt, Xsd10IDCMatcher matcher, Xsd10IDCSelect sel, int type)
    {
        Xsd10IDCStateObj sto;

        /*
        * Reuse the state objects from the pool.
        */
        if (vctxt.xpathStatePool != null)
        {
            sto = vctxt.xpathStatePool;
            vctxt.xpathStatePool = sto.next;
            sto.next = null;
        }
        else
        {
            /*
            * Create a new state object.
            */
            sto = new Xsd10IDCStateObj();
        }
        /*
        * Add to global list.
        */
        if (vctxt.xpathStates != null)
        {
            sto.next = vctxt.xpathStates;
        }
        vctxt.xpathStates = sto;

        /*
        * Free the old xpath validation context.
        */
        if (sto.xpathCtxt != null)
        {
            XmlPattern.streamCtxtFree((XmlStreamCtxt) sto.xpathCtxt);
        }

        /*
        * Create a new XPath (pattern) validation context.
        */
        sto.xpathCtxt = XmlPattern.patternGetStreamCtxt((XmlPattern) sel.xpathComp);
        if (sto.xpathCtxt == null)
        {
            verrorInt(vctxt, "xmlSchemaIDCAddStateObject", "failed to create an XPath validation context");
            return -1;
        }
        sto.type = type;
        sto.depth = vctxt.depth;
        sto.matcher = matcher;
        sto.sel = sel;
        sto.nbHistory = 0;

        return 0;
    }

    // xml_xsd10_x_path_evaluate
    /** Port of xmlSchemaXPathEvaluate. Evaluates all active XPath state objects. */
    static int xPathEvaluate(Xsd10ValidCtxt vctxt, int nodeType)
    {
        Xsd10IDCStateObj sto;
        Xsd10IDCStateObj head = null;
        Xsd10IDCStateObj first;
        int res;
        int resolved = 0;
        int depth = vctxt.depth;

        if (vctxt.xpathStates == null)
        {
            return 0;
        }

        if (nodeType == ATTRIBUTE)
        {
            depth++;
        }
        /*
        * Process all active XPath state objects.
        */
        first = vctxt.xpathStates;
        sto = first;
        while (sto != head)
        {
            if (nodeType == ELEMENT)
            {
                res = XmlPattern.streamPush((XmlStreamCtxt) sto.xpathCtxt, vctxt.inode.localName, vctxt.inode.nsName);
            }
            else
            {
                res = XmlPattern.streamPushAttr((XmlStreamCtxt) sto.xpathCtxt, vctxt.inode.localName, vctxt.inode.nsName);
            }

            if (res == -1)
            {
                verrorInt(vctxt, "xmlSchemaXPathEvaluate", "calling xmlStreamPush()");
                return -1;
            }
            if (res != 0)
            {
                /*
                * Full match.
                */
                /*
                * Register a match in the state object history.
                */
                if (sto.sizeHistory <= sto.nbHistory)
                {
                    int newSize = growCapacity(sto.sizeHistory, 4, 5, XML_XSD10_MAX_ITEMS);
                    if (newSize < 0)
                    {
                        vErrMemory(vctxt);
                        return -1;
                    }
                    sto.history = (sto.history == null) ? new int[newSize] : Arrays.copyOf(sto.history, newSize);
                    sto.sizeHistory = newSize;
                }
                sto.history[sto.nbHistory++] = depth;

                if (sto.type == XSD10_XPATH_STATE_OBJ_TYPE_IDC_SELECTOR)
                {
                    Xsd10IDCSelect sel;
                    /*
                    * Activate state objects for the IDC fields of
                    * the IDC selector.
                    */
                    sel = sto.matcher.aidc.def.fields;
                    while (sel != null)
                    {
                        if (idcAddStateObject(vctxt, sto.matcher, sel, XSD10_XPATH_STATE_OBJ_TYPE_IDC_FIELD) == -1)
                        {
                            return -1;
                        }
                        sel = sel.next;
                    }
                }
                else if (sto.type == XSD10_XPATH_STATE_OBJ_TYPE_IDC_FIELD)
                {
                    /*
                    * An IDC key node was found by the IDC field.
                    */
                    /*
                    * Notify that the character value of this node is
                    * needed.
                    */
                    if (resolved == 0)
                    {
                        if ((vctxt.inode.flags & XML_XSD10_SCHEMA_NODE_INFO_VALUE_NEEDED) == 0)
                        {
                            vctxt.inode.flags |= XML_XSD10_SCHEMA_NODE_INFO_VALUE_NEEDED;
                        }
                    }
                    resolved++;
                }
            }
            // next_sto:
            if (sto.next == null)
            {
                /*
                * Evaluate field state objects created on this node as well.
                */
                head = first;
                sto = vctxt.xpathStates;
            }
            else
            {
                sto = sto.next;
            }
        }
        return resolved;
    }

    // xml_xsd10_format_idc_key_sequence_1
    /** Port of xmlSchemaFormatIDCKeySequence_1. */
    static String formatIdcKeySequence1(Xsd10ValidCtxt vctxt, String[] buf, Xsd10PSVIIDCKey[] seq, int count, int for_hash)
    {
        int i;
        int res;
        String[] value = new String[] { null };

        buf[0] = strdup("[");
        for (i = 0; i < count; i++)
        {
            buf[0] = strcat(buf[0], "'");
            if (for_hash == 0)
            {
                res = getCanonValueWhtspExt(seq[i].val, getWhiteSpaceFacetValue(seq[i].type), value);
            }
            else
            {
                res = getCanonValueHash(seq[i].val, value);
            }
            if (res == 0)
            {
                buf[0] = strcat(buf[0], value[0]);
            }
            else
            {
                verrorInt(vctxt, "xmlSchemaFormatIDCKeySequence", "failed to compute a canonical value");
                buf[0] = strcat(buf[0], "???");
            }
            if (i < count - 1)
            {
                buf[0] = strcat(buf[0], "', ");
            }
            else
            {
                buf[0] = strcat(buf[0], "'");
            }
            value[0] = null;
        }
        buf[0] = strcat(buf[0], "]");

        return buf[0];
    }

    // xml_xsd10_format_idc_key_sequence
    /** Port of xmlSchemaFormatIDCKeySequence. */
    static String formatIdcKeySequence(Xsd10ValidCtxt vctxt, String[] buf, Xsd10PSVIIDCKey[] seq, int count)
    {
        return formatIdcKeySequence1(vctxt, buf, seq, count, 0);
    }

    // xml_xsd10_hash_key_sequence
    /** Port of xmlSchemaHashKeySequence. */
    static String hashKeySequence(Xsd10ValidCtxt vctxt, String[] buf, Xsd10PSVIIDCKey[] seq, int count)
    {
        return formatIdcKeySequence1(vctxt, buf, seq, count, 1);
    }

    // xml_xsd10_x_path_pop
    /** Port of xmlSchemaXPathPop. Pops all XPath states. */
    static int xPathPop(Xsd10ValidCtxt vctxt)
    {
        Xsd10IDCStateObj sto;
        int res;

        if (vctxt.xpathStates == null)
        {
            return 0;
        }
        sto = vctxt.xpathStates;
        do
        {
            res = XmlPattern.streamPop((XmlStreamCtxt) sto.xpathCtxt);
            if (res == -1)
            {
                return -1;
            }
            sto = sto.next;
        } while (sto != null);
        return 0;
    }

    // xml_xsd10_x_path_process_history
    /** Port of xmlSchemaXPathProcessHistory. Processes and pops the history items of the IDC state objects. */
    static int xPathProcessHistory(Xsd10ValidCtxt vctxt, int depth)
    {
        Xsd10IDCStateObj sto;
        Xsd10IDCStateObj nextsto;
        int res;
        int matchDepth;
        Xsd10PSVIIDCKey key = null;
        Xsd10Type type = vctxt.inode.typeDef;
        Xsd10Type simpleType = null;

        if (vctxt.xpathStates == null)
        {
            return 0;
        }
        sto = vctxt.xpathStates;

        /*
        * Evaluate the state objects.
        */
        while (sto != null)
        {
            res = XmlPattern.streamPop((XmlStreamCtxt) sto.xpathCtxt);
            if (res == -1)
            {
                verrorInt(vctxt, "xmlSchemaXPathProcessHistory", "calling xmlStreamPop()");
                return -1;
            }
            deregister_check:
            {
                if (sto.nbHistory == 0)
                {
                    break deregister_check;
                }

                matchDepth = sto.history[sto.nbHistory - 1];

                /*
                * Only matches at the current depth are of interest.
                */
                if (matchDepth != depth)
                {
                    sto = sto.next;
                    continue;
                }
                if (sto.type == XSD10_XPATH_STATE_OBJ_TYPE_IDC_FIELD)
                {
                    /*
                    * NOTE: According to
                    *   http://www.w3.org/Bugs/Public/show_bug.cgi?id=2198
                    *   ... the simple-content of complex types is also allowed.
                    */

                    if (wxsIsComplex(type) != 0)
                    {
                        if (wxsHasSimpleContent(type) != 0)
                        {
                            /*
                            * Sanity check for complex types with simple content.
                            */
                            simpleType = type.contentTypeDef;
                            if (simpleType == null)
                            {
                                verrorInt(vctxt, "xmlSchemaXPathProcessHistory",
                                    "field resolves to a CT with simple content " +
                                    "but the CT is missing the ST definition");
                                return -1;
                            }
                        }
                        else
                        {
                            simpleType = null;
                        }
                    }
                    else
                    {
                        simpleType = type;
                    }
                    if (simpleType == null)
                    {
                        String[] str = new String[] { null };

                        /*
                        * Not qualified if the field resolves to a node of non
                        * simple type.
                        */
                        customErr(vctxt, XML_XSD10_SCHEMAV_CVC_IDC, null,
                            sto.matcher.aidc.def,
                            "The XPath '%s' of a field of %s does evaluate to a node of " +
                            "non-simple type",
                            sto.sel.xpath,
                            getIdcDesignation(str, sto.matcher.aidc.def));
                        sto.nbHistory--;
                        break deregister_check;
                    }

                    if ((key == null) && (vctxt.inode.val == null))
                    {
                        /*
                        * Failed to provide the normalized value; maybe
                        * the value was invalid.
                        */
                        verror(vctxt, XML_XSD10_SCHEMAV_CVC_IDC,
                            sto.matcher.aidc.def,
                            "Warning: No precomputed value available, the value " +
                            "was either invalid or something strange happened");
                        sto.nbHistory--;
                        break deregister_check;
                    }
                    else
                    {
                        Xsd10IDCMatcher matcher = sto.matcher;
                        Xsd10PSVIIDCKey[] keySeq;
                        int pos;
                        int idx;

                        /*
                        * The key will be anchored on the matcher's list of
                        * key-sequences. The position in this list is determined
                        * by the target node's depth relative to the matcher's
                        * depth of creation (i.e. the depth of the scope element).
                        * The size of the list is only dependent on the depth of
                        * the tree.
                        * An entry will be NULLed in selector_leave, i.e. when
                        * we hit the target's
                        */
                        pos = sto.depth - matcher.depth;
                        idx = sto.sel.index;

                        /*
                        * Create/grow the array of key-sequences.
                        */
                        if (pos >= matcher.sizeKeySeqs)
                        {
                            int newSize = growCapacity(pos, PTR_SIZE, 10, XML_XSD10_MAX_ITEMS);
                            if (newSize < 0)
                            {
                                vErrMemory(vctxt);
                                return -1;
                            }
                            /* The array needs to be NULLed (copyOf does). */
                            matcher.keySeqs = (matcher.keySeqs == null) ? new Xsd10PSVIIDCKey[newSize][] : Arrays.copyOf(matcher.keySeqs, newSize);
                            matcher.sizeKeySeqs = newSize;
                        }

                        /*
                        * Get/create the key-sequence.
                        */
                        keySeq = matcher.keySeqs[pos];
                        if (keySeq == null)
                        {
                            // create_sequence:
                            /*
                            * Create a key-sequence.
                            */
                            keySeq = new Xsd10PSVIIDCKey[Math.max(0, matcher.aidc.def.nbFields)];
                            matcher.keySeqs[pos] = keySeq;
                        }
                        else if ((idx < keySeq.length) && (keySeq[idx] != null))
                        {
                            String[] str = new String[] { null };
                            /*
                            * cvc-identity-constraint:
                            * 3 For each node in the `target node set` all
                            * of the {fields}, with that node as the context
                            * node, evaluate to either an empty node-set or
                            * a node-set with exactly one member, which must
                            * have a simple type.
                            *
                            * The key was already set; report an error.
                            */
                            customErr(vctxt, XML_XSD10_SCHEMAV_CVC_IDC, null,
                                matcher.aidc.def,
                                "The XPath '%s' of a field of %s evaluates to a " +
                                "node-set with more than one member",
                                sto.sel.xpath,
                                getIdcDesignation(str, matcher.aidc.def));
                            sto.nbHistory--;
                            break deregister_check;
                        }
                        // create_key:
                        /*
                        * Create a key once per node only.
                        */
                        if (key == null)
                        {
                            key = new Xsd10PSVIIDCKey();
                            /*
                            * Consume the compiled value.
                            */
                            key.type = simpleType;
                            key.val = vctxt.inode.val;
                            vctxt.inode.val = null;
                            /*
                            * Store the key in a global list.
                            */
                            if (idcStoreKey(vctxt, key) == -1)
                            {
                                idcFreeKey(key);
                                return -1;
                            }
                        }
                        if (idx < keySeq.length)
                        {
                            keySeq[idx] = key;
                        }
                    }
                }
                else if (sto.type == XSD10_XPATH_STATE_OBJ_TYPE_IDC_SELECTOR)
                {
                    /* keySeq of C: &(matcher->keySeqs[pos]), valid when keySeqSet. */
                    boolean keySeqSet = false;
                    Xsd10PSVIIDCNode ntItem;
                    Xsd10IDCMatcher matcher;
                    Xsd10IDC idc;
                    Xsd10ItemList targets;
                    int pos;
                    int i;
                    int j;
                    int nbKeys;
                    boolean keyError = false;
                    /*
                    * Here we have the following scenario:
                    * An IDC 'selector' state object resolved to a target node,
                    * during the time this target node was in the
                    * ancestor-or-self axis, the 'field' state object(s) looked
                    * out for matching nodes to create a key-sequence for this
                    * target node. Now we are back to this target node and need
                    * to put the key-sequence, together with the target node
                    * itself, into the node-table of the corresponding IDC
                    * binding.
                    */
                    matcher = sto.matcher;
                    idc = matcher.aidc.def;
                    nbKeys = idc.nbFields;
                    pos = depth - matcher.depth;
                    selector:
                    {
                        /*
                        * Check if the matcher has any key-sequences at all, plus
                        * if it has a key-sequence for the current target node.
                        */
                        if ((matcher.keySeqs == null) || (matcher.sizeKeySeqs <= pos) || (pos < 0))
                        {
                            keyError = (idc.type == XML_XSD10_SCHEMA_TYPE_IDC_KEY);
                            break selector;
                        }

                        keySeqSet = true;
                        Xsd10PSVIIDCKey[] ks = matcher.keySeqs[pos];
                        if (ks == null)
                        {
                            keyError = (idc.type == XML_XSD10_SCHEMA_TYPE_IDC_KEY);
                            break selector;
                        }

                        for (i = 0; i < nbKeys; i++)
                        {
                            if ((i >= ks.length) || (ks[i] == null))
                            {
                                /*
                                * Not qualified, if not all fields did resolve.
                                */
                                if (idc.type == XML_XSD10_SCHEMA_TYPE_IDC_KEY)
                                {
                                    /*
                                    * All fields of a "key" IDC must resolve.
                                    */
                                    keyError = true;
                                }
                                break selector;
                            }
                        }
                        /*
                        * All fields did resolve.
                        */

                        /*
                        * 4.1 If the {identity-constraint category} is unique(/key),
                        * then no two members of the `qualified node set` have
                        * `key-sequences` whose members are pairwise equal, as
                        * defined by Equal in [XML Schemas: Datatypes].
                        *
                        * Get the IDC binding from the matcher and check for
                        * duplicate key-sequences.
                        */
                        targets = idcAcquireTargetList(vctxt, matcher);
                        if (targets == null)
                        {
                            return -1;
                        }

                        if ((idc.type != XML_XSD10_SCHEMA_TYPE_IDC_KEYREF) && (targets.nbItems != 0))
                        {
                            Xsd10PSVIIDCKey ckey;
                            Xsd10PSVIIDCKey bkey;
                            Xsd10PSVIIDCKey[] bkeySeq;
                            Xsd10IDCHashEntry e;

                            res = 0;

                            if (matcher.htab == null)
                            {
                                e = null;
                            }
                            else
                            {
                                String[] value = new String[] { null };
                                hashKeySequence(vctxt, value, ks, nbKeys);
                                e = (Xsd10IDCHashEntry) hashLookup(matcher.htab, value[0]);
                            }

                            /*
                            * Compare the key-sequences, key by key.
                            */
                            for (; e != null; e = e.next)
                            {
                                bkeySeq = ((Xsd10PSVIIDCNode) targets.items[e.index]).keys;
                                for (j = 0; j < nbKeys; j++)
                                {
                                    ckey = ks[j];
                                    bkey = bkeySeq[j];
                                    res = areValuesEqual(ckey.val, bkey.val);
                                    if (res == -1)
                                    {
                                        return -1;
                                    }
                                    else if (res == 0)
                                    {
                                        /*
                                        * One of the keys differs, so the key-sequence
                                        * won't be equal; get out.
                                        */
                                        break;
                                    }
                                }
                                if (res == 1)
                                {
                                    /*
                                    * Duplicate key-sequence found.
                                    */
                                    break;
                                }
                            }
                            if (e != null)
                            {
                                String[] str = new String[] { null };
                                String[] strB = new String[] { null };
                                /*
                                * TODO: Try to report the key-sequence.
                                */
                                customErr(vctxt, XML_XSD10_SCHEMAV_CVC_IDC, null,
                                    idc,
                                    "Duplicate key-sequence %s in %s",
                                    formatIdcKeySequence(vctxt, str, ks, nbKeys),
                                    getIdcDesignation(strB, idc));
                                break selector;
                            }
                        }
                        /*
                        * Add a node-table item to the IDC binding.
                        */
                        ntItem = new Xsd10PSVIIDCNode();

                        /*
                        * Store the node-table item in a global list.
                        */
                        if (idc.type != XML_XSD10_SCHEMA_TYPE_IDC_KEYREF)
                        {
                            if (idcStoreNodeTableItem(vctxt, ntItem) == -1)
                            {
                                matcher.keySeqs[pos] = null;
                                return -1;
                            }
                            ntItem.nodeQNameID = -1;
                        }
                        else
                        {
                            /*
                            * Save a cached QName for this node on the IDC node, to be
                            * able to report it, even if the node is not saved.
                            */
                            ntItem.nodeQNameID = vAddNodeQName(vctxt, vctxt.inode.localName, vctxt.inode.nsName);
                            if (ntItem.nodeQNameID == -1)
                            {
                                matcher.keySeqs[pos] = null;
                                return -1;
                            }
                        }
                        /*
                        * Init the node-table item: Save the node, position and
                        * consume the key-sequence.
                        */
                        ntItem.node = vctxt.node;
                        ntItem.nodeLine = vctxt.inode.nodeLine;
                        ntItem.keys = ks;
                        matcher.keySeqs[pos] = null;
                        if (itemListAdd(targets, ntItem) == -1)
                        {
                            return -1;
                        }
                        if (idc.type != XML_XSD10_SCHEMA_TYPE_IDC_KEYREF)
                        {
                            String[] value = new String[] { null };
                            Xsd10IDCHashEntry r;
                            Xsd10IDCHashEntry e;
                            if (matcher.htab == null)
                            {
                                matcher.htab = hashCreate(4);
                            }
                            hashKeySequence(vctxt, value, ntItem.keys, nbKeys);
                            e = new Xsd10IDCHashEntry();
                            e.index = targets.nbItems - 1;
                            r = (Xsd10IDCHashEntry) hashLookup(matcher.htab, value[0]);
                            if (r != null)
                            {
                                e.next = r.next;
                                r.next = e;
                            }
                            else
                            {
                                e.next = null;
                                if (hashAddEntry(matcher.htab, value[0], e) < 0)
                                {
                                    vErrMemory(vctxt);
                                }
                            }
                        }
                        // goto selector_leave
                    }
                    if (keyError)
                    {
                        // selector_key_error:
                        String[] str = new String[] { null };
                        /*
                        * 4.2.1 (KEY) The `target node set` and the
                        * `qualified node set` are equal, that is, every
                        * member of the `target node set` is also a member
                        * of the `qualified node set` and vice versa.
                        */
                        customErr(vctxt, XML_XSD10_SCHEMAV_CVC_IDC, null,
                            idc,
                            "Not all fields of %s evaluate to a node",
                            getIdcDesignation(str, idc), null);
                    }
                    // selector_leave:
                    /*
                    * Free the key-sequence if not added to the IDC table.
                    */
                    if (keySeqSet && (matcher.keySeqs[pos] != null))
                    {
                        matcher.keySeqs[pos] = null;
                    }
                } /* if selector */

                sto.nbHistory--;
            }
            // deregister_check:
            /*
            * Deregister state objects if they reach the depth of creation.
            */
            if ((sto.nbHistory == 0) && (sto.depth == depth))
            {
                if (vctxt.xpathStates != sto)
                {
                    verrorInt(vctxt, "xmlSchemaXPathProcessHistory",
                        "The state object to be removed is not the first " +
                        "in the list");
                }
                nextsto = sto.next;
                /*
                * Unlink from the list of active XPath state objects.
                */
                vctxt.xpathStates = sto.next;
                sto.next = vctxt.xpathStatePool;
                /*
                * Link it to the pool of reusable state objects.
                */
                vctxt.xpathStatePool = sto;
                sto = nextsto;
            }
            else
            {
                sto = sto.next;
            }
        } /* while (sto != NULL) */
        return 0;
    }

    // xml_xsd10_idc_register_matchers
    /** Port of xmlSchemaIDCRegisterMatchers. Creates helper objects to evaluate IDC selectors/fields successively. */
    static int idcRegisterMatchers(Xsd10ValidCtxt vctxt, Xsd10Element elemDecl)
    {
        Xsd10IDCMatcher matcher;
        Xsd10IDCMatcher last = null;
        Xsd10IDC idc;
        Xsd10IDC refIdc;
        Xsd10IDCAug aidc;

        idc = (Xsd10IDC) elemDecl.idcs;
        if (idc == null)
        {
            return 0;
        }

        if (vctxt.inode.idcMatchers != null)
        {
            verrorInt(vctxt, "xmlSchemaIDCRegisterMatchers", "The chain of IDC matchers is expected to be empty");
            return -1;
        }
        do
        {
            if (idc.type == XML_XSD10_SCHEMA_TYPE_IDC_KEYREF)
            {
                /*
                * Since IDCs bubbles are expensive we need to know the
                * depth at which the bubbles should stop; this will be
                * the depth of the top-most keyref IDC. If no keyref
                * references a key/unique IDC, the keyrefDepth will
                * be -1, indicating that no bubbles are needed.
                */
                refIdc = (idc.ref.item instanceof Xsd10IDC) ? (Xsd10IDC) idc.ref.item : null;
                if (refIdc != null)
                {
                    /*
                    * Remember that we have keyrefs on this node.
                    */
                    vctxt.inode.hasKeyrefs = 1;
                    /*
                    * Lookup the referenced augmented IDC info.
                    */
                    aidc = vctxt.aidcs;
                    while (aidc != null)
                    {
                        if (aidc.def == refIdc)
                        {
                            break;
                        }
                        aidc = aidc.next;
                    }
                    if (aidc == null)
                    {
                        verrorInt(vctxt, "xmlSchemaIDCRegisterMatchers",
                            "Could not find an augmented IDC item for an IDC " +
                            "definition");
                        return -1;
                    }
                    if ((aidc.keyrefDepth == -1) || (vctxt.depth < aidc.keyrefDepth))
                    {
                        aidc.keyrefDepth = vctxt.depth;
                    }
                }
            }
            /*
            * Lookup the augmented IDC item for the IDC definition.
            */
            aidc = vctxt.aidcs;
            while (aidc != null)
            {
                if (aidc.def == idc)
                {
                    break;
                }
                aidc = aidc.next;
            }
            if (aidc == null)
            {
                verrorInt(vctxt, "xmlSchemaIDCRegisterMatchers",
                    "Could not find an augmented IDC item for an IDC definition");
                return -1;
            }
            /*
            * Create an IDC matcher for every IDC definition.
            */
            if (vctxt.idcMatcherCache != null)
            {
                /*
                * Reuse a cached matcher.
                */
                matcher = vctxt.idcMatcherCache;
                vctxt.idcMatcherCache = matcher.nextCached;
                matcher.nextCached = null;
            }
            else
            {
                matcher = new Xsd10IDCMatcher();
            }
            if (last == null)
            {
                vctxt.inode.idcMatchers = matcher;
            }
            else
            {
                last.next = matcher;
            }
            last = matcher;

            matcher.type = XSD10_IDC_MATCHER;
            matcher.depth = vctxt.depth;
            matcher.aidc = aidc;
            matcher.idcType = aidc.def.type;
            /*
            * Init the automaton state object.
            */
            if (idcAddStateObject(vctxt, matcher, idc.selector, XSD10_XPATH_STATE_OBJ_TYPE_IDC_SELECTOR) == -1)
            {
                return -1;
            }

            idc = idc.next;
        } while (idc != null);
        return 0;
    }

    /** Equality of two IDC key sequences of nbFields keys (helper for the repeated C loops). */
    private static Xsd10PSVIIDCNode idcNodeAt(Object[] items, int i)
    {
        return (Xsd10PSVIIDCNode) items[i];
    }

    // xml_xsd10_idc_fill_node_tables
    /** Port of xmlSchemaIDCFillNodeTables. */
    static int idcFillNodeTables(Xsd10ValidCtxt vctxt, Xsd10NodeInfo ielem)
    {
        Xsd10PSVIIDCBinding bind;
        int res;
        int i;
        int j;
        int k;
        int nbTargets;
        int nbFields;
        int nbDupls;
        int nbNodeTable;
        Xsd10PSVIIDCKey[] keys;
        Xsd10PSVIIDCKey[] ntkeys;
        Object[] targets;
        Object[] dupls;

        Xsd10IDCMatcher matcher = ielem.idcMatchers;
        /* vctxt->createIDCNodeTables */
        while (matcher != null)
        {
            /*
            * Skip keyref IDCs and empty IDC target-lists.
            */
            if ((matcher.aidc.def.type == XML_XSD10_SCHEMA_TYPE_IDC_KEYREF) ||
                (wxsIlistIsEmpty(matcher.targets) != 0))
            {
                matcher = matcher.next;
                continue;
            }
            /*
            * If we _want_ the IDC node-table to be created in any case
            * then do so. Otherwise create them only if keyrefs need them.
            */
            if ((vctxt.createIDCNodeTables == 0) &&
                ((matcher.aidc.keyrefDepth == -1) ||
                 (matcher.aidc.keyrefDepth > vctxt.depth)))
            {
                matcher = matcher.next;
                continue;
            }
            /*
            * Get/create the IDC binding on this element for the IDC definition.
            */
            bind = idcAcquireBinding(vctxt, matcher);
            if (bind == null)
            {
                return -1;
            }

            if (wxsIlistIsEmpty(bind.dupls) == 0)
            {
                dupls = bind.dupls.items;
                nbDupls = bind.dupls.nbItems;
            }
            else
            {
                dupls = null;
                nbDupls = 0;
            }
            if (bind.nodeTable != null)
            {
                nbNodeTable = bind.nbNodes;
            }
            else
            {
                nbNodeTable = 0;
            }

            if ((nbNodeTable == 0) && (nbDupls == 0))
            {
                /*
                * Transfer all IDC target-nodes to the IDC node-table.
                */
                Object[] titems = matcher.targets.items;
                int tsize = matcher.targets.sizeItems;
                Xsd10PSVIIDCNode[] nt = new Xsd10PSVIIDCNode[Math.max(tsize, matcher.targets.nbItems)];
                for (int t = 0; t < matcher.targets.nbItems; t++)
                {
                    nt[t] = (Xsd10PSVIIDCNode) titems[t];
                }
                bind.nodeTable = nt;
                bind.sizeNodes = tsize;
                bind.nbNodes = matcher.targets.nbItems;

                matcher.targets.items = null;
                matcher.targets.sizeItems = 0;
                matcher.targets.nbItems = 0;
                if (matcher.htab != null)
                {
                    hashFree(matcher.htab, Xsd10Valid::freeIdcHashEntry);
                    matcher.htab = null;
                }
            }
            else
            {
                /*
                * Compare the key-sequences and add to the IDC node-table.
                */
                nbTargets = matcher.targets.nbItems;
                targets = matcher.targets.items;
                nbFields = matcher.aidc.def.nbFields;
                i = 0;
                do
                {
                    next_target:
                    {
                        keys = idcNodeAt(targets, i).keys;
                        if (nbDupls != 0)
                        {
                            /*
                            * Search in already found duplicates first.
                            */
                            j = 0;
                            do
                            {
                                if (nbFields == 1)
                                {
                                    res = areValuesEqual(keys[0].val, idcNodeAt(dupls, j).keys[0].val);
                                    if (res == -1)
                                    {
                                        return -1;
                                    }
                                    if (res == 1)
                                    {
                                        /*
                                        * Equal key-sequence.
                                        */
                                        break next_target;
                                    }
                                }
                                else
                                {
                                    res = 0;
                                    ntkeys = idcNodeAt(dupls, j).keys;
                                    for (k = 0; k < nbFields; k++)
                                    {
                                        res = areValuesEqual(keys[k].val, ntkeys[k].val);
                                        if (res == -1)
                                        {
                                            return -1;
                                        }
                                        if (res == 0)
                                        {
                                            /*
                                            * One of the keys differs.
                                            */
                                            break;
                                        }
                                    }
                                    if (res == 1)
                                    {
                                        /*
                                        * Equal key-sequence found.
                                        */
                                        break next_target;
                                    }
                                }
                                j++;
                            } while (j < nbDupls);
                        }
                        if (bind.nbNodes != 0)
                        {
                            j = 0;
                            do
                            {
                                next_node_table_entry:
                                {
                                    if (nbFields == 1)
                                    {
                                        res = areValuesEqual(keys[0].val, bind.nodeTable[j].keys[0].val);
                                        if (res == -1)
                                        {
                                            return -1;
                                        }
                                        if (res == 0)
                                        {
                                            /*
                                            * The key-sequence differs.
                                            */
                                            break next_node_table_entry;
                                        }
                                    }
                                    else
                                    {
                                        res = 0;
                                        ntkeys = bind.nodeTable[j].keys;
                                        for (k = 0; k < nbFields; k++)
                                        {
                                            res = areValuesEqual(keys[k].val, ntkeys[k].val);
                                            if (res == -1)
                                            {
                                                return -1;
                                            }
                                            if (res == 0)
                                            {
                                                /*
                                                * One of the keys differs.
                                                */
                                                break next_node_table_entry;
                                            }
                                        }
                                    }
                                    /*
                                    * Add the duplicate to the list of duplicates.
                                    */
                                    if (bind.dupls == null)
                                    {
                                        bind.dupls = itemListCreate();
                                        if (bind.dupls == null)
                                        {
                                            return -1;
                                        }
                                    }
                                    if (itemListAdd(bind.dupls, bind.nodeTable[j]) == -1)
                                    {
                                        return -1;
                                    }
                                    dupls = bind.dupls.items;
                                    /*
                                    * Remove the duplicate entry from the IDC node-table.
                                    */
                                    bind.nodeTable[j] = bind.nodeTable[bind.nbNodes - 1];
                                    bind.nbNodes--;

                                    break next_target;
                                }
                                // next_node_table_entry:
                                j++;
                            } while (j < bind.nbNodes);
                        }
                        /*
                        * If everything is fine, then add the IDC target-node to
                        * the IDC node-table.
                        */
                        if (idcAppendNodeTableItem(bind, idcNodeAt(targets, i)) == -1)
                        {
                            return -1;
                        }
                    }
                    // next_target:
                    i++;
                } while (i < nbTargets);
            }
            matcher = matcher.next;
        }
        return 0;
    }

    // xml_xsd10_bubble_idc_node_tables
    /**
     * Port of xmlSchemaBubbleIDCNodeTables. Merges IDC bindings of an element at depth into the
     * corresponding IDC bindings of its parent element.
     */
    static int bubbleIdcNodeTables(Xsd10ValidCtxt vctxt)
    {
        Xsd10PSVIIDCBinding bind; /* IDC bindings of the current node. */
        Xsd10NodeInfo parTable; /* holder of the parent IDC bindings (C: &parent->idcTable) */
        Xsd10PSVIIDCBinding parBind = null; /* parent IDC bindings. */
        Xsd10PSVIIDCNode node;
        Xsd10PSVIIDCNode parNode = null;
        Object[] dupls;
        Xsd10PSVIIDCNode[] parNodes;
        Xsd10IDCAug aidc;
        int i;
        int j;
        int k;
        int ret = 0;
        int nbFields;
        int oldNum;
        int oldDupls;

        bind = vctxt.inode.idcTable;
        if (bind == null)
        {
            /* Fine, no table, no bubbles. */
            return 0;
        }

        parTable = vctxt.elemInfos[vctxt.depth - 1];
        /*
        * Walk all bindings; create new or add to existing bindings.
        * Remove duplicate key-sequences.
        */
        while (bind != null)
        {
            next_binding:
            {
                if ((bind.nbNodes == 0) && (wxsIlistIsEmpty(bind.dupls) != 0))
                {
                    break next_binding;
                }
                /*
                * Check if the key/unique IDC table needs to be bubbled.
                */
                if (vctxt.createIDCNodeTables == 0)
                {
                    aidc = vctxt.aidcs;
                    do
                    {
                        if (aidc == null)
                        {
                            break;
                        }
                        if (aidc.def == bind.definition)
                        {
                            if ((aidc.keyrefDepth == -1) || (aidc.keyrefDepth >= vctxt.depth))
                            {
                                break next_binding;
                            }
                            break;
                        }
                        aidc = aidc.next;
                    } while (aidc != null);
                }

                if (parTable != null)
                {
                    parBind = parTable.idcTable;
                }
                /*
                * Search a matching parent binding for the
                * IDC definition.
                */
                while (parBind != null)
                {
                    if (parBind.definition == bind.definition)
                    {
                        break;
                    }
                    parBind = parBind.next;
                }

                if (parBind != null)
                {
                    /*
                    * Compare every node-table entry of the child node,
                    * i.e. the key-sequence within, ...
                    */
                    oldNum = parBind.nbNodes; /* Skip newly added items. */

                    if (wxsIlistIsEmpty(parBind.dupls) == 0)
                    {
                        oldDupls = parBind.dupls.nbItems;
                        dupls = parBind.dupls.items;
                    }
                    else
                    {
                        dupls = null;
                        oldDupls = 0;
                    }

                    parNodes = parBind.nodeTable;
                    nbFields = bind.definition.nbFields;

                    for (i = 0; i < bind.nbNodes; i++)
                    {
                        node = bind.nodeTable[i];
                        if (node == null)
                        {
                            continue;
                        }
                        /*
                        * ...with every key-sequence of the parent node, already
                        * evaluated to be a duplicate key-sequence.
                        */
                        if (oldDupls != 0)
                        {
                            j = 0;
                            while (j < oldDupls)
                            {
                                if (nbFields == 1)
                                {
                                    ret = areValuesEqual(node.keys[0].val, idcNodeAt(dupls, j).keys[0].val);
                                    if (ret == -1)
                                    {
                                        return -1;
                                    }
                                    if (ret == 0)
                                    {
                                        j++;
                                        continue;
                                    }
                                }
                                else
                                {
                                    parNode = idcNodeAt(dupls, j);
                                    for (k = 0; k < nbFields; k++)
                                    {
                                        ret = areValuesEqual(node.keys[k].val, parNode.keys[k].val);
                                        if (ret == -1)
                                        {
                                            return -1;
                                        }
                                        if (ret == 0)
                                        {
                                            break;
                                        }
                                    }
                                }
                                if (ret == 1)
                                {
                                    /* Duplicate found. */
                                    break;
                                }
                                j++;
                            }
                            if (j != oldDupls)
                            {
                                /* Duplicate found. Skip this entry. */
                                continue;
                            }
                        }
                        /*
                        * ... and with every key-sequence of the parent node.
                        */
                        if (oldNum != 0)
                        {
                            j = 0;
                            while (j < oldNum)
                            {
                                parNode = parNodes[j];
                                if (nbFields == 1)
                                {
                                    ret = areValuesEqual(node.keys[0].val, parNode.keys[0].val);
                                    if (ret == -1)
                                    {
                                        return -1;
                                    }
                                    if (ret == 0)
                                    {
                                        j++;
                                        continue;
                                    }
                                }
                                else
                                {
                                    for (k = 0; k < nbFields; k++)
                                    {
                                        ret = areValuesEqual(node.keys[k].val, parNode.keys[k].val);
                                        if (ret == -1)
                                        {
                                            return -1;
                                        }
                                        if (ret == 0)
                                        {
                                            break;
                                        }
                                    }
                                }
                                if (ret == 1)
                                {
                                    /* Duplicate found. */
                                    break;
                                }
                                j++;
                            }
                            if (j != oldNum)
                            {
                                /*
                                * Handle duplicates. Move the duplicate in
                                * the parent's node-table to the list of
                                * duplicates.
                                */
                                oldNum--;
                                parBind.nbNodes--;
                                /*
                                * Move last old item to pos of duplicate.
                                */
                                parNodes[j] = parNodes[oldNum];

                                if (parBind.nbNodes != oldNum)
                                {
                                    /*
                                    * If new items exist, move last new item to
                                    * last of old items.
                                    */
                                    parNodes[oldNum] = parNodes[parBind.nbNodes];
                                }
                                if (parBind.dupls == null)
                                {
                                    parBind.dupls = itemListCreate();
                                    if (parBind.dupls == null)
                                    {
                                        return -1;
                                    }
                                }
                                itemListAdd(parBind.dupls, parNode);
                                dupls = parBind.dupls.items;
                            }
                            else
                            {
                                /*
                                * Add the node-table entry (node and key-sequence) of
                                * the child node to the node table of the parent node.
                                */
                                if (parBind.nbNodes >= parBind.sizeNodes)
                                {
                                    int newSize = growCapacity(parBind.sizeNodes, PTR_SIZE, 10, XML_XSD10_MAX_ITEMS);
                                    if (newSize < 0)
                                    {
                                        vErrMemory(vctxt);
                                        return -1;
                                    }
                                    parBind.nodeTable = (parBind.nodeTable == null) ? new Xsd10PSVIIDCNode[newSize] : Arrays.copyOf(parBind.nodeTable, newSize);
                                    parBind.sizeNodes = newSize;
                                }
                                parNodes = parBind.nodeTable;
                                /*
                                * Append the new node-table entry to the 'new node-table
                                * entries' section.
                                */
                                parNodes[parBind.nbNodes++] = node;
                            }
                        }
                    }
                }
                else
                {
                    /*
                    * No binding for the IDC was found: create a new one and
                    * copy all node-tables.
                    */
                    parBind = idcNewBinding(bind.definition);
                    if (parBind == null)
                    {
                        return -1;
                    }

                    if (bind.nbNodes != 0)
                    {
                        /*
                        * Add all IDC node-table entries.
                        */
                        if (vctxt.psviExposeIDCNodeTables == 0)
                        {
                            /*
                            * Just move the entries.
                            * NOTE: this is quite save here, since
                            * all the keyref lookups have already been
                            * performed.
                            */
                            parBind.nodeTable = bind.nodeTable;
                            bind.nodeTable = null;
                            parBind.sizeNodes = bind.sizeNodes;
                            bind.sizeNodes = 0;
                            parBind.nbNodes = bind.nbNodes;
                            bind.nbNodes = 0;
                        }
                        else
                        {
                            /*
                            * Copy the entries.
                            */
                            parBind.nodeTable = Arrays.copyOf(bind.nodeTable, bind.nbNodes);
                            parBind.sizeNodes = bind.nbNodes;
                            parBind.nbNodes = bind.nbNodes;
                        }
                    }
                    if (bind.dupls != null)
                    {
                        /*
                        * Move the duplicates.
                        */
                        if (parBind.dupls != null)
                        {
                            itemListFree(parBind.dupls);
                        }
                        parBind.dupls = bind.dupls;
                        bind.dupls = null;
                    }
                    if (parTable != null)
                    {
                        if (parTable.idcTable == null)
                        {
                            parTable.idcTable = parBind;
                        }
                        else
                        {
                            parBind.next = parTable.idcTable;
                            parTable.idcTable = parBind;
                        }
                    }
                }
            }
            // next_binding:
            bind = bind.next;
        }
        return 0;
    }

    // xml_xsd10_check_cvcidc_key_ref
    /** Port of xmlSchemaCheckCVCIDCKeyRef. Check the cvc-idc-keyref constraints. */
    static int checkCvcidcKeyRef(Xsd10ValidCtxt vctxt)
    {
        Xsd10IDCMatcher matcher;
        Xsd10PSVIIDCBinding bind;

        matcher = vctxt.inode.idcMatchers;
        /*
        * Find a keyref.
        */
        while (matcher != null)
        {
            if ((matcher.idcType == XML_XSD10_SCHEMA_TYPE_IDC_KEYREF) &&
                (matcher.targets != null) &&
                (matcher.targets.nbItems != 0))
            {
                int i;
                int j;
                int k;
                int res;
                int nbFields;
                int hasDupls;
                Xsd10PSVIIDCKey[] refKeys;
                Xsd10PSVIIDCKey[] keys;
                Xsd10PSVIIDCNode refNode = null;
                Xsd10Hash table = null;

                nbFields = matcher.aidc.def.nbFields;

                /*
                * Find the IDC node-table for the referenced IDC key/unique.
                */
                bind = vctxt.inode.idcTable;
                while (bind != null)
                {
                    if (matcher.aidc.def.ref.item == bind.definition)
                    {
                        break;
                    }
                    bind = bind.next;
                }
                hasDupls = ((bind != null) && (bind.dupls != null) && (bind.dupls.nbItems != 0)) ? 1 : 0;
                /*
                * Search for a matching key-sequences.
                */
                if (bind != null)
                {
                    table = hashCreate(bind.nbNodes * 2);
                    for (j = 0; j < bind.nbNodes; j++)
                    {
                        String[] value = new String[] { null };
                        Xsd10IDCHashEntry r;
                        Xsd10IDCHashEntry e;
                        keys = bind.nodeTable[j].keys;
                        hashKeySequence(vctxt, value, keys, nbFields);
                        e = new Xsd10IDCHashEntry();
                        e.index = j;
                        r = (Xsd10IDCHashEntry) hashLookup(table, value[0]);
                        if (r != null)
                        {
                            e.next = r.next;
                            r.next = e;
                        }
                        else
                        {
                            e.next = null;
                            if (hashAddEntry(table, value[0], e) < 0)
                            {
                                vErrMemory(vctxt);
                            }
                        }
                    }
                }
                for (i = 0; i < matcher.targets.nbItems; i++)
                {
                    res = 0;
                    refNode = (Xsd10PSVIIDCNode) matcher.targets.items[i];
                    if (bind != null)
                    {
                        String[] value = new String[] { null };
                        Xsd10IDCHashEntry e;
                        refKeys = refNode.keys;
                        hashKeySequence(vctxt, value, refKeys, nbFields);
                        e = (Xsd10IDCHashEntry) hashLookup(table, value[0]);
                        res = 0;
                        for (; e != null; e = e.next)
                        {
                            keys = bind.nodeTable[e.index].keys;
                            for (k = 0; k < nbFields; k++)
                            {
                                res = areValuesEqual(keys[k].val, refKeys[k].val);
                                if (res == 0)
                                {
                                    break;
                                }
                                else if (res == -1)
                                {
                                    return -1;
                                }
                            }
                            if (res == 1)
                            {
                                /*
                                 * Match found.
                                 */
                                break;
                            }
                        }
                        if ((res == 0) && (hasDupls != 0))
                        {
                            /*
                            * Search in duplicates
                            */
                            for (j = 0; j < bind.dupls.nbItems; j++)
                            {
                                keys = ((Xsd10PSVIIDCNode) bind.dupls.items[j]).keys;
                                for (k = 0; k < nbFields; k++)
                                {
                                    res = areValuesEqual(keys[k].val, refKeys[k].val);
                                    if (res == 0)
                                    {
                                        break;
                                    }
                                    else if (res == -1)
                                    {
                                        return -1;
                                    }
                                }
                                if (res == 1)
                                {
                                    /*
                                    * Match in duplicates found.
                                    */
                                    String[] str = new String[] { null };
                                    String[] strB = new String[] { null };
                                    keyrefErr(vctxt,
                                        XML_XSD10_SCHEMAV_CVC_IDC, refNode,
                                        matcher.aidc.def,
                                        "More than one match found for " +
                                        "key-sequence %s of keyref '%s'",
                                        formatIdcKeySequence(vctxt, str, refNode.keys, nbFields),
                                        getComponentQName(strB, matcher.aidc.def));
                                    break;
                                }
                            }
                        }
                    }

                    if (res == 0)
                    {
                        String[] str = new String[] { null };
                        String[] strB = new String[] { null };
                        keyrefErr(vctxt,
                            XML_XSD10_SCHEMAV_CVC_IDC, refNode,
                            matcher.aidc.def,
                            "No match found for key-sequence %s of keyref '%s'",
                            formatIdcKeySequence(vctxt, str, refNode.keys, nbFields),
                            getComponentQName(strB, matcher.aidc.def));
                    }
                }
                if (table != null)
                {
                    hashFree(table, Xsd10Valid::freeIdcHashEntry);
                }
            }
            matcher = matcher.next;
        }
        /* TODO: Return an error if any error encountered. */
        return 0;
    }

    /* ***********************************************************************
     *                                                                      *
     *                      XML Reader validation code                      *
     *                                                                      *
     ************************************************************************/

    // xml_xsd10_get_fresh_attr_info
    /** Port of xmlSchemaGetFreshAttrInfo. */
    static Xsd10AttrInfo getFreshAttrInfo(Xsd10ValidCtxt vctxt)
    {
        Xsd10AttrInfo iattr;
        /*
        * Grow/create list of attribute infos.
        */
        if (vctxt.sizeAttrInfos <= vctxt.nbAttrInfos)
        {
            int oldSize = vctxt.sizeAttrInfos;
            int newSize = growCapacity(oldSize, PTR_SIZE, 5, XML_XSD10_MAX_ITEMS);
            if (newSize < 0)
            {
                vErrMemory(vctxt);
                return null;
            }
            vctxt.attrInfos = (vctxt.attrInfos == null) ? new Xsd10AttrInfo[newSize] : Arrays.copyOf(vctxt.attrInfos, newSize);
            vctxt.sizeAttrInfos = newSize;
        }

        iattr = vctxt.attrInfos[vctxt.nbAttrInfos];
        if (iattr != null)
        {
            if (iattr.localName != null)
            {
                verrorInt(vctxt, "xmlSchemaGetFreshAttrInfo", "attr info not cleared");
                return null;
            }
            iattr.nodeType = ATTRIBUTE;
            vctxt.nbAttrInfos++;
            return iattr;
        }

        /*
        * Create an attribute info.
        */
        iattr = new Xsd10AttrInfo();
        iattr.nodeType = ATTRIBUTE;
        vctxt.attrInfos[vctxt.nbAttrInfos++] = iattr;

        return iattr;
    }

    // xml_xsd10_validator_push_attribute
    /** Port of xmlSchemaValidatorPushAttribute. */
    static int validatorPushAttribute(Xsd10ValidCtxt vctxt, XmlNode attrNode, int nodeLine, String localName, String nsName, int ownedNames, String value, int ownedValue)
    {
        Xsd10AttrInfo attr;

        attr = getFreshAttrInfo(vctxt);
        if (attr == null)
        {
            verrorInt(vctxt, "xmlSchemaPushAttribute", "calling xmlSchemaGetFreshAttrInfo()");
            return -1;
        }
        attr.node = attrNode;
        attr.nodeLine = nodeLine;
        attr.state = XML_XSD10_SCHEMAS_ATTR_UNKNOWN;
        attr.localName = localName;
        attr.nsName = nsName;
        if (ownedNames != 0)
        {
            attr.flags |= XML_XSD10_SCHEMA_NODE_INFO_FLAG_OWNED_NAMES;
        }
        /*
        * Evaluate if it's an XSI attribute.
        */
        if (nsName != null)
        {
            if (strEqual(localName, "nil") != 0)
            {
                if (strEqual(attr.nsName, XML_XSD10_INSTANCE_NS) != 0)
                {
                    attr.metaType = XML_XSD10_SCHEMA_ATTR_INFO_META_XSI_NIL;
                }
            }
            else if (strEqual(localName, "type") != 0)
            {
                if (strEqual(attr.nsName, XML_XSD10_INSTANCE_NS) != 0)
                {
                    attr.metaType = XML_XSD10_SCHEMA_ATTR_INFO_META_XSI_TYPE;
                }
            }
            else if (strEqual(localName, "schemaLocation") != 0)
            {
                if (strEqual(attr.nsName, XML_XSD10_INSTANCE_NS) != 0)
                {
                    attr.metaType = XML_XSD10_SCHEMA_ATTR_INFO_META_XSI_SCHEMA_LOC;
                }
            }
            else if (strEqual(localName, "noNamespaceSchemaLocation") != 0)
            {
                if (strEqual(attr.nsName, XML_XSD10_INSTANCE_NS) != 0)
                {
                    attr.metaType = XML_XSD10_SCHEMA_ATTR_INFO_META_XSI_NO_NS_SCHEMA_LOC;
                }
            }
            else if (strEqual(attr.nsName, XML_XSD10_NAMESPACE_NS) != 0)
            {
                attr.metaType = XML_XSD10_SCHEMA_ATTR_INFO_META_XMLNS;
            }
        }
        attr.value = value;
        if (ownedValue != 0)
        {
            attr.flags |= XML_XSD10_SCHEMA_NODE_INFO_FLAG_OWNED_VALUES;
        }
        if (attr.metaType != 0)
        {
            attr.state = XML_XSD10_SCHEMAS_ATTR_META;
        }
        return 0;
    }

    /** C memset(info, 0, sizeof(XmlXsd10NodeInfo)) of the shared node info fields. */
    private static void zeroNodeInfo(Xsd10NodeInfo info)
    {
        info.nodeType = 0;
        info.node = null;
        info.nodeLine = 0;
        info.localName = null;
        info.nsName = null;
        info.value = null;
        info.val = null;
        info.typeDef = null;
        info.flags = 0;
        info.valNeeded = 0;
        info.normVal = 0;
        setDecl(info, null);
        info.depth = 0;
        info.idcTable = null;
        info.idcMatchers = null;
        info.regexCtxt = null;
        info.nsBindings = null;
        info.nbNsBindings = 0;
        info.sizeNsBindings = 0;
        info.hasKeyrefs = 0;
        info.appliedXPath = 0;
    }

    /** C memset(attr, 0, sizeof(XmlXsd10AttrInfo)). */
    private static void zeroAttrInfo(Xsd10AttrInfo attr)
    {
        zeroNodeInfo(attr);
        attr.decl = null;
        attr.use = null;
        attr.state = 0;
        attr.metaType = 0;
        attr.vcValue = null;
        attr.parent = null;
    }

    // xml_xsd10_clear_elem_info
    /** Port of xmlSchemaClearElemInfo. */
    static void clearElemInfo(Xsd10ValidCtxt vctxt, Xsd10NodeInfo ielem)
    {
        ielem.hasKeyrefs = 0;
        ielem.appliedXPath = 0;
        ielem.localName = null;
        ielem.nsName = null;
        ielem.value = null;
        if (ielem.val != null)
        {
            /*
            * PSVI TODO: Be careful not to free it when the value is
            * exposed via PSVI.
            */
            freeValue(ielem.val);
            ielem.val = null;
        }
        if (ielem.idcMatchers != null)
        {
            /*
            * REVISIT OPTIMIZE TODO: Use a pool of IDC matchers.
            *   Does it work?
            */
            idcReleaseMatcherList(vctxt, ielem.idcMatchers);
            ielem.idcMatchers = null;
        }
        if (ielem.idcTable != null)
        {
            /*
            * OPTIMIZE TODO: Use a pool of IDC tables??.
            */
            idcFreeIdcTable(ielem.idcTable);
            ielem.idcTable = null;
        }
        if (ielem.regexCtxt != null)
        {
            XmlRegexp.regExecFree(ielem.regexCtxt);
            ielem.regexCtxt = null;
        }
        if (ielem.nsBindings != null)
        {
            ielem.nsBindings = null;
            ielem.nbNsBindings = 0;
            ielem.sizeNsBindings = 0;
        }
    }

    // xml_xsd10_get_fresh_elem_info
    /** Port of xmlSchemaGetFreshElemInfo. Creates/reuses and initializes the element info item for the current tree depth. */
    static Xsd10NodeInfo getFreshElemInfo(Xsd10ValidCtxt vctxt)
    {
        Xsd10NodeInfo info = null;

        if (vctxt.depth > vctxt.sizeElemInfos)
        {
            verrorInt(vctxt, "xmlSchemaGetFreshElemInfo", "inconsistent depth encountered");
            return null;
        }
        if (vctxt.sizeElemInfos <= vctxt.depth)
        {
            int oldSize = vctxt.sizeElemInfos;
            int newSize = growCapacity(oldSize, PTR_SIZE, 10, XML_XSD10_MAX_ITEMS);
            if (newSize < 0)
            {
                vErrMemory(vctxt);
                return null;
            }
            /*
            * We need the new memory to be NULLed.
            */
            vctxt.elemInfos = (vctxt.elemInfos == null) ? new Xsd10NodeInfo[newSize] : Arrays.copyOf(vctxt.elemInfos, newSize);
            vctxt.sizeElemInfos = newSize;
        }
        else
        {
            info = vctxt.elemInfos[vctxt.depth];
        }

        if (info == null)
        {
            info = new Xsd10NodeInfo();
            vctxt.elemInfos[vctxt.depth] = info;
        }
        else
        {
            if (info.localName != null)
            {
                verrorInt(vctxt, "xmlSchemaGetFreshElemInfo", "elem info has not been cleared");
                return null;
            }
        }
        zeroNodeInfo(info);
        info.nodeType = ELEMENT;
        info.depth = vctxt.depth;

        return info;
    }

    // XSD10_ACTIVATE_ELEM
    private static void activateElem(Xsd10ValidCtxt vctxt)
    {
        vctxt.inode = vctxt.elemInfos[vctxt.depth];
    }

    // XSD10_ACTIVATE_PARENT_ELEM
    private static void activateParentElem(Xsd10ValidCtxt vctxt)
    {
        vctxt.inode = vctxt.elemInfos[vctxt.depth - 1];
    }

    // xml_xsd10_validate_facets
    /** Port of xmlSchemaValidateFacets. */
    static int validateFacets(Xsd10AbstractCtxt actxt, XmlNode node, Xsd10Type type, int valType, String value, Xsd10Val val, long length, int fireErrors)
    {
        int ret;
        int error = 0;
        int found;

        Xsd10Type tmpType;
        Xsd10FacetLink facetLink;
        Xsd10Facet facet;
        long[] len = new long[] { 0 };
        int ws;

        /*
        * In Libxml2, derived built-in types have currently no explicit facets.
        */
        if (type.type == XML_XSD10_SCHEMA_TYPE_BASIC)
        {
            return 0;
        }

        /*
        * NOTE: Do not jump away, if the facetSet of the given type is
        * empty: until now, "pattern" and "enumeration" facets of the
        * *base types* need to be checked as well.
        */
        boolean doAtomic = true;
        boolean doList = true;
        if (type.facetSet == null)
        {
            doAtomic = false;
            doList = false;
        }
        else if (wxsIsAtomic(type) == 0)
        {
            doAtomic = false;
            if (wxsIsList(type) == 0)
            {
                doList = false;
            }
        }

        if (doAtomic)
        {
            /*
            * Whitespace handling is only of importance for string-based
            * types.
            */
            tmpType = getPrimitiveType(type);
            if ((tmpType.builtInType == XML_XSD10_SCHEMAS_STRING) || (wxsIsAnySimpleType(tmpType) != 0))
            {
                ws = getWhiteSpaceFacetValue(type);
            }
            else
            {
                ws = XML_XSD10_SCHEMA_WHITESPACE_COLLAPSE;
            }

            /*
            * If the value was not computed (for string or
            * anySimpleType based types), then use the provided
            * type.
            */
            if (val != null)
            {
                valType = getValType(val);
            }

            ret = 0;
            for (facetLink = type.facetSet; facetLink != null; facetLink = facetLink.next)
            {
                /*
                * Skip the pattern "whiteSpace": it is used to
                * format the character content beforehand.
                */
                switch (facetLink.facet.type)
                {
                    case XML_XSD10_SCHEMA_FACET_WHITESPACE:
                    case XML_XSD10_SCHEMA_FACET_PATTERN:
                    case XML_XSD10_SCHEMA_FACET_ENUMERATION:
                        continue;
                    case XML_XSD10_SCHEMA_FACET_LENGTH:
                    case XML_XSD10_SCHEMA_FACET_MINLENGTH:
                    case XML_XSD10_SCHEMA_FACET_MAXLENGTH:
                        ret = validateLengthFacetWhtsp(facetLink.facet, valType, value, val, len, ws);
                        break;
                    default:
                        ret = validateFacetWhtsp(facetLink.facet, ws, valType, value, val, ws);
                        break;
                }
                if (ret < 0)
                {
                    aerrorInt(actxt, "xmlSchemaValidateFacets", "validating against a atomic type facet");
                    return -1;
                }
                else if (ret > 0)
                {
                    if (fireErrors != 0)
                    {
                        facetErr(actxt, ret, node, value, len[0], type, facetLink.facet, null, null, null);
                    }
                    else
                    {
                        return ret;
                    }
                    if (error == 0)
                    {
                        error = ret;
                    }
                }
                ret = 0;
            }
        }

        // XSD10_WXS_IS_LIST:
        if (doList && (wxsIsList(type) != 0))
        {
            /*
            * "length", "minLength" and "maxLength" of list types.
            */
            ret = 0;
            for (facetLink = type.facetSet; facetLink != null; facetLink = facetLink.next)
            {
                switch (facetLink.facet.type)
                {
                    case XML_XSD10_SCHEMA_FACET_LENGTH:
                    case XML_XSD10_SCHEMA_FACET_MINLENGTH:
                    case XML_XSD10_SCHEMA_FACET_MAXLENGTH:
                        ret = validateListSimpleTypeFacet(facetLink.facet, value, length, null);
                        break;
                    default:
                        continue;
                }
                if (ret < 0)
                {
                    aerrorInt(actxt, "xmlSchemaValidateFacets", "validating against a list type facet");
                    return -1;
                }
                else if (ret > 0)
                {
                    if (fireErrors != 0)
                    {
                        facetErr(actxt, ret, node, value, length, type, facetLink.facet, null, null, null);
                    }
                    else
                    {
                        return ret;
                    }
                    if (error == 0)
                    {
                        error = ret;
                    }
                }
                ret = 0;
            }
        }

        // pattern_and_enum:
        found = 0;
        /*
        * Process enumerations. Facet values are in the value space
        * of the defining type's base type. This seems to be a bug in the
        * XML Schema 1.0 spec. Use the whitespace type of the base type.
        * Only the first set of enumerations in the ancestor-or-self axis
        * is used for validation.
        */
        ret = 0;
        tmpType = type;
        do
        {
            for (facet = tmpType.facets; facet != null; facet = facet.next)
            {
                if (facet.type != XML_XSD10_SCHEMA_FACET_ENUMERATION)
                {
                    continue;
                }
                found = 1;
                ret = areValuesEqual(facet.val, val);
                if (ret == 1)
                {
                    break;
                }
                else if (ret < 0)
                {
                    aerrorInt(actxt, "xmlSchemaValidateFacets", "validating against an enumeration facet");
                    return -1;
                }
            }
            if (ret != 0)
            {
                break;
            }
            /*
            * Break on the first set of enumerations. Any additional
            *  enumerations which might be existent on the ancestors
            *  of the current type are restricted by this set; thus
            *  *must* *not* be taken into account.
            */
            if (found != 0)
            {
                break;
            }
            tmpType = tmpType.baseType;
        } while ((tmpType != null) && (tmpType.type != XML_XSD10_SCHEMA_TYPE_BASIC));
        if ((found != 0) && (ret == 0))
        {
            ret = XML_XSD10_SCHEMAV_CVC_ENUMERATION_VALID;
            if (fireErrors != 0)
            {
                facetErr(actxt, ret, node, value, 0, type, null, null, null, null);
            }
            else
            {
                return ret;
            }
            if (error == 0)
            {
                error = ret;
            }
        }

        /*
        * Process patters. Pattern facets are ORed at type level
        * and ANDed if derived. Walk the base type axis.
        */
        tmpType = type;
        facet = null;
        do
        {
            found = 0;
            for (facetLink = tmpType.facetSet; facetLink != null; facetLink = facetLink.next)
            {
                if (facetLink.facet.type != XML_XSD10_SCHEMA_FACET_PATTERN)
                {
                    continue;
                }
                found = 1;
                /*
                * NOTE that for patterns, @value needs to be the
                * normalized value.
                */
                ret = XmlRegexp.regexpExec((XmlRegexp) facetLink.facet.regexp, value);
                if (ret == 1)
                {
                    break;
                }
                else if (ret < 0)
                {
                    aerrorInt(actxt, "xmlSchemaValidateFacets", "validating against a pattern facet");
                    return -1;
                }
                else
                {
                    /*
                    * Save the last non-validating facet.
                    */
                    facet = facetLink.facet;
                }
            }
            if ((found != 0) && (ret != 1))
            {
                ret = XML_XSD10_SCHEMAV_CVC_PATTERN_VALID;
                if (fireErrors != 0)
                {
                    facetErr(actxt, ret, node, value, 0, type, facet, null, null, null);
                }
                else
                {
                    return ret;
                }
                if (error == 0)
                {
                    error = ret;
                }
                break;
            }
            tmpType = tmpType.baseType;
        } while ((tmpType != null) && (tmpType.type != XML_XSD10_SCHEMA_TYPE_BASIC));

        return error;
    }

    // xml_xsd10_normalize_value
    /** Port of xmlSchemaNormalizeValue. */
    static String normalizeValue(Xsd10Type type, String value)
    {
        switch (getWhiteSpaceFacetValue(type))
        {
            case XML_XSD10_SCHEMA_WHITESPACE_COLLAPSE:
                return collapseString(value);
            case XML_XSD10_SCHEMA_WHITESPACE_REPLACE:
                return whiteSpaceReplace(value);
            default:
                return null;
        }
    }

    // xml_xsd10_validate_q_name
    /** Port of xmlSchemaValidateQName. */
    static int validateQName(Xsd10ValidCtxt vctxt, String value, Xsd10Val[] val, int valNeeded)
    {
        int ret;
        String stripped;
        String nsName;
        String local;
        String[] prefix = new String[] { null };

        ret = treeValidateQname(value, 1);
        if (ret != 0)
        {
            if (ret == -1)
            {
                verrorInt(vctxt, "xmlSchemaValidateQName", "calling xmlValidateQName()");
                return -1;
            }
            return XML_XSD10_SCHEMAV_CVC_DATATYPE_VALID_1_2_1;
        }
        /*
        * NOTE: xmlSplitQName2 will always return a duplicated
        * strings.
        */
        /* TODO: Export and use xmlSchemaStrip instead */
        stripped = collapseString(value);
        local = treeSplitQname2((stripped != null) ? stripped : value, prefix);
        if (local == null)
        {
            local = strdup(value);
        }
        /*
        * OPTIMIZE TODO: Use flags for:
        *  - is there any namespace binding?
        *  - is there a default namespace?
        */
        nsName = lookupNamespace(vctxt, prefix[0]);

        if (prefix[0] != null)
        {
            /*
            * A namespace must be found if the prefix is
            * NOT NULL.
            */
            if (nsName == null)
            {
                ret = XML_XSD10_SCHEMAV_CVC_DATATYPE_VALID_1_2_1;
                customErr(vctxt, ret, null,
                    getBuiltInType(XML_XSD10_SCHEMAS_QNAME),
                    "The QName value '%s' has no " +
                    "corresponding namespace declaration in " +
                    "scope", value, null);
                return ret;
            }
        }
        if ((valNeeded != 0) && (val != null))
        {
            if (nsName != null)
            {
                val[0] = newQnameValue(strdup(nsName), local);
            }
            else
            {
                val[0] = newQnameValue(null, local);
            }
        }
        return 0;
    }

    /*
    * The ID table of the schema document holding node (libxml2 registers xs:ID values of
    * attribute nodes in node->doc->ids): the one of its bucket, created when needed.
    */
    // xsd10_valid_schema_doc_ids
    private static Xsd10Ids validSchemaDocIds(Xsd10ParserCtxt pctxt, XmlNode node)
    {
        XmlDocument doc;
        int i;

        if ((node == null) || (nodeKind(node) != ATTRIBUTE) ||
            (pctxt.constructor == null) || (pctxt.constructor.buckets == null))
        {
            return null;
        }
        doc = nodeDom(node);
        for (i = 0; i < pctxt.constructor.buckets.nbItems; i++)
        {
            Xsd10Bucket bucket = (Xsd10Bucket) pctxt.constructor.buckets.items[i];
            if ((bucket != null) && (bucket.doc == doc))
            {
                if (bucket.ids == null)
                {
                    bucket.ids = idsNew();
                }
                return bucket.ids;
            }
        }
        return null;
    }

    /*
    * cvc-simple-type
    */
    // xml_xsd10_v_check_cvc_simple_type
    /** Port of xmlSchemaVCheckCVCSimpleType. */
    static int vCheckCvcSimpleType(Xsd10AbstractCtxt actxt, XmlNode node, Xsd10Type type, String value, Xsd10Val[] retVal, int fireErrors, int normalize, int isNormalized)
    {
        int ret = 0;
        int valNeeded = (retVal != null) ? 1 : 0;
        Xsd10Val[] val = new Xsd10Val[] { null };
        /* xmlSchemaWhitespaceValueType ws; */
        String normValue = null;

        if ((retVal != null) && (retVal[0] != null))
        {
            freeValue(retVal[0]);
            retVal[0] = null;
        }
        /*
        * 3.14.4 Simple Type Definition Validation Rules
        * Validation Rule: String Valid
        */
        /*
        * 1 It is schema-valid with respect to that definition as defined
        * by Datatype Valid in [XML Schemas: Datatypes].
        */
        /*
        * 2.1 If The definition is ENTITY or is validly derived from ENTITY given
        * the empty set, as defined in Type Derivation OK (Simple) ($3.14.6), then
        * the string must be a `declared entity name`.
        */
        /*
        * 2.2 If The definition is ENTITIES or is validly derived from ENTITIES
        * given the empty set, as defined in Type Derivation OK (Simple) ($3.14.6),
        * then every whitespace-delimited substring of the string must be a `declared
        * entity name`.
        */
        /*
        * 2.3 otherwise no further condition applies.
        */
        if ((valNeeded == 0) && ((type.flags & XML_XSD10_SCHEMAS_TYPE_FACETSNEEDVALUE) != 0))
        {
            valNeeded = 1;
        }
        if (value == null)
        {
            value = "";
        }
        if ((wxsIsAnySimpleType(type) != 0) || (wxsIsAtomic(type) != 0))
        {
            Xsd10Type biType; /* The built-in type. */
            /*
            * SPEC (1.2.1) "if {variety} is `atomic` then the string must `match`
            * a literal in the `lexical space` of {base type definition}"
            */
            /*
            * Whitespace-normalize.
            */
            // XSD10_NORMALIZE(type)
            if ((isNormalized == 0) && ((normalize != 0) || ((type.flags & XML_XSD10_SCHEMAS_TYPE_NORMVALUENEEDED) != 0)))
            {
                normValue = normalizeValue(type, value);
                if (normValue != null)
                {
                    value = normValue;
                }
                isNormalized = 1;
            }
            if (type.type != XML_XSD10_SCHEMA_TYPE_BASIC)
            {
                /*
                * Get the built-in type.
                */
                biType = type.baseType;
                while ((biType != null) && (biType.type != XML_XSD10_SCHEMA_TYPE_BASIC))
                {
                    biType = biType.baseType;
                }

                if (biType == null)
                {
                    aerrorInt(actxt, "xmlSchemaVCheckCVCSimpleType", "could not get the built-in type");
                    return -1;
                }
            }
            else
            {
                biType = type;
            }
            /*
            * NOTATIONs need to be processed here, since they need
            * to lookup in the hashtable of NOTATION declarations of the schema.
            */
            if (actxt.type == XML_XSD10_SCHEMA_CTXT_VALIDATOR)
            {
                switch (biType.builtInType)
                {
                    case XML_XSD10_SCHEMAS_NOTATION:
                        ret = validateNotation((Xsd10ValidCtxt) actxt, ((Xsd10ValidCtxt) actxt).schema, null, value, val, valNeeded);
                        break;
                    case XML_XSD10_SCHEMAS_QNAME:
                        ret = validateQName((Xsd10ValidCtxt) actxt, value, val, valNeeded);
                        break;
                    default:
                        /* ws = xmlSchemaGetWhiteSpaceFacetValue(type); */
                        if (valNeeded != 0)
                        {
                            ret = valPredefTypeNodeNoNormIds(biType, value, val, node, ((Xsd10ValidCtxt) actxt).ids);
                        }
                        else
                        {
                            ret = valPredefTypeNodeNoNormIds(biType, value, null, node, ((Xsd10ValidCtxt) actxt).ids);
                        }
                        break;
                }
            }
            else if (actxt.type == XML_XSD10_SCHEMA_CTXT_PARSER)
            {
                switch (biType.builtInType)
                {
                    case XML_XSD10_SCHEMAS_NOTATION:
                        ret = validateNotation(null, ((Xsd10ParserCtxt) actxt).schema, node, value, val, valNeeded);
                        break;
                    default:
                        /* ws = xmlSchemaGetWhiteSpaceFacetValue(type); */
                        if (valNeeded != 0)
                        {
                            ret = valPredefTypeNodeNoNormIds(biType, value, val, node,
                                validSchemaDocIds((Xsd10ParserCtxt) actxt, node));
                        }
                        else
                        {
                            ret = valPredefTypeNodeNoNormIds(biType, value, null, node,
                                validSchemaDocIds((Xsd10ParserCtxt) actxt, node));
                        }
                        break;
                }
            }
            else
            {
                /*
                * Validation via a public API is not implemented yet.
                */
                return -1;
            }
            if (ret != 0)
            {
                if (ret < 0)
                {
                    aerrorInt(actxt, "xmlSchemaVCheckCVCSimpleType", "validating against a built-in type");
                    return -1;
                }
                if (wxsIsList(type) != 0)
                {
                    ret = XML_XSD10_SCHEMAV_CVC_DATATYPE_VALID_1_2_2;
                }
                else
                {
                    ret = XML_XSD10_SCHEMAV_CVC_DATATYPE_VALID_1_2_1;
                }
            }
            if ((ret == 0) && ((type.flags & XML_XSD10_SCHEMAS_TYPE_HAS_FACETS) != 0))
            {
                /*
                * Check facets.
                */
                ret = validateFacets(actxt, node, type, biType.builtInType, value, val[0], 0, fireErrors);
                if (ret != 0)
                {
                    if (ret < 0)
                    {
                        aerrorInt(actxt, "xmlSchemaVCheckCVCSimpleType", "validating facets of atomic simple type");
                        return -1;
                    }
                    if (wxsIsList(type) != 0)
                    {
                        ret = XML_XSD10_SCHEMAV_CVC_DATATYPE_VALID_1_2_2;
                    }
                    else
                    {
                        ret = XML_XSD10_SCHEMAV_CVC_DATATYPE_VALID_1_2_1;
                    }
                }
            }
            else if ((fireErrors != 0) && (ret > 0))
            {
                simpleTypeErr(actxt, ret, node, value, type, 1);
            }
        }
        else if (wxsIsList(type) != 0)
        {
            Xsd10Type itemType;
            int cur;
            int end;
            String tmpValue = null;
            long len = 0;
            Xsd10Val prevVal = null;
            Xsd10Val[] curVal = new Xsd10Val[] { null };
            /* 1.2.2 if {variety} is `list` then the string must be a sequence
            * of white space separated tokens, each of which `match`es a literal
            * in the `lexical space` of {item type definition}
            */
            /*
            * Note that XML_SCHEMAS_TYPE_NORMVALUENEEDED will be set if
            * the list type has an enum or pattern facet.
            */
            // XSD10_NORMALIZE(type)
            if ((isNormalized == 0) && ((normalize != 0) || ((type.flags & XML_XSD10_SCHEMAS_TYPE_NORMVALUENEEDED) != 0)))
            {
                normValue = normalizeValue(type, value);
                if (normValue != null)
                {
                    value = normValue;
                }
                isNormalized = 1;
            }
            /*
            * VAL TODO: Optimize validation of empty values.
            * VAL TODO: We do not have computed values for lists.
            */
            itemType = wxsListItemtype(type);
            cur = 0;
            do
            {
                while (validIsBlankCh(at(value, cur)))
                {
                    cur++;
                }
                end = cur;
                while ((at(value, end) != 0) && (!validIsBlankCh(at(value, end))))
                {
                    end++;
                }
                if (end == cur)
                {
                    break;
                }
                tmpValue = value.substring(cur, end);
                len++;

                if (valNeeded != 0)
                {
                    ret = vCheckCvcSimpleType(actxt, node, itemType, tmpValue, curVal, fireErrors, 0, 1);
                }
                else
                {
                    ret = vCheckCvcSimpleType(actxt, node, itemType, tmpValue, null, fireErrors, 0, 1);
                }
                tmpValue = null;
                if (curVal[0] != null)
                {
                    /*
                    * Add to list of computed values.
                    */
                    if (val[0] == null)
                    {
                        val[0] = curVal[0];
                    }
                    else
                    {
                        valueAppend(prevVal, curVal[0]);
                    }
                    prevVal = curVal[0];
                    curVal[0] = null;
                }
                if (ret != 0)
                {
                    if (ret < 0)
                    {
                        aerrorInt(actxt, "xmlSchemaVCheckCVCSimpleType", "validating an item of list simple type");
                        return -1;
                    }
                    ret = XML_XSD10_SCHEMAV_CVC_DATATYPE_VALID_1_2_2;
                    break;
                }
                cur = end;
            } while (at(value, cur) != 0);
            if ((ret == 0) && ((type.flags & XML_XSD10_SCHEMAS_TYPE_HAS_FACETS) != 0))
            {
                /*
                * Apply facets (pattern, enumeration).
                */
                ret = validateFacets(actxt, node, type, XML_XSD10_SCHEMAS_UNKNOWN, value, val[0], len, fireErrors);
                if (ret != 0)
                {
                    if (ret < 0)
                    {
                        aerrorInt(actxt, "xmlSchemaVCheckCVCSimpleType", "validating facets of list simple type");
                        return -1;
                    }
                    ret = XML_XSD10_SCHEMAV_CVC_DATATYPE_VALID_1_2_2;
                }
            }
            if ((fireErrors != 0) && (ret > 0))
            {
                /*
                * Report the normalized value.
                */
                normalize = 1;
                // XSD10_NORMALIZE(type)
                if ((isNormalized == 0) && ((normalize != 0) || ((type.flags & XML_XSD10_SCHEMAS_TYPE_NORMVALUENEEDED) != 0)))
                {
                    normValue = normalizeValue(type, value);
                    if (normValue != null)
                    {
                        value = normValue;
                    }
                    isNormalized = 1;
                }
                simpleTypeErr(actxt, ret, node, value, type, 1);
            }
        }
        else if (wxsIsUnion(type) != 0)
        {
            Xsd10TypeLink memberLink;
            /*
            * TODO: For all datatypes `derived` by `union`  whiteSpace does
            * not apply directly; however, the normalization behavior of `union`
            * types is controlled by the value of whiteSpace on that one of the
            * `memberTypes` against which the `union` is successfully validated.
            *
            * This means that the value is normalized by the first validating
            * member type, then the facets of the union type are applied. This
            * needs changing of the value!
            */

            /*
            * 1.2.3 if {variety} is `union` then the string must `match` a
            * literal in the `lexical space` of at least one member of
            * {member type definitions}
            */
            memberLink = getUnionSimpleTypeMemberTypes(type);
            if (memberLink == null)
            {
                aerrorInt(actxt, "xmlSchemaVCheckCVCSimpleType", "union simple type has no member types");
                return -1;
            }
            /*
            * Always normalize union type values, since we currently
            * cannot store the whitespace information with the value
            * itself; otherwise a later value-comparison would be
            * not possible.
            */
            while (memberLink != null)
            {
                if (valNeeded != 0)
                {
                    ret = vCheckCvcSimpleType(actxt, node, (Xsd10Type) memberLink.type, value, val, 0, 1, 0);
                }
                else
                {
                    ret = vCheckCvcSimpleType(actxt, node, (Xsd10Type) memberLink.type, value, null, 0, 1, 0);
                }
                if (ret <= 0)
                {
                    break;
                }
                memberLink = memberLink.next;
            }
            if (ret != 0)
            {
                if (ret < 0)
                {
                    aerrorInt(actxt, "xmlSchemaVCheckCVCSimpleType", "validating members of union simple type");
                    return -1;
                }
                ret = XML_XSD10_SCHEMAV_CVC_DATATYPE_VALID_1_2_3;
            }
            /*
            * Apply facets (pattern, enumeration).
            */
            if ((ret == 0) && ((type.flags & XML_XSD10_SCHEMAS_TYPE_HAS_FACETS) != 0))
            {
                /*
                * The normalization behavior of `union` types is controlled by
                * the value of whiteSpace on that one of the `memberTypes`
                * against which the `union` is successfully validated.
                */
                // XSD10_NORMALIZE(memberLink->type)
                if ((isNormalized == 0) && ((normalize != 0) || ((type.flags & XML_XSD10_SCHEMAS_TYPE_NORMVALUENEEDED) != 0)))
                {
                    normValue = normalizeValue((Xsd10Type) memberLink.type, value);
                    if (normValue != null)
                    {
                        value = normValue;
                    }
                    isNormalized = 1;
                }
                ret = validateFacets(actxt, node, type, XML_XSD10_SCHEMAS_UNKNOWN, value, val[0], 0, fireErrors);
                if (ret != 0)
                {
                    if (ret < 0)
                    {
                        aerrorInt(actxt, "xmlSchemaVCheckCVCSimpleType", "validating facets of union simple type");
                        return -1;
                    }
                    ret = XML_XSD10_SCHEMAV_CVC_DATATYPE_VALID_1_2_3;
                }
            }
            if ((fireErrors != 0) && (ret > 0))
            {
                simpleTypeErr(actxt, ret, node, value, type, 1);
            }
        }

        if (ret == 0)
        {
            if (retVal != null)
            {
                retVal[0] = val[0];
            }
            else if (val[0] != null)
            {
                freeValue(val[0]);
            }
        }
        else if (val[0] != null)
        {
            freeValue(val[0]);
        }
        return ret;
    }

    // xml_xsd10_v_expand_q_name
    /** Port of xmlSchemaVExpandQName. */
    static int vExpandQName(Xsd10ValidCtxt vctxt, String value, String[] nsName, String[] localName)
    {
        int ret = 0;

        if ((nsName == null) || (localName == null))
        {
            return -1;
        }
        nsName[0] = null;
        localName[0] = null;

        ret = treeValidateQname(value, 1);
        if (ret == -1)
        {
            return -1;
        }
        if (ret > 0)
        {
            simpleTypeErr(vctxt, XML_XSD10_SCHEMAV_CVC_DATATYPE_VALID_1_2_1, null,
                value, getBuiltInType(XML_XSD10_SCHEMAS_QNAME), 1);
            return 1;
        }
        String local;
        String[] prefix = new String[] { null };

        /*
        * NOTE: xmlSplitQName2 will return a duplicated
        * string.
        */
        local = treeSplitQname2(value, prefix);
        if (local == null)
        {
            localName[0] = dictLookup(vctxt.dict, value, -1);
        }
        else
        {
            localName[0] = dictLookup(vctxt.dict, local, -1);
        }

        nsName[0] = lookupNamespace(vctxt, prefix[0]);

        if (prefix[0] != null)
        {
            /*
            * A namespace must be found if the prefix is NOT NULL.
            */
            if (nsName[0] == null)
            {
                customErr(vctxt, XML_XSD10_SCHEMAV_CVC_DATATYPE_VALID_1_2_1, null,
                    getBuiltInType(XML_XSD10_SCHEMAS_QNAME),
                    "The QName value '%s' has no " +
                    "corresponding namespace declaration in scope",
                    value, null);
                return 2;
            }
        }
        return 0;
    }

    // xml_xsd10_process_xsi_type
    /** Port of xmlSchemaProcessXSIType. */
    static int processXsiType(Xsd10ValidCtxt vctxt, Xsd10AttrInfo iattr, Xsd10Type[] localType, Xsd10Element elemDecl)
    {
        int ret = 0;
        /*
        * cvc-elt (3.3.4) : (4)
        * AND
        * Schema-Validity Assessment (Element) (cvc-assess-elt)
        *   (1.2.1.2.1) - (1.2.1.2.4)
        * Handle 'xsi:type'.
        */
        if (localType == null)
        {
            return -1;
        }
        localType[0] = null;
        if (iattr == null)
        {
            return 0;
        }
        exit:
        {
            String[] nsName = new String[] { null };
            String[] local = new String[] { null };
            /*
            * TODO: We should report a *warning* that the type was overridden
            * by the instance.
            */
            vctxt.inode = iattr;
            /*
            * (cvc-elt) (3.3.4) : (4.1)
            * (cvc-assess-elt) (1.2.1.2.2)
            */
            ret = vExpandQName(vctxt, iattr.value, nsName, local);
            if (ret != 0)
            {
                if (ret < 0)
                {
                    verrorInt(vctxt, "xmlSchemaValidateElementByDeclaration",
                        "calling xmlSchemaQNameExpand() to validate the " +
                        "attribute 'xsi:type'");
                    activateElem(vctxt);
                    return -1;
                }
                break exit;
            }
            /*
            * (cvc-elt) (3.3.4) : (4.2)
            * (cvc-assess-elt) (1.2.1.2.3)
            */
            localType[0] = getType(vctxt.schema, local[0], nsName[0]);
            if (localType[0] == null)
            {
                String[] str = new String[] { null };

                customErr(vctxt, XML_XSD10_SCHEMAV_CVC_ELT_4_2, null,
                    getBuiltInType(XML_XSD10_SCHEMAS_QNAME),
                    "The QName value '%s' of the xsi:type attribute does not " +
                    "resolve to a type definition",
                    formatQName(str, nsName[0], local[0]), null);
                ret = vctxt.err;
                break exit;
            }
            if (elemDecl != null)
            {
                int set = 0;

                /*
                * SPEC cvc-elt (3.3.4) : (4.3) (Type Derivation OK)
                * "The `local type definition` must be validly
                * derived from the {type definition} given the union of
                * the {disallowed substitutions} and the {type definition}'s
                * {prohibited substitutions}, as defined in
                * Type Derivation OK (Complex) ($3.4.6)
                * (if it is a complex type definition),
                * or given {disallowed substitutions} as defined in Type
                * Derivation OK (Simple) ($3.14.6) (if it is a simple type
                * definition)."
                *
                * {disallowed substitutions}: the "block" on the element decl.
                * {prohibited substitutions}: the "block" on the type def.
                */
                if (((elemDecl.flags & XML_XSD10_SCHEMAS_ELEM_BLOCK_EXTENSION) != 0) ||
                    ((elemDecl.subtypes.flags & XML_XSD10_SCHEMAS_TYPE_BLOCK_EXTENSION) != 0))
                {
                    set |= XSD10_SUBSET_EXTENSION;
                }

                if (((elemDecl.flags & XML_XSD10_SCHEMAS_ELEM_BLOCK_RESTRICTION) != 0) ||
                    ((elemDecl.subtypes.flags & XML_XSD10_SCHEMAS_TYPE_BLOCK_RESTRICTION) != 0))
                {
                    set |= XSD10_SUBSET_RESTRICTION;
                }

                /*
                * REMOVED and CHANGED since this produced a parser context
                * which adds to the string dict of the schema. So this would
                * change the schema and we don't want this. We don't need
                * the parser context anymore.
                */

                if (checkCosDerivedOk(vctxt, localType[0], elemDecl.subtypes, set) != 0)
                {
                    String[] str = new String[] { null };

                    customErr(vctxt, XML_XSD10_SCHEMAV_CVC_ELT_4_3, null, null,
                        "The type definition '%s', specified by xsi:type, is " +
                        "blocked or not validly derived from the type definition " +
                        "of the element declaration",
                        formatQName(str, localType[0].targetNamespace, localType[0].name),
                        null);
                    ret = vctxt.err;
                    localType[0] = null;
                }
            }
        }
        // exit:
        activateElem(vctxt);
        return ret;
    }

    // xml_xsd10_validate_elem_decl
    /** Port of xmlSchemaValidateElemDecl. */
    static int validateElemDecl(Xsd10ValidCtxt vctxt)
    {
        Xsd10Element elemDecl = vctxt.inode.decl;
        Xsd10Type actualType;

        /*
        * cvc-elt (3.3.4) : 1
        */
        if (elemDecl == null)
        {
            verror(vctxt, XML_XSD10_SCHEMAV_CVC_ELT_1, null, "No matching declaration available");
            return vctxt.err;
        }
        actualType = wxsElemTypedef(elemDecl);
        /*
        * cvc-elt (3.3.4) : 2
        */
        if ((elemDecl.flags & XML_XSD10_SCHEMAS_ELEM_ABSTRACT) != 0)
        {
            verror(vctxt, XML_XSD10_SCHEMAV_CVC_ELT_2, null, "The element declaration is abstract");
            return vctxt.err;
        }
        if (actualType == null)
        {
            verror(vctxt, XML_XSD10_SCHEMAV_CVC_TYPE_1, null, "The type definition is absent");
            return XML_XSD10_SCHEMAV_CVC_TYPE_1;
        }
        if (vctxt.nbAttrInfos != 0)
        {
            int ret;
            Xsd10AttrInfo iattr;
            /*
            * cvc-elt (3.3.4) : 3
            * Handle 'xsi:nil'.
            */
            iattr = getMetaAttrInfo(vctxt, XML_XSD10_SCHEMA_ATTR_INFO_META_XSI_NIL);
            if (iattr != null)
            {
                vctxt.inode = iattr;
                /*
                * Validate the value.
                */
                Xsd10Val[] ival = new Xsd10Val[] { iattr.val };
                ret = vCheckCvcSimpleType(vctxt, null,
                    getBuiltInType(XML_XSD10_SCHEMAS_BOOLEAN),
                    iattr.value, ival, 1, 0, 0);
                iattr.val = ival[0];
                activateElem(vctxt);
                if (ret < 0)
                {
                    verrorInt(vctxt, "xmlSchemaValidateElemDecl",
                        "calling xmlSchemaVCheckCVCSimpleType() to " +
                        "validate the attribute 'xsi:nil'");
                    return -1;
                }
                if (ret == 0)
                {
                    if ((elemDecl.flags & XML_XSD10_SCHEMAS_ELEM_NILLABLE) == 0)
                    {
                        /*
                        * cvc-elt (3.3.4) : 3.1
                        */
                        verror(vctxt, XML_XSD10_SCHEMAV_CVC_ELT_3_1, null, "The element is not 'nillable'");
                        /* Does not return an error on purpose. */
                    }
                    else
                    {
                        if (valueGetAsBoolean(iattr.val) != 0)
                        {
                            /*
                            * cvc-elt (3.3.4) : 3.2.2
                            */
                            if (((elemDecl.flags & XML_XSD10_SCHEMAS_ELEM_FIXED) != 0) && (elemDecl.value != null))
                            {
                                verror(vctxt, XML_XSD10_SCHEMAV_CVC_ELT_3_2_2, null,
                                    "The element cannot be 'nilled' because " +
                                    "there is a fixed value constraint defined " +
                                    "for it");
                                /* Does not return an error on purpose. */
                            }
                            else
                            {
                                vctxt.inode.flags |= XML_XSD10_SCHEMA_ELEM_INFO_NILLED;
                            }
                        }
                    }
                }
            }
            /*
            * cvc-elt (3.3.4) : 4
            * Handle 'xsi:type'.
            */
            iattr = getMetaAttrInfo(vctxt, XML_XSD10_SCHEMA_ATTR_INFO_META_XSI_TYPE);
            if (iattr != null)
            {
                Xsd10Type[] localType = new Xsd10Type[] { null };

                ret = processXsiType(vctxt, iattr, localType, elemDecl);
                if (ret != 0)
                {
                    if (ret == -1)
                    {
                        verrorInt(vctxt, "xmlSchemaValidateElemDecl",
                            "calling xmlSchemaProcessXSIType() to " +
                            "process the attribute 'xsi:type'");
                        return -1;
                    }
                    /* Does not return an error on purpose. */
                }
                if (localType[0] != null)
                {
                    vctxt.inode.flags |= XML_XSD10_SCHEMA_ELEM_INFO_LOCAL_TYPE;
                    actualType = localType[0];
                }
            }
        }
        /*
        * IDC: Register identity-constraint XPath matchers.
        */
        if ((elemDecl.idcs != null) && (idcRegisterMatchers(vctxt, elemDecl) == -1))
        {
            return -1;
        }
        /*
        * No actual type definition.
        */
        if (actualType == null)
        {
            verror(vctxt, XML_XSD10_SCHEMAV_CVC_TYPE_1, null, "The type definition is absent");
            return XML_XSD10_SCHEMAV_CVC_TYPE_1;
        }
        /*
        * Remember the actual type definition.
        */
        vctxt.inode.typeDef = actualType;

        return 0;
    }

    // xml_xsd10_v_attributes_simple
    /** Port of xmlSchemaVAttributesSimple. */
    static int vAttributesSimple(Xsd10ValidCtxt vctxt)
    {
        Xsd10AttrInfo iattr;
        int ret = 0;
        int i;

        /*
        * SPEC cvc-type (3.1.1)
        * "The attributes of must be empty, excepting those whose namespace
        * name is identical to http://www.w3.org/2001/XMLSchema-instance and
        * whose local name is one of type, nil, schemaLocation or
        * noNamespaceSchemaLocation."
        */
        if (vctxt.nbAttrInfos == 0)
        {
            return 0;
        }
        for (i = 0; i < vctxt.nbAttrInfos; i++)
        {
            iattr = vctxt.attrInfos[i];
            if (iattr.metaType == 0)
            {
                vctxt.inode = iattr;
                illegalAttrErr(vctxt, XML_XSD10_SCHEMAV_CVC_TYPE_3_1_1, iattr, null);
                ret = XML_XSD10_SCHEMAV_CVC_TYPE_3_1_1;
            }
        }
        activateElem(vctxt);
        return ret;
    }

    /*
    * Cleanup currently used attribute infos.
    */
    // xml_xsd10_clear_attr_infos
    /** Port of xmlSchemaClearAttrInfos. */
    static void clearAttrInfos(Xsd10ValidCtxt vctxt)
    {
        int i;
        Xsd10AttrInfo attr;

        if (vctxt.nbAttrInfos == 0)
        {
            return;
        }
        for (i = 0; i < vctxt.nbAttrInfos; i++)
        {
            attr = vctxt.attrInfos[i];
            if (attr.val != null)
            {
                freeValue(attr.val);
                attr.val = null;
            }
            zeroAttrInfo(attr);
        }
        vctxt.nbAttrInfos = 0;
    }

    /*
    * C stores either an XmlXsd10Element or an XmlXsd10Wildcard (content model transition data)
    * in inode->decl. Xsd10NodeInfo.decl is typed Xsd10Element, so a wildcard stored there is
    * kept in this side table (identity keyed, weak, so dropped node infos are collected).
    */
    private static final java.util.Map<Xsd10NodeInfo, Xsd10Wildcard> wildDecls =
        java.util.Collections.synchronizedMap(new java.util.WeakHashMap<>());

    /** C inode->decl as the generic item it really is (Element, Wildcard or null). */
    private static Xsd10BasicItem getDecl(Xsd10NodeInfo inode)
    {
        if (inode.decl != null)
        {
            return inode.decl;
        }
        return wildDecls.get(inode);
    }

    /** C inode->decl = item. */
    private static void setDecl(Xsd10NodeInfo inode, Object item)
    {
        if (item instanceof Xsd10Wildcard)
        {
            inode.decl = null;
            wildDecls.put(inode, (Xsd10Wildcard) item);
        }
        else
        {
            inode.decl = (item instanceof Xsd10Element) ? (Xsd10Element) item : null;
            if (!wildDecls.isEmpty())
            {
                wildDecls.remove(inode);
            }
        }
    }

    // xml_xsd10_v_attributes_complex
    /**
     * Port of xmlSchemaVAttributesComplex.
     * 3.4.4 Complex Type Definition Validation Rules
     *   Element Locally Valid (Complex Type) (cvc-complex-type)
     * 3.2.4 Attribute Declaration Validation Rules
     *   Validation Rule: Attribute Locally Valid (cvc-attribute)
     *   Attribute Locally Valid (Use) (cvc-au)
     * Only "assessed" attribute information items will be visible to
     * IDCs. I.e. not "lax" (without declaration) and "skip" wild attributes.
     */
    static int vAttributesComplex(Xsd10ValidCtxt vctxt)
    {
        Xsd10Type type = vctxt.inode.typeDef;
        Xsd10ItemList attrUseList;
        Xsd10AttributeUse attrUse = null;
        Xsd10Attribute attrDecl = null;
        Xsd10AttrInfo iattr;
        Xsd10AttrInfo tmpiattr;
        int i;
        int j;
        int found;
        int nbAttrs;
        int nbUses;
        int xpathRes = 0;
        int res;
        int wildIDs = 0;
        int fixed;
        XmlNode defAttrOwnerElem = null;

        /*
        * SPEC (cvc-attribute)
        * (1) "The declaration must not be `absent` (see Missing
        * Sub-components ($5.3) for how this can fail to be
        * the case)."
        * (2) "Its {type definition} must not be absent."
        *
        * NOTE (1) + (2): This is not handled here, since we currently do not
        * allow validation against schemas which have missing sub-components.
        *
        * SPEC (cvc-complex-type)
        * (3) "For each attribute information item in the element information
        * item's [attributes] excepting those whose [namespace name] is
        * identical to http://www.w3.org/2001/XMLSchema-instance and whose
        * [local name] is one of type, nil, schemaLocation or
        * noNamespaceSchemaLocation, the appropriate case among the following
        * must be true:
        *
        */
        attrUseList = (Xsd10ItemList) type.attrUses;
        /*
        * @nbAttrs is the number of attributes present in the instance.
        */
        nbAttrs = vctxt.nbAttrInfos;
        if (attrUseList != null)
        {
            nbUses = attrUseList.nbItems;
        }
        else
        {
            nbUses = 0;
        }
        for (i = 0; i < nbUses; i++)
        {
            found = 0;
            attrUse = (Xsd10AttributeUse) attrUseList.items[i];
            attrDecl = wxsAttruseDecl(attrUse);
            for (j = 0; j < nbAttrs; j++)
            {
                iattr = vctxt.attrInfos[j];
                /*
                * SPEC (cvc-complex-type) (3)
                * Skip meta attributes.
                */
                if (iattr.metaType != 0)
                {
                    continue;
                }
                if (at(iattr.localName, 0) != at(attrDecl.name, 0))
                {
                    continue;
                }
                if (strEqual(iattr.localName, attrDecl.name) == 0)
                {
                    continue;
                }
                if (strEqual(iattr.nsName, attrDecl.targetNamespace) == 0)
                {
                    continue;
                }
                found = 1;
                /*
                * SPEC (cvc-complex-type)
                * (3.1) "If there is among the {attribute uses} an attribute
                * use with an {attribute declaration} whose {name} matches
                * the attribute information item's [local name] and whose
                * {target namespace} is identical to the attribute information
                * item's [namespace name] (where an `absent` {target namespace}
                * is taken to be identical to a [namespace name] with no value),
                * then the attribute information must be `valid` with respect
                * to that attribute use as per Attribute Locally Valid (Use)
                * ($3.5.4). In this case the {attribute declaration} of that
                * attribute use is the `context-determined declaration` for the
                * attribute information item with respect to Schema-Validity
                * Assessment (Attribute) ($3.2.4) and
                * Assessment Outcome (Attribute) ($3.2.5).
                */
                iattr.state = XML_XSD10_SCHEMAS_ATTR_ASSESSED;
                iattr.use = attrUse;
                /*
                * Context-determined declaration.
                */
                iattr.decl = attrDecl;
                iattr.typeDef = attrDecl.subtypes;
                break;
            }

            if (found != 0)
            {
                continue;
            }

            if (attrUse.occurs == XML_XSD10_SCHEMAS_ATTR_USE_REQUIRED)
            {
                /*
                * Handle non-existent, required attributes.
                *
                * SPEC (cvc-complex-type)
                * (4) "The {attribute declaration} of each attribute use in
                * the {attribute uses} whose {required} is true matches one
                * of the attribute information items in the element information
                * item's [attributes] as per clause 3.1 above."
                */
                tmpiattr = getFreshAttrInfo(vctxt);
                if (tmpiattr == null)
                {
                    verrorInt(vctxt, "xmlSchemaVAttributesComplex", "calling xmlSchemaGetFreshAttrInfo()");
                    return -1;
                }
                tmpiattr.state = XML_XSD10_SCHEMAS_ATTR_ERR_MISSING;
                tmpiattr.use = attrUse;
                tmpiattr.decl = attrDecl;
            }
            else if ((attrUse.occurs == XML_XSD10_SCHEMAS_ATTR_USE_OPTIONAL) &&
                ((attrUse.defValue != null) || (attrDecl.defValue != null)))
            {
                /*
                * Handle non-existent, optional, default/fixed attributes.
                */
                tmpiattr = getFreshAttrInfo(vctxt);
                if (tmpiattr == null)
                {
                    verrorInt(vctxt, "xmlSchemaVAttributesComplex", "calling xmlSchemaGetFreshAttrInfo()");
                    return -1;
                }
                tmpiattr.state = XML_XSD10_SCHEMAS_ATTR_DEFAULT;
                tmpiattr.use = attrUse;
                tmpiattr.decl = attrDecl;
                tmpiattr.typeDef = attrDecl.subtypes;
                tmpiattr.localName = attrDecl.name;
                tmpiattr.nsName = attrDecl.targetNamespace;
            }
        }

        if (vctxt.nbAttrInfos == 0)
        {
            return 0;
        }
        /*
        * Validate against the wildcard.
        */
        if (type.attributeWildcard != null)
        {
            /*
            * SPEC (cvc-complex-type)
            * (3.2.1) "There must be an {attribute wildcard}."
            */
            for (i = 0; i < nbAttrs; i++)
            {
                iattr = vctxt.attrInfos[i];
                /*
                * SPEC (cvc-complex-type) (3)
                * Skip meta attributes.
                */
                if (iattr.state != XML_XSD10_SCHEMAS_ATTR_UNKNOWN)
                {
                    continue;
                }
                /*
                * SPEC (cvc-complex-type)
                * (3.2.2) "The attribute information item must be `valid` with
                * respect to it as defined in Item Valid (Wildcard) ($3.10.4)."
                *
                * SPEC Item Valid (Wildcard) (cvc-wildcard)
                * "... its [namespace name] must be `valid` with respect to
                * the wildcard constraint, as defined in Wildcard allows
                * Namespace Name ($3.10.4)."
                */
                if (checkCvcWildcardNamespace(type.attributeWildcard, iattr.nsName) == 0)
                {
                    /*
                    * Handle processContents.
                    *
                    * SPEC (cvc-wildcard):
                    * processContents | context-determined declaration:
                    * "strict"          "mustFind"
                    * "lax"             "none"
                    * "skip"            "skip"
                    */
                    if (type.attributeWildcard.processContents == XML_XSD10_SCHEMAS_ANY_SKIP)
                    {
                        /*
                        * context-determined declaration = "skip"
                        *
                        * SPEC PSVI Assessment Outcome (Attribute)
                        * [validity] = "notKnown"
                        * [validation attempted] = "none"
                        */
                        iattr.state = XML_XSD10_SCHEMAS_ATTR_WILD_SKIP;
                        continue;
                    }
                    /*
                    * Find an attribute declaration.
                    */
                    iattr.decl = getAttributeDecl(vctxt.schema, iattr.localName, iattr.nsName);
                    if (iattr.decl != null)
                    {
                        iattr.state = XML_XSD10_SCHEMAS_ATTR_ASSESSED;
                        /*
                        * SPEC (cvc-complex-type)
                        * (5) "Let [Definition:]  the wild IDs be the set of
                        * all attribute information item to which clause 3.2
                        * applied and whose `validation` resulted in a
                        * `context-determined declaration` of mustFind or no
                        * `context-determined declaration` at all, and whose
                        * [local name] and [namespace name] resolve (as
                        * defined by QName resolution (Instance) ($3.15.4)) to
                        * an attribute declaration whose {type definition} is
                        * or is derived from ID. Then all of the following
                        * must be true:"
                        */
                        iattr.typeDef = wxsAttrTypedef(iattr.decl);
                        if (isDerivedFromBuiltInType(iattr.typeDef, XML_XSD10_SCHEMAS_ID) != 0)
                        {
                            /*
                            * SPEC (5.1) "There must be no more than one
                            * item in `wild IDs`."
                            */
                            if (wildIDs != 0)
                            {
                                /* VAL TODO */
                                iattr.state = XML_XSD10_SCHEMAS_ATTR_ERR_WILD_DUPLICATE_ID;
                                continue;
                            }
                            wildIDs++;
                            /*
                            * SPEC (cvc-complex-type)
                            * (5.2) "If `wild IDs` is non-empty, there must not
                            * be any attribute uses among the {attribute uses}
                            * whose {attribute declaration}'s {type definition}
                            * is or is derived from ID."
                            */
                            if (attrUseList != null)
                            {
                                for (j = 0; j < attrUseList.nbItems; j++)
                                {
                                    if (isDerivedFromBuiltInType(
                                        wxsAttruseTypedef((Xsd10BasicItem) attrUseList.items[j]),
                                        XML_XSD10_SCHEMAS_ID) != 0)
                                    {
                                        /* URGENT VAL TODO: implement */
                                        iattr.state = XML_XSD10_SCHEMAS_ATTR_ERR_WILD_AND_USE_ID;
                                        break;
                                    }
                                }
                            }
                        }
                    }
                    else if (type.attributeWildcard.processContents == XML_XSD10_SCHEMAS_ANY_LAX)
                    {
                        iattr.state = XML_XSD10_SCHEMAS_ATTR_WILD_LAX_NO_DECL;
                        /*
                        * SPEC PSVI Assessment Outcome (Attribute)
                        * [validity] = "notKnown"
                        * [validation attempted] = "none"
                        */
                    }
                    else
                    {
                        iattr.state = XML_XSD10_SCHEMAS_ATTR_ERR_WILD_STRICT_NO_DECL;
                    }
                }
            }
        }

        if (vctxt.nbAttrInfos == 0)
        {
            return 0;
        }

        /*
        * Get the owner element; needed for creation of default attributes.
        * This fixes bug #341337, reported by David Grohmann.
        */
        if ((vctxt.options & XML_XSD10_SCHEMA_VAL_VC_I_CREATE) != 0)
        {
            Xsd10NodeInfo ielem = vctxt.elemInfos[vctxt.depth];
            if ((ielem != null) && (ielem.node != null) && (nodeDom(ielem.node) != null))
            {
                defAttrOwnerElem = ielem.node;
            }
        }
        /*
        * Validate values, create default attributes, evaluate IDCs.
        */
        for (i = 0; i < vctxt.nbAttrInfos; i++)
        {
            iattr = vctxt.attrInfos[i];
            /*
            * VAL TODO: Note that we won't try to resolve IDCs to
            * "lax" and "skip" validated attributes. Check what to
            * do in this case.
            */
            if ((iattr.state != XML_XSD10_SCHEMAS_ATTR_ASSESSED) &&
                (iattr.state != XML_XSD10_SCHEMAS_ATTR_DEFAULT))
            {
                continue;
            }
            /*
            * VAL TODO: What to do if the type definition is missing?
            */
            if (iattr.typeDef == null)
            {
                iattr.state = XML_XSD10_SCHEMAS_ATTR_ERR_NO_TYPE;
                continue;
            }

            vctxt.inode = iattr;
            fixed = 0;
            xpathRes = 0;

            if (vctxt.xpathStates != null)
            {
                /*
                * Evaluate IDCs.
                */
                xpathRes = xPathEvaluate(vctxt, ATTRIBUTE);
                if (xpathRes == -1)
                {
                    verrorInt(vctxt, "xmlSchemaVAttributesComplex", "calling xmlSchemaXPathEvaluate()");
                    activateElem(vctxt);
                    return -1;
                }
            }

            eval_idcs:
            {
                if (iattr.state == XML_XSD10_SCHEMAS_ATTR_DEFAULT)
                {
                    /*
                    * Default/fixed attributes.
                    * We need the value only if we need to resolve IDCs or
                    * will create default attributes.
                    */
                    if ((xpathRes != 0) || (defAttrOwnerElem != null))
                    {
                        if (iattr.use.defValue != null)
                        {
                            iattr.value = iattr.use.defValue;
                            iattr.val = iattr.use.defVal;
                        }
                        else
                        {
                            iattr.value = iattr.decl.defValue;
                            iattr.val = iattr.decl.defVal;
                        }
                        /*
                        * IDCs will consume the precomputed default value,
                        * so we need to clone it.
                        */
                        if (iattr.val == null)
                        {
                            verrorInt(vctxt, "xmlSchemaVAttributesComplex",
                                "default/fixed value on an attribute use was " +
                                "not precomputed");
                            activateElem(vctxt);
                            return -1;
                        }
                        iattr.val = copyValue(iattr.val);
                        if (iattr.val == null)
                        {
                            verrorInt(vctxt, "xmlSchemaVAttributesComplex", "calling xmlSchemaCopyValue()");
                            activateElem(vctxt);
                            return -1;
                        }
                    }
                    /*
                    * PSVI: Add the default attribute to the current element.
                    * VAL TODO: Should we use the *normalized* value? This currently
                    *   uses the *initial* value.
                    */

                    if (defAttrOwnerElem != null)
                    {
                        String normValue;
                        String value;

                        value = iattr.value;
                        /*
                        * Normalize the value.
                        */
                        normValue = normalizeValue(iattr.typeDef, iattr.value);
                        if (normValue != null)
                        {
                            value = normValue;
                        }

                        if (iattr.nsName == null)
                        {
                            if (nodeAppendAttr(defAttrOwnerElem, iattr.localName, null, value) == null)
                            {
                                verrorInt(vctxt, "xmlSchemaVAttributesComplex", "calling xmlNewProp()");
                                activateElem(vctxt);
                                return -1;
                            }
                        }
                        else
                        {
                            XmlNs ns;

                            ns = treeSearchNsByHref(defAttrOwnerElem, iattr.nsName);
                            if (ns == null)
                            {
                                String prefix;
                                int counter = 0;

                                /*
                                * Create a namespace declaration on the validation
                                * root node if no namespace declaration is in scope.
                                */
                                do
                                {
                                    prefix = "p" + (counter++);
                                    ns = treeSearchNs(defAttrOwnerElem, prefix);
                                    if (counter > 1000)
                                    {
                                        verrorInt(vctxt, "xmlSchemaVAttributesComplex",
                                            "could not compute a ns prefix for a " +
                                            "default/fixed attribute");
                                        activateElem(vctxt);
                                        return -1;
                                    }
                                } while (ns != null);
                                ns = nsDeclare(vctxt.validationRoot, prefix, iattr.nsName);
                            }
                            /*
                            * TODO:
                            * http://lists.w3.org/Archives/Public/www-xml-schema-comments/2005JulSep/0406.html
                            * If we have QNames: do we need to ensure there's a
                            * prefix defined for the QName?
                            */
                            nodeAppendAttr(defAttrOwnerElem, iattr.localName, ns, value);
                        }
                    }
                    /*
                    * Go directly to IDC evaluation.
                    */
                    break eval_idcs;
                }
                /*
                * Validate the value.
                */
                if (vctxt.value != null)
                {
                    /*
                    * Free last computed value; just for safety reasons.
                    */
                    freeValue(vctxt.value);
                    vctxt.value = null;
                }
                /*
                * Note that the attribute *use* can be unavailable, if
                * the attribute was a wild attribute.
                */
                if (((iattr.decl.flags & XML_XSD10_SCHEMAS_ATTR_FIXED) != 0) ||
                    ((iattr.use != null) && ((iattr.use.flags & XML_XSD10_SCHEMAS_ATTR_FIXED) != 0)))
                {
                    fixed = 1;
                }
                else
                {
                    fixed = 0;
                }
                /*
                * SPEC (cvc-attribute)
                * (3) "The item's `normalized value` must be locally `valid`
                * with respect to that {type definition} as per
                * String Valid ($3.14.4)."
                *
                * VAL TODO: Do we already have the
                * "normalized attribute value" here?
                */
                if ((xpathRes != 0) || (fixed != 0))
                {
                    iattr.flags |= XML_XSD10_SCHEMA_NODE_INFO_VALUE_NEEDED;
                    /*
                    * Request a computed value.
                    */
                    Xsd10Val[] aval = new Xsd10Val[] { iattr.val };
                    res = vCheckCvcSimpleType(vctxt, iattr.node, iattr.typeDef, iattr.value, aval, 1, 1, 0);
                    iattr.val = aval[0];
                }
                else
                {
                    res = vCheckCvcSimpleType(vctxt, iattr.node, iattr.typeDef, iattr.value, null, 1, 0, 0);
                }

                if (res != 0)
                {
                    if (res == -1)
                    {
                        verrorInt(vctxt, "xmlSchemaVAttributesComplex",
                            "calling xmlSchemaStreamValidateSimpleTypeValue()");
                        activateElem(vctxt);
                        return -1;
                    }
                    iattr.state = XML_XSD10_SCHEMAS_ATTR_INVALID_VALUE;
                    /*
                    * SPEC PSVI Assessment Outcome (Attribute)
                    * [validity] = "invalid"
                    */
                    break eval_idcs;
                }

                if (fixed != 0)
                {
                    /*
                    * SPEC Attribute Locally Valid (Use) (cvc-au)
                    * "For an attribute information item to be `valid`
                    * with respect to an attribute use its *normalized*
                    * value must match the *canonical* lexical
                    * representation of the attribute use's {value
                    * constraint}value, if it is present and fixed."
                    *
                    * VAL TODO: The requirement for the *canonical* value
                    * will be removed in XML Schema 1.1.
                    */
                    /*
                    * SPEC Attribute Locally Valid (cvc-attribute)
                    * (4) "The item's *actual* value must match the *value* of
                    * the {value constraint}, if it is present and fixed."
                    */
                    if (iattr.val == null)
                    {
                        /* VAL TODO: A value was not precomputed. */
                        break eval_idcs;
                    }
                    if ((iattr.use != null) && (iattr.use.defValue != null))
                    {
                        if (iattr.use.defVal == null)
                        {
                            /* VAL TODO: A default value was not precomputed. */
                            break eval_idcs;
                        }
                        iattr.vcValue = iattr.use.defValue;
                        if (areValuesEqual(iattr.val, iattr.use.defVal) == 0)
                        {
                            iattr.state = XML_XSD10_SCHEMAS_ATTR_ERR_FIXED_VALUE;
                        }
                    }
                    else
                    {
                        if (iattr.decl.defVal == null)
                        {
                            /* VAL TODO: A default value was not precomputed. */
                            break eval_idcs;
                        }
                        iattr.vcValue = iattr.decl.defValue;
                        if (areValuesEqual(iattr.val, iattr.decl.defVal) == 0)
                        {
                            iattr.state = XML_XSD10_SCHEMAS_ATTR_ERR_FIXED_VALUE;
                        }
                    }
                    /*
                    * [validity] = "valid"
                    */
                }
            }
            // eval_idcs:
            /*
            * Evaluate IDCs.
            */
            if (xpathRes != 0)
            {
                if (xPathProcessHistory(vctxt, vctxt.depth + 1) == -1)
                {
                    verrorInt(vctxt, "xmlSchemaVAttributesComplex", "calling xmlSchemaXPathEvaluate()");
                    activateElem(vctxt);
                    return -1;
                }
            }
            else if (vctxt.xpathStates != null)
            {
                xPathPop(vctxt);
            }
        }

        /*
        * Report errors.
        */
        for (i = 0; i < vctxt.nbAttrInfos; i++)
        {
            iattr = vctxt.attrInfos[i];
            if ((iattr.state == XML_XSD10_SCHEMAS_ATTR_META) ||
                (iattr.state == XML_XSD10_SCHEMAS_ATTR_ASSESSED) ||
                (iattr.state == XML_XSD10_SCHEMAS_ATTR_WILD_SKIP) ||
                (iattr.state == XML_XSD10_SCHEMAS_ATTR_WILD_LAX_NO_DECL))
            {
                continue;
            }
            vctxt.inode = iattr;
            switch (iattr.state)
            {
                case XML_XSD10_SCHEMAS_ATTR_ERR_MISSING:
                {
                    String[] str = new String[] { null };
                    activateElem(vctxt);
                    customErr(vctxt, XML_XSD10_SCHEMAV_CVC_COMPLEX_TYPE_4, null, null,
                        "The attribute '%s' is required but missing",
                        formatQName(str, iattr.decl.targetNamespace, iattr.decl.name),
                        null);
                    break;
                }
                case XML_XSD10_SCHEMAS_ATTR_ERR_NO_TYPE:
                    verror(vctxt, XML_XSD10_SCHEMAV_CVC_ATTRIBUTE_2, null, "The type definition is absent");
                    break;
                case XML_XSD10_SCHEMAS_ATTR_ERR_FIXED_VALUE:
                    customErr(vctxt, XML_XSD10_SCHEMAV_CVC_AU, null, null,
                        "The value '%s' does not match the fixed " +
                        "value constraint '%s'",
                        iattr.value, iattr.vcValue);
                    break;
                case XML_XSD10_SCHEMAS_ATTR_ERR_WILD_STRICT_NO_DECL:
                    verror(vctxt, XML_XSD10_SCHEMAV_CVC_WILDCARD, null,
                        "No matching global attribute declaration available, but " +
                        "demanded by the strict wildcard");
                    break;
                case XML_XSD10_SCHEMAS_ATTR_UNKNOWN:
                    if (iattr.metaType != 0)
                    {
                        break;
                    }
                    /*
                    * MAYBE VAL TODO: One might report different error messages
                    * for the following errors.
                    */
                    if (type.attributeWildcard == null)
                    {
                        illegalAttrErr(vctxt, XML_XSD10_SCHEMAV_CVC_COMPLEX_TYPE_3_2_1, iattr, null);
                    }
                    else
                    {
                        illegalAttrErr(vctxt, XML_XSD10_SCHEMAV_CVC_COMPLEX_TYPE_3_2_2, iattr, null);
                    }
                    break;
                default:
                    break;
            }
        }

        activateElem(vctxt);
        return 0;
    }

    // xml_xsd10_validate_elem_wildcard
    /** Port of xmlSchemaValidateElemWildcard. */
    static int validateElemWildcard(Xsd10ValidCtxt vctxt, int[] skip)
    {
        Xsd10BasicItem d = getDecl(vctxt.inode);
        Xsd10Wildcard wild = (d instanceof Xsd10Wildcard) ? (Xsd10Wildcard) d : null;
        /*
        * The namespace of the element was already identified to be
        * matching the wildcard.
        */
        if ((skip == null) || (wild == null) || (wild.type != XML_XSD10_SCHEMA_TYPE_ANY))
        {
            verrorInt(vctxt, "xmlSchemaValidateElemWildcard", "bad arguments");
            return -1;
        }
        skip[0] = 0;
        if (wild.processContents == XML_XSD10_SCHEMAS_ANY_SKIP)
        {
            /*
            * URGENT VAL TODO: Either we need to position the stream to the
            * next sibling, or walk the whole subtree.
            */
            skip[0] = 1;
            return 0;
        }
        {
            Xsd10Element decl = getElem(vctxt.schema, vctxt.inode.localName, vctxt.inode.nsName);
            if (decl != null)
            {
                setDecl(vctxt.inode, decl);
                return 0;
            }
        }
        if (wild.processContents == XML_XSD10_SCHEMAS_ANY_STRICT)
        {
            /* VAL TODO: Change to proper error code. */
            verror(vctxt, XML_XSD10_SCHEMAV_CVC_ELT_1, null, /* WXS_BASIC_CAST wild */
                "No matching global element declaration available, but " +
                "demanded by the strict wildcard");
            return vctxt.err;
        }
        if (vctxt.nbAttrInfos != 0)
        {
            Xsd10AttrInfo iattr;
            /*
            * SPEC Validation Rule: Schema-Validity Assessment (Element)
            * (1.2.1.2.1) - (1.2.1.2.3 )
            *
            * Use the xsi:type attribute for the type definition.
            */
            iattr = getMetaAttrInfo(vctxt, XML_XSD10_SCHEMA_ATTR_INFO_META_XSI_TYPE);
            if (iattr != null)
            {
                Xsd10NodeInfo ielem = vctxt.inode;
                Xsd10Type[] lt = new Xsd10Type[] { ielem.typeDef };
                int r = processXsiType(vctxt, iattr, lt, null);
                ielem.typeDef = lt[0];
                if (r == -1)
                {
                    verrorInt(vctxt, "xmlSchemaValidateElemWildcard",
                        "calling xmlSchemaProcessXSIType() to " +
                        "process the attribute 'xsi:nil'");
                    return -1;
                }
                /*
                * Don't return an error on purpose.
                */
                return 0;
            }
        }
        /*
        * SPEC Validation Rule: Schema-Validity Assessment (Element)
        *
        * Fallback to "anyType".
        */
        vctxt.inode.typeDef = getBuiltInType(XML_XSD10_SCHEMAS_ANYTYPE);
        return 0;
    }

    /*
    * xmlSchemaCheckCOSValidDefault:
    *
    * This will be called if: not nilled, no content and a default/fixed
    * value is provided.
    */
    // xml_xsd10_check_cos_valid_default
    /** Port of xmlSchemaCheckCOSValidDefault. */
    static int checkCosValidDefault(Xsd10ValidCtxt vctxt, String value, Xsd10Val[] val)
    {
        int ret = 0;
        Xsd10NodeInfo inode = vctxt.inode;

        /*
        * cos-valid-default:
        * Schema Component Constraint: Element Default Valid (Immediate)
        * For a string to be a valid default with respect to a type
        * definition the appropriate case among the following must be true:
        */
        if (wxsIsComplex(inode.typeDef) != 0)
        {
            /*
            * Complex type.
            *
            * SPEC (2.1) "its {content type} must be a simple type definition
            * or mixed."
            * SPEC (2.2.2) "If the {content type} is mixed, then the {content
            * type}'s particle must be `emptiable` as defined by
            * Particle Emptiable ($3.9.6)."
            */
            if ((wxsHasSimpleContent(inode.typeDef) == 0) &&
                ((wxsHasMixedContent(inode.typeDef) == 0) || (wxsEmptiable(inode.typeDef) == 0)))
            {
                ret = XML_XSD10_SCHEMAP_COS_VALID_DEFAULT_2_1;
                /* NOTE that this covers (2.2.2) as well. */
                verror(vctxt, ret, null,
                    "For a string to be a valid default, the type definition " +
                    "must be a simple type or a complex type with simple content " +
                    "or mixed content and a particle emptiable");
                return ret;
            }
        }
        /*
        * 1 If the type definition is a simple type definition, then the string
        * must be `valid` with respect to that definition as defined by String
        * Valid ($3.14.4).
        *
        * AND
        *
        * 2.2.1 If the {content type} is a simple type definition, then the
        * string must be `valid` with respect to that simple type definition
        * as defined by String Valid ($3.14.4).
        */
        if (wxsIsSimple(inode.typeDef) != 0)
        {
            ret = vCheckCvcSimpleType(vctxt, null, inode.typeDef, value, val, 1, 1, 0);
        }
        else if (wxsHasSimpleContent(inode.typeDef) != 0)
        {
            ret = vCheckCvcSimpleType(vctxt, null, inode.typeDef.contentTypeDef, value, val, 1, 1, 0);
        }
        if (ret < 0)
        {
            verrorInt(vctxt, "xmlSchemaCheckCOSValidDefault", "calling xmlSchemaVCheckCVCSimpleType()");
        }
        return ret;
    }

    // xml_xsd10_v_content_model_callback
    /** Port of xmlSchemaVContentModelCallback. */
    static void vContentModelCallback(Object exec, String name, Object transdata, Object inputdata)
    {
        Xsd10NodeInfo inode = (Xsd10NodeInfo) inputdata;
        setDecl(inode, transdata);
    }

    // xml_xsd10_validator_push_elem
    /** Port of xmlSchemaValidatorPushElem. */
    static int validatorPushElem(Xsd10ValidCtxt vctxt)
    {
        vctxt.inode = getFreshElemInfo(vctxt);
        if (vctxt.inode == null)
        {
            verrorInt(vctxt, "xmlSchemaValidatorPushElem", "calling xmlSchemaGetFreshElemInfo()");
            return -1;
        }
        vctxt.nbAttrInfos = 0;
        return 0;
    }

    // xml_xsd10_v_check_i_node_data_type
    /** Port of xmlSchemaVCheckINodeDataType. */
    static int vCheckINodeDataType(Xsd10ValidCtxt vctxt, Xsd10NodeInfo inode, Xsd10Type type, String value)
    {
        if ((inode.flags & XML_XSD10_SCHEMA_NODE_INFO_VALUE_NEEDED) != 0)
        {
            Xsd10Val[] v = new Xsd10Val[] { inode.val };
            int r = vCheckCvcSimpleType(vctxt, null, type, value, v, 1, 1, 0);
            inode.val = v[0];
            return r;
        }
        else
        {
            return vCheckCvcSimpleType(vctxt, null, type, value, null, 1, 0, 0);
        }
    }

    /** The content model callback as an XmlRegexp exec callback. */
    private static final XmlRegexp.ExecCallback CONTENT_MODEL_CALLBACK = (exec, token, transdata, inputdata) ->
    {
        if (inputdata != null)
        {
            vContentModelCallback(exec, token, transdata, inputdata);
        }
    };

    /*
    * Process END of element.
    */
    // xml_xsd10_validator_pop_elem
    /** Port of xmlSchemaValidatorPopElem. */
    static int validatorPopElem(Xsd10ValidCtxt vctxt)
    {
        int ret = 0;
        Xsd10NodeInfo inode = vctxt.inode;

        if (vctxt.nbAttrInfos != 0)
        {
            clearAttrInfos(vctxt);
        }
        end_elem:
        {
            if ((inode.flags & XML_XSD10_SCHEMA_NODE_INFO_ERR_NOT_EXPECTED) != 0)
            {
                /*
                * This element was not expected;
                * we will not validate child elements of broken parents.
                * Skip validation of all content of the parent.
                */
                vctxt.skipDepth = vctxt.depth - 1;
                break end_elem;
            }
            if ((inode.typeDef == null) || ((inode.flags & XML_XSD10_SCHEMA_NODE_INFO_ERR_BAD_TYPE) != 0))
            {
                /*
                * 1. the type definition might be missing if the element was
                *    error prone
                * 2. it might be abstract.
                */
                break end_elem;
            }
            boolean characterContent = false;
            /*
            * Check the content model.
            */
            if ((inode.typeDef.contentType == XML_XSD10_SCHEMA_CONTENT_MIXED) ||
                (inode.typeDef.contentType == XML_XSD10_SCHEMA_CONTENT_ELEMENTS))
            {
                /*
                * Workaround for "anyType".
                */
                if (inode.typeDef.builtInType == XML_XSD10_SCHEMAS_ANYTYPE)
                {
                    characterContent = true;
                }
                else if ((inode.flags & XML_XSD10_SCHEMA_ELEM_INFO_ERR_BAD_CONTENT) == 0)
                {
                    String[] values = new String[10];
                    int[] terminal = new int[] { 0 };
                    int[] nbval = new int[] { 10 };
                    int[] nbneg = new int[] { 0 };

                    skip_nilled:
                    {
                        if (inode.regexCtxt == null)
                        {
                            /*
                            * Create the regex context.
                            */
                            inode.regexCtxt = XmlRegexp.regExecNew((XmlRegexp) inode.typeDef.contModel, CONTENT_MODEL_CALLBACK, vctxt);
                            if (inode.regexCtxt == null)
                            {
                                verrorInt(vctxt, "xmlSchemaValidatorPopElem", "failed to create a regex context");
                                vctxt.err = -1;
                                return -1;
                            }
                        }

                        /*
                         * Do not check further content if the node has been nilled
                         */
                        if (inodeNilled(inode) != 0)
                        {
                            ret = 0;
                            break skip_nilled;
                        }

                        /*
                        * Get hold of the still expected content, since a further
                        * call to xmlRegExecPushString() will lose this information.
                        */
                        XmlRegexp.regExecNextValues(inode.regexCtxt, nbval, nbneg, values, terminal);
                        ret = XmlRegexp.regExecPushString(inode.regexCtxt, null, null);
                        if ((ret < 0) || ((ret == 0) && (inodeNilled(inode) == 0)))
                        {
                            /*
                            * Still missing something.
                            */
                            ret = 1;
                            inode.flags |= XML_XSD10_SCHEMA_ELEM_INFO_ERR_BAD_CONTENT;
                            complexTypeErr(vctxt, XML_XSD10_SCHEMAV_ELEMENT_CONTENT, null, null,
                                "Missing child element(s)",
                                nbval[0], nbneg[0], values);
                        }
                        else
                        {
                            /*
                            * Content model is satisfied.
                            */
                            ret = 0;
                        }
                    }
                }
            }

            if (!characterContent)
            {
                // skip_nilled:
                if (inode.typeDef.contentType == XML_XSD10_SCHEMA_CONTENT_ELEMENTS)
                {
                    break end_elem;
                }
            }

            // character_content:
            if (vctxt.value != null)
            {
                freeValue(vctxt.value);
                vctxt.value = null;
            }
            /*
            * Check character content.
            */
            if (inode.decl == null)
            {
                /*
                * Speedup if no declaration exists.
                */
                if (wxsIsSimple(inode.typeDef) != 0)
                {
                    ret = vCheckINodeDataType(vctxt, inode, inode.typeDef, inode.value);
                }
                else if (wxsHasSimpleContent(inode.typeDef) != 0)
                {
                    ret = vCheckINodeDataType(vctxt, inode, inode.typeDef.contentTypeDef, inode.value);
                }
                if (ret < 0)
                {
                    verrorInt(vctxt, "xmlSchemaValidatorPopElem", "calling xmlSchemaVCheckCVCSimpleType()");
                    vctxt.err = -1;
                    return -1;
                }
                break end_elem;
            }
            /*
            * cvc-elt (3.3.4) : 5
            * The appropriate case among the following must be true:
            */
            /*
            * cvc-elt (3.3.4) : 5.1
            * If the declaration has a {value constraint},
            * the item has neither element nor character [children] and
            * clause 3.2 has not applied, then all of the following must be true:
            */
            if ((inode.decl.value != null) &&
                ((inode.flags & XML_XSD10_SCHEMA_ELEM_INFO_EMPTY) != 0) &&
                (inodeNilled(inode) == 0))
            {
                /*
                * cvc-elt (3.3.4) : 5.1.1
                * If the `actual type definition` is a `local type definition`
                * then the canonical lexical representation of the {value constraint}
                * value must be a valid default for the `actual type definition` as
                * defined in Element Default Valid (Immediate) ($3.3.6).
                */
                /*
                * NOTE: 'local' above means types acquired by xsi:type.
                * NOTE: Although the *canonical* value is stated, it is not
                * relevant if canonical or not. Additionally XML Schema 1.1
                * will removed this requirement as well.
                */
                if ((inode.flags & XML_XSD10_SCHEMA_ELEM_INFO_LOCAL_TYPE) != 0)
                {
                    Xsd10Val[] v = new Xsd10Val[] { inode.val };
                    ret = checkCosValidDefault(vctxt, inode.decl.value, v);
                    inode.val = v[0];
                    if (ret != 0)
                    {
                        if (ret < 0)
                        {
                            verrorInt(vctxt, "xmlSchemaValidatorPopElem", "calling xmlSchemaCheckCOSValidDefault()");
                            vctxt.err = -1;
                            return -1;
                        }
                        break end_elem;
                    }
                    /*
                    * Stop here, to avoid redundant validation of the value
                    * (see following).
                    */
                    // goto default_psvi;
                }
                else
                {
                    /*
                    * cvc-elt (3.3.4) : 5.1.2
                    * The element information item with the canonical lexical
                    * representation of the {value constraint} value used as its
                    * `normalized value` must be `valid` with respect to the
                    * `actual type definition` as defined by Element Locally Valid (Type)
                    * ($3.3.4).
                    */
                    if (wxsIsSimple(inode.typeDef) != 0)
                    {
                        ret = vCheckINodeDataType(vctxt, inode, inode.typeDef, inode.decl.value);
                    }
                    else if (wxsHasSimpleContent(inode.typeDef) != 0)
                    {
                        ret = vCheckINodeDataType(vctxt, inode, inode.typeDef.contentTypeDef, inode.decl.value);
                    }
                    if (ret != 0)
                    {
                        if (ret < 0)
                        {
                            verrorInt(vctxt, "xmlSchemaValidatorPopElem", "calling xmlSchemaVCheckCVCSimpleType()");
                            vctxt.err = -1;
                            return -1;
                        }
                        break end_elem;
                    }
                }

                // default_psvi:
                /*
                * PSVI: Create a text node on the instance element.
                */
                if (((vctxt.options & XML_XSD10_SCHEMA_VAL_VC_I_CREATE) != 0) && (inode.node != null))
                {
                    XmlNode textChild;
                    String normValue;
                    /*
                    * VAL TODO: Normalize the value.
                    */
                    normValue = normalizeValue(inode.typeDef, inode.decl.value);
                    if (normValue != null)
                    {
                        textChild = nodeNewText(nodeDom(inode.node), normValue);
                    }
                    else
                    {
                        textChild = nodeNewText(nodeDom(inode.node), inode.decl.value);
                    }
                    if (textChild == null)
                    {
                        verrorInt(vctxt, "xmlSchemaValidatorPopElem", "calling xmlNewDocText()");
                        vctxt.err = -1;
                        return -1;
                    }
                    else
                    {
                        nodeAddChild(inode.node, textChild);
                    }
                }
            }
            else if (inodeNilled(inode) == 0)
            {
                /*
                * 5.2.1 The element information item must be `valid` with respect
                * to the `actual type definition` as defined by Element Locally
                * Valid (Type) ($3.3.4).
                */
                if (wxsIsSimple(inode.typeDef) != 0)
                {
                    /*
                    * SPEC (cvc-type) (3.1)
                    * "If the type definition is a simple type definition, ..."
                    * (3.1.3) "If clause 3.2 of Element Locally Valid
                    * (Element) ($3.3.4) did not apply, then the `normalized value`
                    * must be `valid` with respect to the type definition as defined
                    * by String Valid ($3.14.4).
                    */
                    ret = vCheckINodeDataType(vctxt, inode, inode.typeDef, inode.value);
                }
                else if (wxsHasSimpleContent(inode.typeDef) != 0)
                {
                    /*
                    * SPEC (cvc-type) (3.2) "If the type definition is a complex type
                    * definition, then the element information item must be
                    * `valid` with respect to the type definition as per
                    * Element Locally Valid (Complex Type) ($3.4.4);"
                    *
                    * SPEC (cvc-complex-type) (2.2)
                    * "If the {content type} is a simple type definition, ...
                    * the `normalized value` of the element information item is
                    * `valid` with respect to that simple type definition as
                    * defined by String Valid ($3.14.4)."
                    */
                    ret = vCheckINodeDataType(vctxt, inode, inode.typeDef.contentTypeDef, inode.value);
                }
                if (ret != 0)
                {
                    if (ret < 0)
                    {
                        verrorInt(vctxt, "xmlSchemaValidatorPopElem", "calling xmlSchemaVCheckCVCSimpleType()");
                        vctxt.err = -1;
                        return -1;
                    }
                    break end_elem;
                }
                /*
                * 5.2.2 If there is a fixed {value constraint} and clause 3.2 has
                * not applied, all of the following must be true:
                */
                if ((inode.decl.value != null) && ((inode.decl.flags & XML_XSD10_SCHEMAS_ELEM_FIXED) != 0))
                {
                    /*
                    * TODO: We will need a computed value, when comparison is
                    * done on computed values.
                    */
                    /*
                    * 5.2.2.1 The element information item must have no element
                    * information item [children].
                    */
                    if ((inode.flags & XML_XSD10_SCHEMA_ELEM_INFO_HAS_ELEM_CONTENT) != 0)
                    {
                        ret = XML_XSD10_SCHEMAV_CVC_ELT_5_2_2_1;
                        verror(vctxt, ret, null,
                            "The content must not contain element nodes since " +
                            "there is a fixed value constraint");
                        break end_elem;
                    }
                    else
                    {
                        /*
                        * 5.2.2.2 The appropriate case among the following must
                        * be true:
                        */
                        if (wxsHasMixedContent(inode.typeDef) != 0)
                        {
                            /*
                            * 5.2.2.2.1 If the {content type} of the `actual type
                            * definition` is mixed, then the *initial value* of the
                            * item must match the canonical lexical representation
                            * of the {value constraint} value.
                            *
                            * ... the *initial value* of an element information
                            * item is the string composed of, in order, the
                            * [character code] of each character information item in
                            * the [children] of that element information item.
                            */
                            if (strEqual(inode.value, inode.decl.value) == 0)
                            {
                                /*
                                * VAL TODO: Report invalid & expected values as well.
                                * VAL TODO: Implement the canonical stuff.
                                */
                                ret = XML_XSD10_SCHEMAV_CVC_ELT_5_2_2_2_1;
                                customErr(vctxt, ret, null, null,
                                    "The initial value '%s' does not match the fixed " +
                                    "value constraint '%s'",
                                    inode.value, inode.decl.value);
                                break end_elem;
                            }
                        }
                        else if (wxsHasSimpleContent(inode.typeDef) != 0)
                        {
                            /*
                            * 5.2.2.2.2 If the {content type} of the `actual type
                            * definition` is a simple type definition, then the
                            * *actual value* of the item must match the canonical
                            * lexical representation of the {value constraint} value.
                            */
                            /*
                            * VAL TODO: *actual value* is the normalized value, impl.
                            *           this.
                            * VAL TODO: Report invalid & expected values as well.
                            * VAL TODO: Implement a comparison with the computed values.
                            */
                            if (strEqual(inode.value, inode.decl.value) == 0)
                            {
                                ret = XML_XSD10_SCHEMAV_CVC_ELT_5_2_2_2_2;
                                customErr(vctxt, ret, null, null,
                                    "The actual value '%s' does not match the fixed " +
                                    "value constraint '%s'",
                                    inode.value, inode.decl.value);
                                break end_elem;
                            }
                        }
                    }
                }
            }
        }

        // end_elem:
        if (vctxt.depth < 0)
        {
            /* TODO: raise error? */
            return 0;
        }
        if (vctxt.depth == vctxt.skipDepth)
        {
            vctxt.skipDepth = -1;
        }
        /*
        * Evaluate the history of XPath state objects.
        */
        if ((inode.appliedXPath != 0) && (xPathProcessHistory(vctxt, vctxt.depth) == -1))
        {
            vctxt.err = -1;
            return -1;
        }
        /*
        * MAYBE TODO:
        * SPEC (6) "The element information item must be `valid` with
        * respect to each of the {identity-constraint definitions} as per
        * Identity-constraint Satisfied ($3.11.4)."
        */
        /*
        * PSVI TODO: If we expose IDC node-tables via PSVI then the tables
        *   need to be built in any case.
        *   We will currently build IDC node-tables and bubble them only if
        *   keyrefs do exist.
        */

        /*
        * Add the current IDC target-nodes to the IDC node-tables.
        */
        if ((inode.idcMatchers != null) && ((vctxt.hasKeyrefs != 0) || (vctxt.createIDCNodeTables != 0)))
        {
            if (idcFillNodeTables(vctxt, inode) == -1)
            {
                vctxt.err = -1;
                return -1;
            }
        }
        /*
        * Validate IDC keyrefs.
        */
        if (vctxt.inode.hasKeyrefs != 0)
        {
            if (checkCvcidcKeyRef(vctxt) == -1)
            {
                vctxt.err = -1;
                return -1;
            }
        }
        /*
        * Merge/free the IDC table.
        */
        if (inode.idcTable != null)
        {
            if ((vctxt.depth > 0) && ((vctxt.hasKeyrefs != 0) || (vctxt.createIDCNodeTables != 0)))
            {
                /*
                * Merge the IDC node table with the table of the parent node.
                */
                if (bubbleIdcNodeTables(vctxt) == -1)
                {
                    vctxt.err = -1;
                    return -1;
                }
            }
        }
        /*
        * Clear the current ielem.
        * VAL TODO: Don't free the PSVI IDC tables if they are
        * requested for the PSVI.
        */
        clearElemInfo(vctxt, inode);
        /*
        * Skip further processing if we are on the validation root.
        */
        if (vctxt.depth == 0)
        {
            vctxt.depth--;
            vctxt.inode = null;
            return 0;
        }
        /*
        * Reset the keyrefDepth if needed.
        */
        if (vctxt.aidcs != null)
        {
            Xsd10IDCAug aidc = vctxt.aidcs;
            do
            {
                if (aidc.keyrefDepth == vctxt.depth)
                {
                    /*
                    * A 'keyrefDepth' of a key/unique IDC matches the current
                    * depth, this means that we are leaving the scope of the
                    * top-most keyref IDC which refers to this IDC.
                    */
                    aidc.keyrefDepth = -1;
                }
                aidc = aidc.next;
            } while (aidc != null);
        }
        vctxt.depth--;
        vctxt.inode = vctxt.elemInfos[vctxt.depth];
        /*
        * VAL TODO: 7 If the element information item is the `validation root`, it must be
        * `valid` per Validation Root Valid (ID/IDREF) ($3.3.4).
        */
        return ret;
    }

    /*
    * 3.4.4 Complex Type Definition Validation Rules
    * Validation Rule: Element Locally Valid (Complex Type) (cvc-complex-type)
    */
    // xml_xsd10_validate_child_elem
    /** Port of xmlSchemaValidateChildElem. */
    static int validateChildElem(Xsd10ValidCtxt vctxt)
    {
        Xsd10NodeInfo pielem;
        Xsd10Type ptype;
        int ret = 0;

        if (vctxt.depth <= 0)
        {
            verrorInt(vctxt, "xmlSchemaValidateChildElem", "not intended for the validation root");
            return -1;
        }
        pielem = vctxt.elemInfos[vctxt.depth - 1];
        if ((pielem.flags & XML_XSD10_SCHEMA_ELEM_INFO_EMPTY) != 0)
        {
            pielem.flags ^= XML_XSD10_SCHEMA_ELEM_INFO_EMPTY;
        }
        unexpected_elem:
        {
            /*
            * Handle 'nilled' elements.
            */
            if (inodeNilled(pielem) != 0)
            {
                /*
                * SPEC (cvc-elt) (3.3.4) : (3.2.1)
                */
                activateParentElem(vctxt);
                ret = XML_XSD10_SCHEMAV_CVC_ELT_3_2_1;
                verror(vctxt, ret, null,
                    "Neither character nor element content is allowed, " +
                    "because the element was 'nilled'");
                activateElem(vctxt);
                break unexpected_elem;
            }

            ptype = pielem.typeDef;

            if (ptype.builtInType == XML_XSD10_SCHEMAS_ANYTYPE)
            {
                /*
                * Workaround for "anyType": we have currently no content model
                * assigned for "anyType", so handle it explicitly.
                * "anyType" has an unbounded, lax "any" wildcard.
                */
                setDecl(vctxt.inode, getElem(vctxt.schema, vctxt.inode.localName, vctxt.inode.nsName));

                if (getDecl(vctxt.inode) == null)
                {
                    Xsd10AttrInfo iattr;
                    /*
                    * Process "xsi:type".
                    * SPEC (cvc-assess-elt) (1.2.1.2.1) - (1.2.1.2.3)
                    */
                    iattr = getMetaAttrInfo(vctxt, XML_XSD10_SCHEMA_ATTR_INFO_META_XSI_TYPE);
                    if (iattr != null)
                    {
                        Xsd10NodeInfo ielem = vctxt.inode;
                        Xsd10Type[] lt = new Xsd10Type[] { ielem.typeDef };
                        ret = processXsiType(vctxt, iattr, lt, null);
                        ielem.typeDef = lt[0];
                        if (ret != 0)
                        {
                            if (ret == -1)
                            {
                                verrorInt(vctxt, "xmlSchemaValidateChildElem",
                                    "calling xmlSchemaProcessXSIType() to " +
                                    "process the attribute 'xsi:nil'");
                                return -1;
                            }
                            return ret;
                        }
                    }
                    else
                    {
                        /*
                        * Fallback to "anyType".
                        *
                        * SPEC (cvc-assess-elt)
                        * "If the item cannot be `strictly assessed`, [...]
                        * an element information item's schema validity may be laxly
                        * assessed if its `context-determined declaration` is not
                        * skip by `validating` with respect to the `ur-type
                        * definition` as per Element Locally Valid (Type) ($3.3.4)."
                        */
                        vctxt.inode.typeDef = getBuiltInType(XML_XSD10_SCHEMAS_ANYTYPE);
                    }
                }
                return 0;
            }

            switch (ptype.contentType)
            {
                case XML_XSD10_SCHEMA_CONTENT_EMPTY:
                    /*
                    * SPEC (2.1) "If the {content type} is empty, then the
                    * element information item has no character or element
                    * information item [children]."
                    */
                    activateParentElem(vctxt);
                    ret = XML_XSD10_SCHEMAV_CVC_COMPLEX_TYPE_2_1;
                    verror(vctxt, ret, null,
                        "Element content is not allowed, " +
                        "because the content type is empty");
                    activateElem(vctxt);
                    break unexpected_elem;

                case XML_XSD10_SCHEMA_CONTENT_MIXED:
                case XML_XSD10_SCHEMA_CONTENT_ELEMENTS:
                {
                    XmlRegExecCtxt regexCtxt;
                    String[] values = new String[10];
                    int[] terminal = new int[] { 0 };
                    int[] nbval = new int[] { 10 };
                    int[] nbneg = new int[] { 0 };

                    /* VAL TODO: Optimized "anyType" validation.*/

                    if (ptype.contModel == null)
                    {
                        verrorInt(vctxt, "xmlSchemaValidateChildElem", "type has elem content but no content model");
                        return -1;
                    }
                    /*
                    * Safety belt for evaluation if the cont. model was already
                    * examined to be invalid.
                    */
                    if ((pielem.flags & XML_XSD10_SCHEMA_ELEM_INFO_ERR_BAD_CONTENT) != 0)
                    {
                        verrorInt(vctxt, "xmlSchemaValidateChildElem", "validating elem, but elem content is already invalid");
                        return -1;
                    }

                    regexCtxt = pielem.regexCtxt;
                    if (regexCtxt == null)
                    {
                        /*
                        * Create the regex context.
                        */
                        regexCtxt = XmlRegexp.regExecNew((XmlRegexp) ptype.contModel, CONTENT_MODEL_CALLBACK, vctxt);
                        if (regexCtxt == null)
                        {
                            verrorInt(vctxt, "xmlSchemaValidateChildElem", "failed to create a regex context");
                            return -1;
                        }
                        pielem.regexCtxt = regexCtxt;
                    }

                    /*
                    * SPEC (2.4) "If the {content type} is element-only or mixed,
                    * then the sequence of the element information item's
                    * element information item [children], if any, taken in
                    * order, is `valid` with respect to the {content type}'s
                    * particle, as defined in Element Sequence Locally Valid
                    * (Particle) ($3.9.4)."
                    */
                    ret = XmlRegexp.regExecPushString2(regexCtxt, vctxt.inode.localName, vctxt.inode.nsName, vctxt.inode);
                    if (vctxt.err == XML_XSD10_SCHEMAV_INTERNAL)
                    {
                        verrorInt(vctxt, "xmlSchemaValidateChildElem", "calling xmlRegExecPushString2()");
                        return -1;
                    }
                    if (ret < 0)
                    {
                        XmlRegexp.regExecErrInfo(regexCtxt, null, nbval, nbneg, values, terminal);
                        complexTypeErr(vctxt, XML_XSD10_SCHEMAV_ELEMENT_CONTENT, null, null,
                            "This element is not expected",
                            nbval[0], nbneg[0], values);
                        ret = vctxt.err;
                        break unexpected_elem;
                    }
                    else
                    {
                        ret = 0;
                    }
                    break;
                }
                case XML_XSD10_SCHEMA_CONTENT_SIMPLE:
                case XML_XSD10_SCHEMA_CONTENT_BASIC:
                    activateParentElem(vctxt);
                    if (wxsIsComplex(ptype) != 0)
                    {
                        /*
                        * SPEC (cvc-complex-type) (2.2)
                        * "If the {content type} is a simple type definition, then
                        * the element information item has no element information
                        * item [children], ..."
                        */
                        ret = XML_XSD10_SCHEMAV_CVC_COMPLEX_TYPE_2_2;
                        verror(vctxt, ret, null, "Element content is not allowed, " +
                            "because the content type is a simple type definition");
                    }
                    else
                    {
                        /*
                        * SPEC (cvc-type) (3.1.2) "The element information item must
                        * have no element information item [children]."
                        */
                        ret = XML_XSD10_SCHEMAV_CVC_TYPE_3_1_2;
                        verror(vctxt, ret, null, "Element content is not allowed, " +
                            "because the type definition is simple");
                    }
                    activateElem(vctxt);
                    ret = vctxt.err;
                    break unexpected_elem;

                default:
                    break;
            }
            return ret;
        }
        // unexpected_elem:
        /*
        * Pop this element and set the skipDepth to skip
        * all further content of the parent element.
        */
        vctxt.skipDepth = vctxt.depth;
        vctxt.inode.flags |= XML_XSD10_SCHEMA_NODE_INFO_ERR_NOT_EXPECTED;
        pielem.flags |= XML_XSD10_SCHEMA_ELEM_INFO_ERR_BAD_CONTENT;
        return ret;
    }

    private static final int XML_XSD10_SCHEMA_PUSH_TEXT_PERSIST = 1;
    private static final int XML_XSD10_SCHEMA_PUSH_TEXT_CREATED = 2;
    private static final int XML_XSD10_SCHEMA_PUSH_TEXT_VOLATILE = 3;

    // xml_xsd10_v_push_text
    /** Port of xmlSchemaVPushText. */
    static int vPushText(Xsd10ValidCtxt vctxt, int nodeType, String value, int len, int mode, int[] consumed)
    {
        /*
        * Unfortunately we have to duplicate the text sometimes.
        * OPTIMIZE: Maybe we could skip it, if:
        *   1. content type is simple
        *   2. whitespace is "collapse"
        *   3. it consists of whitespace only
        *
        * Process character content.
        */
        if (consumed != null)
        {
            consumed[0] = 0;
        }
        if (inodeNilled(vctxt.inode) != 0)
        {
            /*
            * SPEC cvc-elt (3.3.4 - 3.2.1)
            * "The element information item must have no character or
            * element information item [children]."
            */
            verror(vctxt, XML_XSD10_SCHEMAV_CVC_ELT_3_2_1, null,
                "Neither character nor element content is allowed " +
                "because the element is 'nilled'");
            return vctxt.err;
        }
        /*
        * SPEC (2.1) "If the {content type} is empty, then the
        * element information item has no character or element
        * information item [children]."
        */
        if (vctxt.inode.typeDef.contentType == XML_XSD10_SCHEMA_CONTENT_EMPTY)
        {
            verror(vctxt, XML_XSD10_SCHEMAV_CVC_COMPLEX_TYPE_2_1, null,
                "Character content is not allowed, " +
                "because the content type is empty");
            return vctxt.err;
        }

        if (vctxt.inode.typeDef.contentType == XML_XSD10_SCHEMA_CONTENT_ELEMENTS)
        {
            if ((nodeType != TEXT) || (isBlank(value, len) == 0))
            {
                /*
                * SPEC cvc-complex-type (2.3)
                * "If the {content type} is element-only, then the
                * element information item has no character information
                * item [children] other than those whose [character
                * code] is defined as a white space in [XML 1.0 (Second
                * Edition)]."
                */
                verror(vctxt, XML_XSD10_SCHEMAV_CVC_COMPLEX_TYPE_2_3, null,
                    "Character content other than whitespace is not allowed " +
                    "because the content type is 'element-only'");
                return vctxt.err;
            }
            return 0;
        }

        if ((value == null) || value.isEmpty())
        {
            return 0;
        }
        /*
        * Save the value.
        * NOTE that even if the content type is *mixed*, we need the
        * *initial value* for default/fixed value constraints.
        */
        if ((vctxt.inode.typeDef.contentType == XML_XSD10_SCHEMA_CONTENT_MIXED) &&
            ((vctxt.inode.decl == null) || (vctxt.inode.decl.value == null)))
        {
            return 0;
        }

        if (vctxt.inode.value == null)
        {
            /*
            * Set the value.
            */
            switch (mode)
            {
                case XML_XSD10_SCHEMA_PUSH_TEXT_PERSIST:
                    /*
                    * When working on a tree.
                    */
                    vctxt.inode.value = value;
                    vctxt.inode.flags &= ~XML_XSD10_SCHEMA_NODE_INFO_FLAG_OWNED_VALUES;
                    break;
                case XML_XSD10_SCHEMA_PUSH_TEXT_CREATED:
                    /*
                    * When working with the reader.
                    * The value will be freed by the element info.
                    */
                    vctxt.inode.value = value;
                    if (consumed != null)
                    {
                        consumed[0] = 1;
                    }
                    vctxt.inode.flags |= XML_XSD10_SCHEMA_NODE_INFO_FLAG_OWNED_VALUES;
                    break;
                case XML_XSD10_SCHEMA_PUSH_TEXT_VOLATILE:
                    /*
                    * When working with SAX.
                    * The value will be freed by the element info.
                    */
                    if (len != -1)
                    {
                        vctxt.inode.value = strndup(value, len);
                    }
                    else
                    {
                        vctxt.inode.value = strdup(value);
                    }
                    vctxt.inode.flags |= XML_XSD10_SCHEMA_NODE_INFO_FLAG_OWNED_VALUES;
                    break;
                default:
                    break;
            }
        }
        else
        {
            if (len < 0)
            {
                len = strlen(value);
            }
            /*
            * Concat the value.
            */
            if ((vctxt.inode.flags & XML_XSD10_SCHEMA_NODE_INFO_FLAG_OWNED_VALUES) != 0)
            {
                vctxt.inode.value = strncat(vctxt.inode.value, value, len);
            }
            else
            {
                vctxt.inode.value = strncatNew(vctxt.inode.value, value, len);
                vctxt.inode.flags |= XML_XSD10_SCHEMA_NODE_INFO_FLAG_OWNED_VALUES;
            }
        }

        return 0;
    }

    // xml_xsd10_validate_elem
    /** Port of xmlSchemaValidateElem. */
    static int validateElem(Xsd10ValidCtxt vctxt)
    {
        int ret = 0;

        if ((vctxt.skipDepth != -1) && (vctxt.depth >= vctxt.skipDepth))
        {
            verrorInt(vctxt, "xmlSchemaValidateElem", "in skip-state");
            return -1;
        }
        exit:
        {
            if (vctxt.xsiAssemble != 0)
            {
                /*
                * We will stop validation if there was an error during
                * dynamic schema construction.
                * Note that we simply set @skipDepth to 0, this could
                * mean that a streaming document via SAX would be
                * still read to the end but it won't be validated any more.
                * TODO: If we are sure how to stop the validation at once
                *   for all input scenarios, then this should be changed to
                *   instantly stop the validation.
                */
                ret = assembleByXsi(vctxt);
                if (ret != 0)
                {
                    if (ret == -1)
                    {
                        return -1;
                    }
                    vctxt.skipDepth = 0;
                    return ret;
                }
                /*
                 * Augment the IDC definitions for the main schema and all imported ones
                 * NOTE: main schema is the first in the imported list
                 */
                hashScan(vctxt.schema.schemasImports, Xsd10Valid::augmentImportedIdc, vctxt);
            }
            if (vctxt.depth > 0)
            {
                /*
                * Validate this element against the content model
                * of the parent.
                */
                ret = validateChildElem(vctxt);
                if (ret != 0)
                {
                    if (ret < 0)
                    {
                        verrorInt(vctxt, "xmlSchemaValidateElem", "calling xmlSchemaStreamValidateChildElement()");
                        return -1;
                    }
                    break exit;
                }
                if (vctxt.depth == vctxt.skipDepth)
                {
                    break exit;
                }
                if ((getDecl(vctxt.inode) == null) && (vctxt.inode.typeDef == null))
                {
                    verrorInt(vctxt, "xmlSchemaValidateElem",
                        "the child element was valid but neither the " +
                        "declaration nor the type was set");
                    return -1;
                }
            }
            else
            {
                /*
                * Get the declaration of the validation root.
                */
                setDecl(vctxt.inode, getElem(vctxt.schema, vctxt.inode.localName, vctxt.inode.nsName));
                if (getDecl(vctxt.inode) == null)
                {
                    ret = XML_XSD10_SCHEMAV_CVC_ELT_1;
                    verror(vctxt, ret, null,
                        "No matching global declaration available " +
                        "for the validation root");
                    break exit;
                }
            }

            type_validation:
            {
                if (getDecl(vctxt.inode) == null)
                {
                    break type_validation;
                }

                if (getDecl(vctxt.inode).type == XML_XSD10_SCHEMA_TYPE_ANY)
                {
                    int[] skip = new int[] { 0 };
                    /*
                    * Wildcards.
                    */
                    ret = validateElemWildcard(vctxt, skip);
                    if (ret != 0)
                    {
                        if (ret < 0)
                        {
                            verrorInt(vctxt, "xmlSchemaValidateElem", "calling xmlSchemaValidateElemWildcard()");
                            return -1;
                        }
                        break exit;
                    }
                    if (skip[0] != 0)
                    {
                        vctxt.skipDepth = vctxt.depth;
                        break exit;
                    }
                    /*
                    * The declaration might be set by the wildcard validation,
                    * when the processContents is "lax" or "strict".
                    */
                    if (getDecl(vctxt.inode).type != XML_XSD10_SCHEMA_TYPE_ELEMENT)
                    {
                        /*
                        * Clear the "decl" field to not confuse further processing.
                        */
                        setDecl(vctxt.inode, null);
                        break type_validation;
                    }
                }
                /*
                * Validate against the declaration.
                */
                ret = validateElemDecl(vctxt);
                if (ret != 0)
                {
                    if (ret < 0)
                    {
                        verrorInt(vctxt, "xmlSchemaValidateElem", "calling xmlSchemaValidateElemDecl()");
                        return -1;
                    }
                    break exit;
                }
            }
            /*
            * Validate against the type definition.
            */
            // type_validation:

            if (vctxt.inode.typeDef == null)
            {
                vctxt.inode.flags |= XML_XSD10_SCHEMA_NODE_INFO_ERR_BAD_TYPE;
                ret = XML_XSD10_SCHEMAV_CVC_TYPE_1;
                verror(vctxt, ret, null, "The type definition is absent");
                break exit;
            }
            if ((vctxt.inode.typeDef.flags & XML_XSD10_SCHEMAS_TYPE_ABSTRACT) != 0)
            {
                vctxt.inode.flags |= XML_XSD10_SCHEMA_NODE_INFO_ERR_BAD_TYPE;
                ret = XML_XSD10_SCHEMAV_CVC_TYPE_2;
                verror(vctxt, ret, null, "The type definition is abstract");
                break exit;
            }
            /*
            * Evaluate IDCs. Do it here, since new IDC matchers are registered
            * during validation against the declaration. This must be done
            * _before_ attribute validation.
            */
            if (vctxt.xpathStates != null)
            {
                ret = xPathEvaluate(vctxt, ELEMENT);
                vctxt.inode.appliedXPath = 1;
                if (ret == -1)
                {
                    verrorInt(vctxt, "xmlSchemaValidateElem", "calling xmlSchemaXPathEvaluate()");
                    return -1;
                }
            }
            /*
            * Validate attributes.
            */
            if (wxsIsComplex(vctxt.inode.typeDef) != 0)
            {
                if ((vctxt.nbAttrInfos != 0) || (vctxt.inode.typeDef.attrUses != null))
                {
                    ret = vAttributesComplex(vctxt);
                }
            }
            else if (vctxt.nbAttrInfos != 0)
            {
                ret = vAttributesSimple(vctxt);
            }
            /*
            * Clear registered attributes.
            */
            if (vctxt.nbAttrInfos != 0)
            {
                clearAttrInfos(vctxt);
            }
            if (ret == -1)
            {
                verrorInt(vctxt, "xmlSchemaValidateElem", "calling attributes validation");
                return -1;
            }
            /*
            * Don't return an error if attributes are invalid on purpose.
            */
            ret = 0;
        }
        // exit:
        if (ret != 0)
        {
            vctxt.skipDepth = vctxt.depth;
        }
        return ret;
    }

    /* ***********************************************************************
     *                                                                      *
     *                      Validation interfaces                           *
     *                                                                      *
     ************************************************************************/

    // xml_xsd10_new_valid_ctxt
    /**
     * Port of xmlSchemaNewValidCtxt. Create an XML Schemas validation context based on the given schema.
     *
     * @param schema a precompiled XML Schemas
     * @return the validation context or null in case of error
     */
    public static Xsd10ValidCtxt newValidCtxt(Xsd10Schema schema)
    {
        Xsd10ValidCtxt ret = new Xsd10ValidCtxt();
        ret.type = XML_XSD10_SCHEMA_CTXT_VALIDATOR;
        ret.dict = dictCreate();
        if (ret.dict == null)
        {
            freeValidCtxt(ret);
            return null;
        }
        ret.nodeQNames = itemListCreate();
        if (ret.nodeQNames == null)
        {
            freeValidCtxt(ret);
            return null;
        }
        ret.schema = schema;
        return ret;
    }

    // xml_xsd10_validate_set_filename
    /**
     * Port of xmlSchemaValidateSetFilename. Workaround to provide file error reporting information
     * when this is not provided by current APIs.
     *
     * @param vctxt the schema validation context
     * @param filename the file name
     */
    public static void validateSetFilename(Xsd10ValidCtxt vctxt, String filename)
    {
        if (vctxt == null)
        {
            return;
        }
        if (filename != null)
        {
            vctxt.filename = strdup(filename);
        }
        else
        {
            vctxt.filename = null;
        }
    }

    // xml_xsd10_clear_valid_ctxt
    /**
     * Port of xmlSchemaClearValidCtxt. Free the resources associated to the schema validation context;
     * leaves some fields alive intended for reuse of the context.
     */
    static void clearValidCtxt(Xsd10ValidCtxt vctxt)
    {
        if (vctxt == null)
        {
            return;
        }

        /*
        * TODO: Should we clear the flags?
        *   Might be problematic if one reuses the context
        *   and assumes that the options remain the same.
        */
        vctxt.flags = 0;
        vctxt.validationRoot = null;
        vctxt.doc = null;
        /* (The reader field is not ported.) */
        vctxt.hasKeyrefs = 0;
        /* The ID table of the validated document (libxml2 keeps them in doc->ids). */
        if (vctxt.ids != null)
        {
            idsFree(vctxt.ids);
            vctxt.ids = null;
        }

        if (vctxt.value != null)
        {
            freeValue(vctxt.value);
            vctxt.value = null;
        }
        /*
        * Augmented IDC information.
        */
        vctxt.aidcs = null;

        if (vctxt.idcNodes != null)
        {
            vctxt.idcNodes = null;
            vctxt.nbIdcNodes = 0;
            vctxt.sizeIdcNodes = 0;
        }

        if (vctxt.idcKeys != null)
        {
            for (int i = 0; i < vctxt.nbIdcKeys; i++)
            {
                idcFreeKey(vctxt.idcKeys[i]);
            }
            vctxt.idcKeys = null;
            vctxt.nbIdcKeys = 0;
            vctxt.sizeIdcKeys = 0;
        }

        /*
        * Note that we won't delete the XPath state pool here.
        */
        if (vctxt.xpathStates != null)
        {
            freeIdcStateObjList(vctxt.xpathStates);
            vctxt.xpathStates = null;
        }
        /*
        * Attribute info.
        */
        if (vctxt.nbAttrInfos != 0)
        {
            clearAttrInfos(vctxt);
        }
        /*
        * Element info.
        */
        if (vctxt.elemInfos != null)
        {
            for (int i = 0; i < vctxt.sizeElemInfos; i++)
            {
                Xsd10NodeInfo ei = vctxt.elemInfos[i];
                if (ei == null)
                {
                    break;
                }
                clearElemInfo(vctxt, ei);
            }
        }
        itemListClear(vctxt.nodeQNames);
        /* Recreate the dict. */
        dictFree(vctxt.dict);
        /*
        * TODO: Is is save to recreate it? Do we have a scenario
        * where the user provides the dict?
        */
        vctxt.dict = dictCreate();

        vctxt.filename = null;

        /*
         * Note that some cleanup functions can move items to the cache,
         * so the cache shouldn't be freed too early.
         */
        if (vctxt.idcMatcherCache != null)
        {
            Xsd10IDCMatcher matcher = vctxt.idcMatcherCache;
            Xsd10IDCMatcher tmp;

            while (matcher != null)
            {
                tmp = matcher;
                matcher = matcher.nextCached;
                idcFreeMatcherList(tmp);
            }
            vctxt.idcMatcherCache = null;
        }
    }

    // xml_xsd10_free_valid_ctxt
    /**
     * Port of xmlSchemaFreeValidCtxt. Free the resources associated to the schema validation context.
     *
     * @param ctxt the schema validation context
     */
    public static void freeValidCtxt(Xsd10ValidCtxt ctxt)
    {
        if (ctxt == null)
        {
            return;
        }
        if (ctxt.value != null)
        {
            freeValue(ctxt.value);
        }
        if (ctxt.pctxt != null)
        {
            freeParserCtxt(ctxt.pctxt);
        }
        if (ctxt.idcKeys != null)
        {
            for (int i = 0; i < ctxt.nbIdcKeys; i++)
            {
                idcFreeKey(ctxt.idcKeys[i]);
            }
        }

        if (ctxt.xpathStates != null)
        {
            freeIdcStateObjList(ctxt.xpathStates);
            ctxt.xpathStates = null;
        }
        if (ctxt.xpathStatePool != null)
        {
            freeIdcStateObjList(ctxt.xpathStatePool);
            ctxt.xpathStatePool = null;
        }

        if (ctxt.attrInfos != null)
        {
            /* Just a paranoid call to the cleanup. */
            if (ctxt.nbAttrInfos != 0)
            {
                clearAttrInfos(ctxt);
            }
        }
        if (ctxt.elemInfos != null)
        {
            for (int i = 0; i < ctxt.sizeElemInfos; i++)
            {
                Xsd10NodeInfo ei = ctxt.elemInfos[i];
                if (ei == null)
                {
                    break;
                }
                clearElemInfo(ctxt, ei);
            }
        }
        if (ctxt.nodeQNames != null)
        {
            itemListFree(ctxt.nodeQNames);
        }
        if (ctxt.dict != null)
        {
            dictFree(ctxt.dict);
        }
        if (ctxt.ids != null)
        {
            idsFree(ctxt.ids);
        }
    }

    // xml_xsd10_v_doc_walk
    /**
     * Port of xmlSchemaVDocWalk.
     * Returns 1 if valid so far, 0 if errors were detected, and -1 in case of internal error.
     */
    static int vDocWalk(Xsd10ValidCtxt vctxt)
    {
        XmlAttr attr;
        int ret = 0;
        Xsd10NodeInfo ielem = null;
        XmlNode node;
        XmlNode valRoot;
        String nsName;

        /* DOC VAL TODO: Move this to the start function. */
        if (vctxt.validationRoot != null)
        {
            valRoot = vctxt.validationRoot;
        }
        else
        {
            valRoot = domRootElement(vctxt.doc);
        }
        if (valRoot == null)
        {
            /* VAL TODO: Error code? */
            verror(vctxt, 1, null, "The document has no document element");
            return 1;
        }
        vctxt.depth = -1;
        vctxt.validationRoot = valRoot;
        node = valRoot;
        /* 0: loop body, 1: leave_node, 2: next_sibling (the C gotos) */
        int state = 0;
        while (node != null)
        {
            if (state == 0)
            {
                state = 1;
                if ((vctxt.skipDepth != -1) && (vctxt.depth >= vctxt.skipDepth))
                {
                    state = 2;
                }
                else if (nodeKind(node) == ELEMENT)
                {
                    /*
                    * Init the node-info.
                    */
                    vctxt.depth++;
                    if (validatorPushElem(vctxt) == -1)
                    {
                        return -1;
                    }
                    ielem = vctxt.inode;
                    ielem.node = node;
                    ielem.nodeLine = nodeLine(node);
                    ielem.localName = nodeName(node);
                    if (nodeNs(node) != null)
                    {
                        ielem.nsName = nodeNsUri(node);
                    }
                    ielem.flags |= XML_XSD10_SCHEMA_ELEM_INFO_EMPTY;
                    /*
                    * Register attributes.
                    * DOC VAL TODO: We do not register namespace declaration
                    * attributes yet.
                    */
                    vctxt.nbAttrInfos = 0;
                    if (nodeFirstAttr(node) != null)
                    {
                        attr = nodeFirstAttr(node);
                        do
                        {
                            String content;

                            if (nodeNs(attrNode(attr)) != null)
                            {
                                nsName = nodeNsUri(attrNode(attr));
                            }
                            else
                            {
                                nsName = null;
                            }
                            content = treeNodeGetContent(attrNode(attr));
                            ret = validatorPushAttribute(vctxt,
                                attrNode(attr),
                                /*
                                * Note that we give it the line number of the
                                * parent element.
                                */
                                ielem.nodeLine,
                                attrName(attr), nsName, 0,
                                content, 1);
                            if (ret == -1)
                            {
                                verrorInt(vctxt, "xmlSchemaDocWalk", "calling xmlSchemaValidatorPushAttribute()");
                                return -1;
                            }
                            attr = attrNext(attr);
                        } while (attr != null);
                    }
                    /*
                    * Validate the element.
                    */
                    ret = validateElem(vctxt);
                    if (ret != 0)
                    {
                        if (ret == -1)
                        {
                            verrorInt(vctxt, "xmlSchemaDocWalk", "calling xmlSchemaValidateElem()");
                            return -1;
                        }
                        /*
                        * Don't stop validation; just skip the content
                        * of this element.
                        */
                        // goto leave_node
                    }
                    else if (!((vctxt.skipDepth != -1) && (vctxt.depth >= vctxt.skipDepth)))
                    {
                        state = 0;
                    }
                }
                else if ((nodeKind(node) == TEXT) || (nodeKind(node) == CDATA))
                {
                    /*
                    * Process character content.
                    */
                    if ((ielem != null) && ((ielem.flags & XML_XSD10_SCHEMA_ELEM_INFO_EMPTY) != 0))
                    {
                        ielem.flags ^= XML_XSD10_SCHEMA_ELEM_INFO_EMPTY;
                    }
                    ret = vPushText(vctxt, nodeKind(node), nodeContent(node), -1, XML_XSD10_SCHEMA_PUSH_TEXT_PERSIST, null);
                    if (ret < 0)
                    {
                        verrorInt(vctxt, "xmlSchemaVDocWalk", "calling xmlSchemaVPushText()");
                        return -1;
                    }
                    /*
                    * DOC VAL TODO: Should we skip further validation of the
                    * element content here?
                    */
                    state = 0;
                }
                else if (nodeKind(node) == ENTITY_REF)
                {
                    /* (node->type == XML_ENTITY_NODE) does not exist in the native dom */
                    /*
                    * DOC VAL TODO: What to do with entities?
                    */
                    verrorInt(vctxt, "xmlSchemaVDocWalk",
                        "there is at least one entity reference in the node-tree " +
                        "currently being validated. Processing of entities with " +
                        "this XML Schema processor is not supported (yet). Please " +
                        "substitute entities before validation.");
                    return -1;
                }
                else
                {
                    // goto leave_node;
                    /*
                    * DOC VAL TODO: XInclude nodes, etc.
                    */
                }
                if (state == 0)
                {
                    /*
                    * Walk the doc.
                    */
                    if (nodeFirstChild(node) != null)
                    {
                        node = nodeFirstChild(node);
                        continue;
                    }
                    state = 1;
                }
            }
            if (state == 1)
            {
                // leave_node:
                if (nodeKind(node) == ELEMENT)
                {
                    /*
                    * Leaving the scope of an element.
                    */
                    if ((vctxt.inode == null) || (node != vctxt.inode.node))
                    {
                        verrorInt(vctxt, "xmlSchemaVDocWalk", "element position mismatch");
                        return -1;
                    }
                    ret = validatorPopElem(vctxt);
                    if (ret != 0)
                    {
                        if (ret < 0)
                        {
                            verrorInt(vctxt, "xmlSchemaVDocWalk", "calling xmlSchemaValidatorPopElem()");
                            return -1;
                        }
                    }
                    if (node == valRoot)
                    {
                        return ret;
                    }
                }
                state = 2;
            }
            // next_sibling:
            if (nodeNext(node) != null)
            {
                node = nodeNext(node);
                state = 0;
            }
            else
            {
                node = nodeParent(node);
                state = 1;
            }
        }

        // exit:
        return ret;
    }

    // xml_xsd10_pre_run
    /** Port of xmlSchemaPreRun. */
    static int preRun(Xsd10ValidCtxt vctxt)
    {
        /*
        * Some initialization.
        */
        vctxt.err = 0;
        vctxt.nberrors = 0;
        vctxt.depth = -1;
        vctxt.skipDepth = -1;
        vctxt.hasKeyrefs = 0;
        vctxt.createIDCNodeTables = 0;
        /*
        * The ID table of the instance (replaces libxml2 doc->ids).
        */
        if (vctxt.ids == null)
        {
            vctxt.ids = idsNew();
            if (vctxt.ids == null)
            {
                vErrMemory(vctxt);
                return -1;
            }
        }
        /*
        * Create a schema + parser if necessary.
        */
        if (vctxt.schema == null)
        {
            Xsd10ParserCtxt pctxt;

            vctxt.xsiAssemble = 1;
            /*
            * If not schema was given then we will create a schema
            * dynamically using XSI schema locations.
            *
            * Create the schema parser context.
            */
            if ((vctxt.pctxt == null) && (createPCtxtOnVCtxt(vctxt) == -1))
            {
                return -1;
            }
            pctxt = vctxt.pctxt;
            pctxt.xsiAssemble = 1;
            /*
            * Create the schema.
            */
            vctxt.schema = newSchema(pctxt);
            if (vctxt.schema == null)
            {
                return -1;
            }
            /*
            * Create the schema construction context.
            */
            pctxt.constructor = constructionCtxtCreate(pctxt.dict);
            if (pctxt.constructor == null)
            {
                return -1;
            }
            pctxt.constructor.mainSchema = vctxt.schema;
            /*
            * Take ownership of the constructor to be able to free it.
            */
            pctxt.ownsConstructor = 1;
        }
        /*
        * Augment the IDC definitions for the main schema and all imported ones
        * NOTE: main schema if the first in the imported list
        */
        hashScan(vctxt.schema.schemasImports, Xsd10Valid::augmentImportedIdc, vctxt);

        return 0;
    }

    // xml_xsd10_post_run
    /** Port of xmlSchemaPostRun. */
    static void postRun(Xsd10ValidCtxt vctxt)
    {
        if (vctxt.xsiAssemble != 0)
        {
            if (vctxt.schema != null)
            {
                free(vctxt.schema);
                vctxt.schema = null;
            }
        }
        clearValidCtxt(vctxt);
    }

    // xml_xsd10_v_start
    /** Port of xmlSchemaVStart. */
    static int vStart(Xsd10ValidCtxt vctxt)
    {
        int ret = 0;

        if (preRun(vctxt) < 0)
        {
            return -1;
        }

        if (vctxt.doc != null)
        {
            /*
             * Tree validation.
             */
            ret = vDocWalk(vctxt);
            /* The XML Reader and SAX validation are not ported. */
        }
        else
        {
            verrorInt(vctxt, "xmlSchemaVStart", "no instance to validate");
            ret = -1;
        }

        postRun(vctxt);
        if (ret == 0)
        {
            ret = vctxt.err;
        }
        return ret;
    }

    // xml_xsd10_validate_one_element
    /**
     * Port of xmlSchemaValidateOneElement. Validate a branch of a tree, starting with the given elem.
     *
     * @param ctxt a schema validation context
     * @param elem an element node
     * @return 0 if the element and its subtree is valid, a positive error code
     * number otherwise and -1 in case of an internal or API error.
     */
    public static int validateOneElement(Xsd10ValidCtxt ctxt, XmlNode elem)
    {
        if ((ctxt == null) || (elem == null) || (nodeKind(elem) != ELEMENT))
        {
            return -1;
        }

        if (ctxt.schema == null)
        {
            return -1;
        }

        ctxt.doc = nodeDom(elem);
        ctxt.node = elem;
        ctxt.validationRoot = elem;
        return vStart(ctxt);
    }

    // xml_xsd10_validate_doc
    /**
     * Port of xmlSchemaValidateDoc. Validate a document tree in memory.
     *
     * @param ctxt a schema validation context
     * @param doc a parsed document tree
     * @return 0 if the document is schemas valid, a positive error code
     *     number otherwise and -1 in case of internal or API error.
     */
    public static int validateDoc(Xsd10ValidCtxt ctxt, XmlDocument doc)
    {
        if ((ctxt == null) || (doc == null))
        {
            return -1;
        }

        ctxt.doc = doc;
        ctxt.node = domRootElement(doc);
        if (ctxt.node == null)
        {
            customErr(ctxt, XML_XSD10_SCHEMAV_DOCUMENT_ELEMENT_MISSING,
                domDocument(doc), null,
                "The document has no document element", null, null);
            return ctxt.err;
        }
        ctxt.validationRoot = ctxt.node;
        return vStart(ctxt);
    }
}
