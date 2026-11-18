/* This file is ported from libxml2 (xmlschemas.c, and the helpers
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
    static int assembleByLocation(Xsd10ValidCtxt vctxt, Xsd10Schema schema, XmlNode node, String ns_name, String location)
    {
        int ret = 0;
        Xsd10ParserCtxt pctxt;
        Xsd10Bucket[] bucket = new Xsd10Bucket[] { null };
        String base_uri = null;

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
        location = buildAbsoluteUri(pctxt.dict, location, node, base_uri);
        /*
        * Note that we pass XML_SCHEMA_SCHEMA_IMPORT here;
        * the process will automatically change this to
        * XML_SCHEMA_SCHEMA_MAIN if it is the first schema document.
        */
        ret = addSchemaDoc(pctxt, XML_XSD10_SCHEMA_SCHEMA_IMPORT, location, null, null, 0, node, null, ns_name, bucket);
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
    static Xsd10AttrInfo getMetaAttrInfo(Xsd10ValidCtxt vctxt, int meta_type)
    {
        if (vctxt.nb_attr_infos == 0)
        {
            return null;
        }
        for (int i = 0; i < vctxt.nb_attr_infos; i++)
        {
            Xsd10AttrInfo iattr = vctxt.attr_infos[i];
            if (iattr.meta_type == meta_type)
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
            if (iattr.meta_type == XML_XSD10_SCHEMA_ATTR_INFO_META_XSI_SCHEMA_LOC)
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
                if (iattr.meta_type == XML_XSD10_SCHEMA_ATTR_INFO_META_XSI_SCHEMA_LOC)
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
        /* The SAX (ns_bindings) and reader branches are not ported. */
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
    static int validateNotation(Xsd10ValidCtxt vctxt, Xsd10Schema schema, XmlNode node, String value, Xsd10Val[] val, int val_needed)
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
        String local_name;
        String[] prefix = new String[] { null };

        local_name = treeSplitQname2(value, prefix);
        if (prefix[0] != null)
        {
            String ns_name = null;

            if (vctxt != null)
            {
                ns_name = lookupNamespace(vctxt, prefix[0]);
            }
            else if (node != null)
            {
                XmlNs ns = treeSearchNs(node, prefix[0]);
                if (ns != null)
                {
                    ns_name = nsHref(ns);
                }
            }
            else
            {
                return 1;
            }
            if (ns_name == null)
            {
                return 1;
            }
            if (getNotation(schema, local_name, ns_name) != null)
            {
                if ((val_needed != 0) && (val != null))
                {
                    val[0] = newNotationValue(strdup(local_name), strdup(ns_name));
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
                if ((val_needed != 0) && (val != null))
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
        for (i = 0; i < vctxt.node_q_names.nb_items; i += 2)
        {
            if (Objects.equals(vctxt.node_q_names.items[i], lname) &&
                Objects.equals(vctxt.node_q_names.items[i + 1], nsname))
            {
                /* Already there */
                return i;
            }
        }
        /* Add new entry. */
        i = vctxt.node_q_names.nb_items;
        if (itemListAdd(vctxt.node_q_names, lname) < 0)
        {
            vErrMemory(vctxt);
            return -1;
        }
        if (itemListAdd(vctxt.node_q_names, nsname) < 0)
        {
            vctxt.node_q_names.nb_items--;
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
        Xsd10IDC idc_def = (Xsd10IDC) payload;
        Xsd10ValidCtxt vctxt = (Xsd10ValidCtxt) data;
        Xsd10IDCAug aidc;

        aidc = new Xsd10IDCAug();
        aidc.keyref_depth = -1;
        aidc.def = idc_def;
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
        if ((vctxt.has_keyrefs == 0) && (idc_def.type == XML_XSD10_SCHEMA_TYPE_IDC_KEYREF))
        {
            vctxt.has_keyrefs = 1;
        }
    }

    // xml_xsd10_augment_imported_idc
    /** Port of xmlSchemaAugmentImportedIDC. Creates an augmented IDC definition for the imported schema. */
    static void augmentImportedIdc(Object payload, Object data, String name)
    {
        Xsd10Import imported = (Xsd10Import) payload;
        Xsd10ValidCtxt vctxt = (Xsd10ValidCtxt) data;
        if (imported.schema.idc_def != null)
        {
            hashScan(imported.schema.idc_def, Xsd10Valid::augmentIdc, vctxt);
        }
    }

    // xml_xsd10_idc_new_binding
    /** Port of xmlSchemaIDCNewBinding. Creates a new IDC binding. */
    static Xsd10PSVIIDCBinding idcNewBinding(Xsd10IDC idc_def)
    {
        Xsd10PSVIIDCBinding ret = new Xsd10PSVIIDCBinding();
        ret.definition = idc_def;
        return ret;
    }

    // xml_xsd10_idc_store_node_table_item
    /** Port of xmlSchemaIDCStoreNodeTableItem. */
    static int idcStoreNodeTableItem(Xsd10ValidCtxt vctxt, Xsd10PSVIIDCNode item)
    {
        /*
        * Add to global list.
        */
        if (vctxt.size_idc_nodes <= vctxt.nb_idc_nodes)
        {
            int new_size = growCapacity(vctxt.size_idc_nodes, PTR_SIZE, 20, XML_XSD10_MAX_ITEMS);
            if (new_size < 0)
            {
                vErrMemory(vctxt);
                return -1;
            }
            vctxt.idc_nodes = (vctxt.idc_nodes == null) ? new Xsd10PSVIIDCNode[new_size] : Arrays.copyOf(vctxt.idc_nodes, new_size);
            vctxt.size_idc_nodes = new_size;
        }

        vctxt.idc_nodes[vctxt.nb_idc_nodes++] = item;
        return 0;
    }

    // xml_xsd10_idc_store_key
    /** Port of xmlSchemaIDCStoreKey. */
    static int idcStoreKey(Xsd10ValidCtxt vctxt, Xsd10PSVIIDCKey key)
    {
        /*
        * Add to global list.
        */
        if (vctxt.size_idc_keys <= vctxt.nb_idc_keys)
        {
            int new_size = growCapacity(vctxt.size_idc_keys, PTR_SIZE, 20, XML_XSD10_MAX_ITEMS);
            if (new_size < 0)
            {
                vErrMemory(vctxt);
                return -1;
            }
            vctxt.idc_keys = (vctxt.idc_keys == null) ? new Xsd10PSVIIDCKey[new_size] : Arrays.copyOf(vctxt.idc_keys, new_size);
            vctxt.size_idc_keys = new_size;
        }

        vctxt.idc_keys[vctxt.nb_idc_keys++] = key;
        return 0;
    }

    // xml_xsd10_idc_append_node_table_item
    /** Port of xmlSchemaIDCAppendNodeTableItem. */
    static int idcAppendNodeTableItem(Xsd10PSVIIDCBinding bind, Xsd10PSVIIDCNode nt_item)
    {
        if (bind.size_nodes <= bind.nb_nodes)
        {
            int new_size = growCapacity(bind.size_nodes, PTR_SIZE, 10, XML_XSD10_MAX_ITEMS);
            if (new_size < 0)
            {
                vErrMemory(null);
                return -1;
            }
            bind.node_table = (bind.node_table == null) ? new Xsd10PSVIIDCNode[new_size] : Arrays.copyOf(bind.node_table, new_size);
            bind.size_nodes = new_size;
        }

        bind.node_table[bind.nb_nodes++] = nt_item;
        return 0;
    }

    // xml_xsd10_idc_acquire_binding
    /** Port of xmlSchemaIDCAcquireBinding. */
    static Xsd10PSVIIDCBinding idcAcquireBinding(Xsd10ValidCtxt vctxt, Xsd10IDCMatcher matcher)
    {
        Xsd10NodeInfo ielem;

        ielem = vctxt.elem_infos[matcher.depth];

        if (ielem.idc_table == null)
        {
            ielem.idc_table = idcNewBinding(matcher.aidc.def);
            if (ielem.idc_table == null)
            {
                return null;
            }
            return ielem.idc_table;
        }
        else
        {
            Xsd10PSVIIDCBinding bind = ielem.idc_table;
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
        bind.node_table = null;
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
            matcher.key_seqs = null;
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
            if (matcher.key_seqs != null)
            {
                /*
                * Don't free the array, but only the content.
                */
                for (int i = 0; i < matcher.size_key_seqs; i++)
                {
                    matcher.key_seqs[i] = null;
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
            if (vctxt.idc_matcher_cache != null)
            {
                matcher.next_cached = vctxt.idc_matcher_cache;
            }
            vctxt.idc_matcher_cache = matcher;

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
        if (vctxt.xpath_state_pool != null)
        {
            sto = vctxt.xpath_state_pool;
            vctxt.xpath_state_pool = sto.next;
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
        if (vctxt.xpath_states != null)
        {
            sto.next = vctxt.xpath_states;
        }
        vctxt.xpath_states = sto;

        /*
        * Free the old xpath validation context.
        */
        if (sto.xpath_ctxt != null)
        {
            XmlPattern.streamCtxtFree((XmlStreamCtxt) sto.xpath_ctxt);
        }

        /*
        * Create a new XPath (pattern) validation context.
        */
        sto.xpath_ctxt = XmlPattern.patternGetStreamCtxt((XmlPattern) sel.xpath_comp);
        if (sto.xpath_ctxt == null)
        {
            verrorInt(vctxt, "xmlSchemaIDCAddStateObject", "failed to create an XPath validation context");
            return -1;
        }
        sto.type = type;
        sto.depth = vctxt.depth;
        sto.matcher = matcher;
        sto.sel = sel;
        sto.nb_history = 0;

        return 0;
    }

    // xml_xsd10_x_path_evaluate
    /** Port of xmlSchemaXPathEvaluate. Evaluates all active XPath state objects. */
    static int xPathEvaluate(Xsd10ValidCtxt vctxt, int node_type)
    {
        Xsd10IDCStateObj sto;
        Xsd10IDCStateObj head = null;
        Xsd10IDCStateObj first;
        int res;
        int resolved = 0;
        int depth = vctxt.depth;

        if (vctxt.xpath_states == null)
        {
            return 0;
        }

        if (node_type == ATTRIBUTE)
        {
            depth++;
        }
        /*
        * Process all active XPath state objects.
        */
        first = vctxt.xpath_states;
        sto = first;
        while (sto != head)
        {
            if (node_type == ELEMENT)
            {
                res = XmlPattern.streamPush((XmlStreamCtxt) sto.xpath_ctxt, vctxt.inode.local_name, vctxt.inode.ns_name);
            }
            else
            {
                res = XmlPattern.streamPushAttr((XmlStreamCtxt) sto.xpath_ctxt, vctxt.inode.local_name, vctxt.inode.ns_name);
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
                if (sto.size_history <= sto.nb_history)
                {
                    int new_size = growCapacity(sto.size_history, 4, 5, XML_XSD10_MAX_ITEMS);
                    if (new_size < 0)
                    {
                        vErrMemory(vctxt);
                        return -1;
                    }
                    sto.history = (sto.history == null) ? new int[new_size] : Arrays.copyOf(sto.history, new_size);
                    sto.size_history = new_size;
                }
                sto.history[sto.nb_history++] = depth;

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
                sto = vctxt.xpath_states;
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

        if (vctxt.xpath_states == null)
        {
            return 0;
        }
        sto = vctxt.xpath_states;
        do
        {
            res = XmlPattern.streamPop((XmlStreamCtxt) sto.xpath_ctxt);
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
        int match_depth;
        Xsd10PSVIIDCKey key = null;
        Xsd10Type type = vctxt.inode.type_def;
        Xsd10Type simple_type = null;

        if (vctxt.xpath_states == null)
        {
            return 0;
        }
        sto = vctxt.xpath_states;

        /*
        * Evaluate the state objects.
        */
        while (sto != null)
        {
            res = XmlPattern.streamPop((XmlStreamCtxt) sto.xpath_ctxt);
            if (res == -1)
            {
                verrorInt(vctxt, "xmlSchemaXPathProcessHistory", "calling xmlStreamPop()");
                return -1;
            }
            deregister_check:
            {
                if (sto.nb_history == 0)
                {
                    break deregister_check;
                }

                match_depth = sto.history[sto.nb_history - 1];

                /*
                * Only matches at the current depth are of interest.
                */
                if (match_depth != depth)
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
                            simple_type = type.content_type_def;
                            if (simple_type == null)
                            {
                                verrorInt(vctxt, "xmlSchemaXPathProcessHistory",
                                    "field resolves to a CT with simple content " +
                                    "but the CT is missing the ST definition");
                                return -1;
                            }
                        }
                        else
                        {
                            simple_type = null;
                        }
                    }
                    else
                    {
                        simple_type = type;
                    }
                    if (simple_type == null)
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
                        sto.nb_history--;
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
                        sto.nb_history--;
                        break deregister_check;
                    }
                    else
                    {
                        Xsd10IDCMatcher matcher = sto.matcher;
                        Xsd10PSVIIDCKey[] key_seq;
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
                        if (pos >= matcher.size_key_seqs)
                        {
                            int new_size = growCapacity(pos, PTR_SIZE, 10, XML_XSD10_MAX_ITEMS);
                            if (new_size < 0)
                            {
                                vErrMemory(vctxt);
                                return -1;
                            }
                            /* The array needs to be NULLed (copyOf does). */
                            matcher.key_seqs = (matcher.key_seqs == null) ? new Xsd10PSVIIDCKey[new_size][] : Arrays.copyOf(matcher.key_seqs, new_size);
                            matcher.size_key_seqs = new_size;
                        }

                        /*
                        * Get/create the key-sequence.
                        */
                        key_seq = matcher.key_seqs[pos];
                        if (key_seq == null)
                        {
                            // create_sequence:
                            /*
                            * Create a key-sequence.
                            */
                            key_seq = new Xsd10PSVIIDCKey[Math.max(0, matcher.aidc.def.nb_fields)];
                            matcher.key_seqs[pos] = key_seq;
                        }
                        else if ((idx < key_seq.length) && (key_seq[idx] != null))
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
                            sto.nb_history--;
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
                            key.type = simple_type;
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
                        if (idx < key_seq.length)
                        {
                            key_seq[idx] = key;
                        }
                    }
                }
                else if (sto.type == XSD10_XPATH_STATE_OBJ_TYPE_IDC_SELECTOR)
                {
                    /* key_seq of C: &(matcher->key_seqs[pos]), valid when key_seq_set. */
                    boolean key_seq_set = false;
                    Xsd10PSVIIDCNode nt_item;
                    Xsd10IDCMatcher matcher;
                    Xsd10IDC idc;
                    Xsd10ItemList targets;
                    int pos;
                    int i;
                    int j;
                    int nb_keys;
                    boolean key_error = false;
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
                    nb_keys = idc.nb_fields;
                    pos = depth - matcher.depth;
                    selector:
                    {
                        /*
                        * Check if the matcher has any key-sequences at all, plus
                        * if it has a key-sequence for the current target node.
                        */
                        if ((matcher.key_seqs == null) || (matcher.size_key_seqs <= pos) || (pos < 0))
                        {
                            key_error = (idc.type == XML_XSD10_SCHEMA_TYPE_IDC_KEY);
                            break selector;
                        }

                        key_seq_set = true;
                        Xsd10PSVIIDCKey[] ks = matcher.key_seqs[pos];
                        if (ks == null)
                        {
                            key_error = (idc.type == XML_XSD10_SCHEMA_TYPE_IDC_KEY);
                            break selector;
                        }

                        for (i = 0; i < nb_keys; i++)
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
                                    key_error = true;
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

                        if ((idc.type != XML_XSD10_SCHEMA_TYPE_IDC_KEYREF) && (targets.nb_items != 0))
                        {
                            Xsd10PSVIIDCKey ckey;
                            Xsd10PSVIIDCKey bkey;
                            Xsd10PSVIIDCKey[] bkey_seq;
                            Xsd10IDCHashEntry e;

                            res = 0;

                            if (matcher.htab == null)
                            {
                                e = null;
                            }
                            else
                            {
                                String[] value = new String[] { null };
                                hashKeySequence(vctxt, value, ks, nb_keys);
                                e = (Xsd10IDCHashEntry) hashLookup(matcher.htab, value[0]);
                            }

                            /*
                            * Compare the key-sequences, key by key.
                            */
                            for (; e != null; e = e.next)
                            {
                                bkey_seq = ((Xsd10PSVIIDCNode) targets.items[e.index]).keys;
                                for (j = 0; j < nb_keys; j++)
                                {
                                    ckey = ks[j];
                                    bkey = bkey_seq[j];
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
                                String[] str_b = new String[] { null };
                                /*
                                * TODO: Try to report the key-sequence.
                                */
                                customErr(vctxt, XML_XSD10_SCHEMAV_CVC_IDC, null,
                                    idc,
                                    "Duplicate key-sequence %s in %s",
                                    formatIdcKeySequence(vctxt, str, ks, nb_keys),
                                    getIdcDesignation(str_b, idc));
                                break selector;
                            }
                        }
                        /*
                        * Add a node-table item to the IDC binding.
                        */
                        nt_item = new Xsd10PSVIIDCNode();

                        /*
                        * Store the node-table item in a global list.
                        */
                        if (idc.type != XML_XSD10_SCHEMA_TYPE_IDC_KEYREF)
                        {
                            if (idcStoreNodeTableItem(vctxt, nt_item) == -1)
                            {
                                matcher.key_seqs[pos] = null;
                                return -1;
                            }
                            nt_item.node_q_name_id = -1;
                        }
                        else
                        {
                            /*
                            * Save a cached QName for this node on the IDC node, to be
                            * able to report it, even if the node is not saved.
                            */
                            nt_item.node_q_name_id = vAddNodeQName(vctxt, vctxt.inode.local_name, vctxt.inode.ns_name);
                            if (nt_item.node_q_name_id == -1)
                            {
                                matcher.key_seqs[pos] = null;
                                return -1;
                            }
                        }
                        /*
                        * Init the node-table item: Save the node, position and
                        * consume the key-sequence.
                        */
                        nt_item.node = vctxt.node;
                        nt_item.node_line = vctxt.inode.node_line;
                        nt_item.keys = ks;
                        matcher.key_seqs[pos] = null;
                        if (itemListAdd(targets, nt_item) == -1)
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
                            hashKeySequence(vctxt, value, nt_item.keys, nb_keys);
                            e = new Xsd10IDCHashEntry();
                            e.index = targets.nb_items - 1;
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
                    if (key_error)
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
                    if (key_seq_set && (matcher.key_seqs[pos] != null))
                    {
                        matcher.key_seqs[pos] = null;
                    }
                } /* if selector */

                sto.nb_history--;
            }
            // deregister_check:
            /*
            * Deregister state objects if they reach the depth of creation.
            */
            if ((sto.nb_history == 0) && (sto.depth == depth))
            {
                if (vctxt.xpath_states != sto)
                {
                    verrorInt(vctxt, "xmlSchemaXPathProcessHistory",
                        "The state object to be removed is not the first " +
                        "in the list");
                }
                nextsto = sto.next;
                /*
                * Unlink from the list of active XPath state objects.
                */
                vctxt.xpath_states = sto.next;
                sto.next = vctxt.xpath_state_pool;
                /*
                * Link it to the pool of reusable state objects.
                */
                vctxt.xpath_state_pool = sto;
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
    static int idcRegisterMatchers(Xsd10ValidCtxt vctxt, Xsd10Element elem_decl)
    {
        Xsd10IDCMatcher matcher;
        Xsd10IDCMatcher last = null;
        Xsd10IDC idc;
        Xsd10IDC ref_idc;
        Xsd10IDCAug aidc;

        idc = (Xsd10IDC) elem_decl.idcs;
        if (idc == null)
        {
            return 0;
        }

        if (vctxt.inode.idc_matchers != null)
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
                * references a key/unique IDC, the keyref_depth will
                * be -1, indicating that no bubbles are needed.
                */
                ref_idc = (idc.ref.item instanceof Xsd10IDC) ? (Xsd10IDC) idc.ref.item : null;
                if (ref_idc != null)
                {
                    /*
                    * Remember that we have keyrefs on this node.
                    */
                    vctxt.inode.has_keyrefs = 1;
                    /*
                    * Lookup the referenced augmented IDC info.
                    */
                    aidc = vctxt.aidcs;
                    while (aidc != null)
                    {
                        if (aidc.def == ref_idc)
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
                    if ((aidc.keyref_depth == -1) || (vctxt.depth < aidc.keyref_depth))
                    {
                        aidc.keyref_depth = vctxt.depth;
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
            if (vctxt.idc_matcher_cache != null)
            {
                /*
                * Reuse a cached matcher.
                */
                matcher = vctxt.idc_matcher_cache;
                vctxt.idc_matcher_cache = matcher.next_cached;
                matcher.next_cached = null;
            }
            else
            {
                matcher = new Xsd10IDCMatcher();
            }
            if (last == null)
            {
                vctxt.inode.idc_matchers = matcher;
            }
            else
            {
                last.next = matcher;
            }
            last = matcher;

            matcher.type = XSD10_IDC_MATCHER;
            matcher.depth = vctxt.depth;
            matcher.aidc = aidc;
            matcher.idc_type = aidc.def.type;
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

    /** Equality of two IDC key sequences of nb_fields keys (helper for the repeated C loops). */
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
        int nb_targets;
        int nb_fields;
        int nb_dupls;
        int nb_node_table;
        Xsd10PSVIIDCKey[] keys;
        Xsd10PSVIIDCKey[] ntkeys;
        Object[] targets;
        Object[] dupls;

        Xsd10IDCMatcher matcher = ielem.idc_matchers;
        /* vctxt->create_idc_node_tables */
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
            if ((vctxt.create_idc_node_tables == 0) &&
                ((matcher.aidc.keyref_depth == -1) ||
                 (matcher.aidc.keyref_depth > vctxt.depth)))
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
                nb_dupls = bind.dupls.nb_items;
            }
            else
            {
                dupls = null;
                nb_dupls = 0;
            }
            if (bind.node_table != null)
            {
                nb_node_table = bind.nb_nodes;
            }
            else
            {
                nb_node_table = 0;
            }

            if ((nb_node_table == 0) && (nb_dupls == 0))
            {
                /*
                * Transfer all IDC target-nodes to the IDC node-table.
                */
                Object[] titems = matcher.targets.items;
                int tsize = matcher.targets.size_items;
                Xsd10PSVIIDCNode[] nt = new Xsd10PSVIIDCNode[Math.max(tsize, matcher.targets.nb_items)];
                for (int t = 0; t < matcher.targets.nb_items; t++)
                {
                    nt[t] = (Xsd10PSVIIDCNode) titems[t];
                }
                bind.node_table = nt;
                bind.size_nodes = tsize;
                bind.nb_nodes = matcher.targets.nb_items;

                matcher.targets.items = null;
                matcher.targets.size_items = 0;
                matcher.targets.nb_items = 0;
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
                nb_targets = matcher.targets.nb_items;
                targets = matcher.targets.items;
                nb_fields = matcher.aidc.def.nb_fields;
                i = 0;
                do
                {
                    next_target:
                    {
                        keys = idcNodeAt(targets, i).keys;
                        if (nb_dupls != 0)
                        {
                            /*
                            * Search in already found duplicates first.
                            */
                            j = 0;
                            do
                            {
                                if (nb_fields == 1)
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
                                    for (k = 0; k < nb_fields; k++)
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
                            } while (j < nb_dupls);
                        }
                        if (bind.nb_nodes != 0)
                        {
                            j = 0;
                            do
                            {
                                next_node_table_entry:
                                {
                                    if (nb_fields == 1)
                                    {
                                        res = areValuesEqual(keys[0].val, bind.node_table[j].keys[0].val);
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
                                        ntkeys = bind.node_table[j].keys;
                                        for (k = 0; k < nb_fields; k++)
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
                                    if (itemListAdd(bind.dupls, bind.node_table[j]) == -1)
                                    {
                                        return -1;
                                    }
                                    dupls = bind.dupls.items;
                                    /*
                                    * Remove the duplicate entry from the IDC node-table.
                                    */
                                    bind.node_table[j] = bind.node_table[bind.nb_nodes - 1];
                                    bind.nb_nodes--;

                                    break next_target;
                                }
                                // next_node_table_entry:
                                j++;
                            } while (j < bind.nb_nodes);
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
                } while (i < nb_targets);
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
        Xsd10NodeInfo par_table; /* holder of the parent IDC bindings (C: &parent->idc_table) */
        Xsd10PSVIIDCBinding par_bind = null; /* parent IDC bindings. */
        Xsd10PSVIIDCNode node;
        Xsd10PSVIIDCNode par_node = null;
        Object[] dupls;
        Xsd10PSVIIDCNode[] par_nodes;
        Xsd10IDCAug aidc;
        int i;
        int j;
        int k;
        int ret = 0;
        int nb_fields;
        int old_num;
        int old_dupls;

        bind = vctxt.inode.idc_table;
        if (bind == null)
        {
            /* Fine, no table, no bubbles. */
            return 0;
        }

        par_table = vctxt.elem_infos[vctxt.depth - 1];
        /*
        * Walk all bindings; create new or add to existing bindings.
        * Remove duplicate key-sequences.
        */
        while (bind != null)
        {
            next_binding:
            {
                if ((bind.nb_nodes == 0) && (wxsIlistIsEmpty(bind.dupls) != 0))
                {
                    break next_binding;
                }
                /*
                * Check if the key/unique IDC table needs to be bubbled.
                */
                if (vctxt.create_idc_node_tables == 0)
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
                            if ((aidc.keyref_depth == -1) || (aidc.keyref_depth >= vctxt.depth))
                            {
                                break next_binding;
                            }
                            break;
                        }
                        aidc = aidc.next;
                    } while (aidc != null);
                }

                if (par_table != null)
                {
                    par_bind = par_table.idc_table;
                }
                /*
                * Search a matching parent binding for the
                * IDC definition.
                */
                while (par_bind != null)
                {
                    if (par_bind.definition == bind.definition)
                    {
                        break;
                    }
                    par_bind = par_bind.next;
                }

                if (par_bind != null)
                {
                    /*
                    * Compare every node-table entry of the child node,
                    * i.e. the key-sequence within, ...
                    */
                    old_num = par_bind.nb_nodes; /* Skip newly added items. */

                    if (wxsIlistIsEmpty(par_bind.dupls) == 0)
                    {
                        old_dupls = par_bind.dupls.nb_items;
                        dupls = par_bind.dupls.items;
                    }
                    else
                    {
                        dupls = null;
                        old_dupls = 0;
                    }

                    par_nodes = par_bind.node_table;
                    nb_fields = bind.definition.nb_fields;

                    for (i = 0; i < bind.nb_nodes; i++)
                    {
                        node = bind.node_table[i];
                        if (node == null)
                        {
                            continue;
                        }
                        /*
                        * ...with every key-sequence of the parent node, already
                        * evaluated to be a duplicate key-sequence.
                        */
                        if (old_dupls != 0)
                        {
                            j = 0;
                            while (j < old_dupls)
                            {
                                if (nb_fields == 1)
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
                                    par_node = idcNodeAt(dupls, j);
                                    for (k = 0; k < nb_fields; k++)
                                    {
                                        ret = areValuesEqual(node.keys[k].val, par_node.keys[k].val);
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
                            if (j != old_dupls)
                            {
                                /* Duplicate found. Skip this entry. */
                                continue;
                            }
                        }
                        /*
                        * ... and with every key-sequence of the parent node.
                        */
                        if (old_num != 0)
                        {
                            j = 0;
                            while (j < old_num)
                            {
                                par_node = par_nodes[j];
                                if (nb_fields == 1)
                                {
                                    ret = areValuesEqual(node.keys[0].val, par_node.keys[0].val);
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
                                    for (k = 0; k < nb_fields; k++)
                                    {
                                        ret = areValuesEqual(node.keys[k].val, par_node.keys[k].val);
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
                            if (j != old_num)
                            {
                                /*
                                * Handle duplicates. Move the duplicate in
                                * the parent's node-table to the list of
                                * duplicates.
                                */
                                old_num--;
                                par_bind.nb_nodes--;
                                /*
                                * Move last old item to pos of duplicate.
                                */
                                par_nodes[j] = par_nodes[old_num];

                                if (par_bind.nb_nodes != old_num)
                                {
                                    /*
                                    * If new items exist, move last new item to
                                    * last of old items.
                                    */
                                    par_nodes[old_num] = par_nodes[par_bind.nb_nodes];
                                }
                                if (par_bind.dupls == null)
                                {
                                    par_bind.dupls = itemListCreate();
                                    if (par_bind.dupls == null)
                                    {
                                        return -1;
                                    }
                                }
                                itemListAdd(par_bind.dupls, par_node);
                                dupls = par_bind.dupls.items;
                            }
                            else
                            {
                                /*
                                * Add the node-table entry (node and key-sequence) of
                                * the child node to the node table of the parent node.
                                */
                                if (par_bind.nb_nodes >= par_bind.size_nodes)
                                {
                                    int new_size = growCapacity(par_bind.size_nodes, PTR_SIZE, 10, XML_XSD10_MAX_ITEMS);
                                    if (new_size < 0)
                                    {
                                        vErrMemory(vctxt);
                                        return -1;
                                    }
                                    par_bind.node_table = (par_bind.node_table == null) ? new Xsd10PSVIIDCNode[new_size] : Arrays.copyOf(par_bind.node_table, new_size);
                                    par_bind.size_nodes = new_size;
                                }
                                par_nodes = par_bind.node_table;
                                /*
                                * Append the new node-table entry to the 'new node-table
                                * entries' section.
                                */
                                par_nodes[par_bind.nb_nodes++] = node;
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
                    par_bind = idcNewBinding(bind.definition);
                    if (par_bind == null)
                    {
                        return -1;
                    }

                    if (bind.nb_nodes != 0)
                    {
                        /*
                        * Add all IDC node-table entries.
                        */
                        if (vctxt.psvi_expose_idc_node_tables == 0)
                        {
                            /*
                            * Just move the entries.
                            * NOTE: this is quite save here, since
                            * all the keyref lookups have already been
                            * performed.
                            */
                            par_bind.node_table = bind.node_table;
                            bind.node_table = null;
                            par_bind.size_nodes = bind.size_nodes;
                            bind.size_nodes = 0;
                            par_bind.nb_nodes = bind.nb_nodes;
                            bind.nb_nodes = 0;
                        }
                        else
                        {
                            /*
                            * Copy the entries.
                            */
                            par_bind.node_table = Arrays.copyOf(bind.node_table, bind.nb_nodes);
                            par_bind.size_nodes = bind.nb_nodes;
                            par_bind.nb_nodes = bind.nb_nodes;
                        }
                    }
                    if (bind.dupls != null)
                    {
                        /*
                        * Move the duplicates.
                        */
                        if (par_bind.dupls != null)
                        {
                            itemListFree(par_bind.dupls);
                        }
                        par_bind.dupls = bind.dupls;
                        bind.dupls = null;
                    }
                    if (par_table != null)
                    {
                        if (par_table.idc_table == null)
                        {
                            par_table.idc_table = par_bind;
                        }
                        else
                        {
                            par_bind.next = par_table.idc_table;
                            par_table.idc_table = par_bind;
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

        matcher = vctxt.inode.idc_matchers;
        /*
        * Find a keyref.
        */
        while (matcher != null)
        {
            if ((matcher.idc_type == XML_XSD10_SCHEMA_TYPE_IDC_KEYREF) &&
                (matcher.targets != null) &&
                (matcher.targets.nb_items != 0))
            {
                int i;
                int j;
                int k;
                int res;
                int nb_fields;
                int has_dupls;
                Xsd10PSVIIDCKey[] ref_keys;
                Xsd10PSVIIDCKey[] keys;
                Xsd10PSVIIDCNode ref_node = null;
                Xsd10Hash table = null;

                nb_fields = matcher.aidc.def.nb_fields;

                /*
                * Find the IDC node-table for the referenced IDC key/unique.
                */
                bind = vctxt.inode.idc_table;
                while (bind != null)
                {
                    if (matcher.aidc.def.ref.item == bind.definition)
                    {
                        break;
                    }
                    bind = bind.next;
                }
                has_dupls = ((bind != null) && (bind.dupls != null) && (bind.dupls.nb_items != 0)) ? 1 : 0;
                /*
                * Search for a matching key-sequences.
                */
                if (bind != null)
                {
                    table = hashCreate(bind.nb_nodes * 2);
                    for (j = 0; j < bind.nb_nodes; j++)
                    {
                        String[] value = new String[] { null };
                        Xsd10IDCHashEntry r;
                        Xsd10IDCHashEntry e;
                        keys = bind.node_table[j].keys;
                        hashKeySequence(vctxt, value, keys, nb_fields);
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
                for (i = 0; i < matcher.targets.nb_items; i++)
                {
                    res = 0;
                    ref_node = (Xsd10PSVIIDCNode) matcher.targets.items[i];
                    if (bind != null)
                    {
                        String[] value = new String[] { null };
                        Xsd10IDCHashEntry e;
                        ref_keys = ref_node.keys;
                        hashKeySequence(vctxt, value, ref_keys, nb_fields);
                        e = (Xsd10IDCHashEntry) hashLookup(table, value[0]);
                        res = 0;
                        for (; e != null; e = e.next)
                        {
                            keys = bind.node_table[e.index].keys;
                            for (k = 0; k < nb_fields; k++)
                            {
                                res = areValuesEqual(keys[k].val, ref_keys[k].val);
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
                        if ((res == 0) && (has_dupls != 0))
                        {
                            /*
                            * Search in duplicates
                            */
                            for (j = 0; j < bind.dupls.nb_items; j++)
                            {
                                keys = ((Xsd10PSVIIDCNode) bind.dupls.items[j]).keys;
                                for (k = 0; k < nb_fields; k++)
                                {
                                    res = areValuesEqual(keys[k].val, ref_keys[k].val);
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
                                    String[] str_b = new String[] { null };
                                    keyrefErr(vctxt,
                                        XML_XSD10_SCHEMAV_CVC_IDC, ref_node,
                                        matcher.aidc.def,
                                        "More than one match found for " +
                                        "key-sequence %s of keyref '%s'",
                                        formatIdcKeySequence(vctxt, str, ref_node.keys, nb_fields),
                                        getComponentQName(str_b, matcher.aidc.def));
                                    break;
                                }
                            }
                        }
                    }

                    if (res == 0)
                    {
                        String[] str = new String[] { null };
                        String[] str_b = new String[] { null };
                        keyrefErr(vctxt,
                            XML_XSD10_SCHEMAV_CVC_IDC, ref_node,
                            matcher.aidc.def,
                            "No match found for key-sequence %s of keyref '%s'",
                            formatIdcKeySequence(vctxt, str, ref_node.keys, nb_fields),
                            getComponentQName(str_b, matcher.aidc.def));
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
        if (vctxt.size_attr_infos <= vctxt.nb_attr_infos)
        {
            int old_size = vctxt.size_attr_infos;
            int new_size = growCapacity(old_size, PTR_SIZE, 5, XML_XSD10_MAX_ITEMS);
            if (new_size < 0)
            {
                vErrMemory(vctxt);
                return null;
            }
            vctxt.attr_infos = (vctxt.attr_infos == null) ? new Xsd10AttrInfo[new_size] : Arrays.copyOf(vctxt.attr_infos, new_size);
            vctxt.size_attr_infos = new_size;
        }

        iattr = vctxt.attr_infos[vctxt.nb_attr_infos];
        if (iattr != null)
        {
            if (iattr.local_name != null)
            {
                verrorInt(vctxt, "xmlSchemaGetFreshAttrInfo", "attr info not cleared");
                return null;
            }
            iattr.node_type = ATTRIBUTE;
            vctxt.nb_attr_infos++;
            return iattr;
        }

        /*
        * Create an attribute info.
        */
        iattr = new Xsd10AttrInfo();
        iattr.node_type = ATTRIBUTE;
        vctxt.attr_infos[vctxt.nb_attr_infos++] = iattr;

        return iattr;
    }

    // xml_xsd10_validator_push_attribute
    /** Port of xmlSchemaValidatorPushAttribute. */
    static int validatorPushAttribute(Xsd10ValidCtxt vctxt, XmlNode attr_node, int node_line, String local_name, String ns_name, int owned_names, String value, int owned_value)
    {
        Xsd10AttrInfo attr;

        attr = getFreshAttrInfo(vctxt);
        if (attr == null)
        {
            verrorInt(vctxt, "xmlSchemaPushAttribute", "calling xmlSchemaGetFreshAttrInfo()");
            return -1;
        }
        attr.node = attr_node;
        attr.node_line = node_line;
        attr.state = XML_XSD10_SCHEMAS_ATTR_UNKNOWN;
        attr.local_name = local_name;
        attr.ns_name = ns_name;
        if (owned_names != 0)
        {
            attr.flags |= XML_XSD10_SCHEMA_NODE_INFO_FLAG_OWNED_NAMES;
        }
        /*
        * Evaluate if it's an XSI attribute.
        */
        if (ns_name != null)
        {
            if (strEqual(local_name, "nil") != 0)
            {
                if (strEqual(attr.ns_name, XML_XSD10_INSTANCE_NS) != 0)
                {
                    attr.meta_type = XML_XSD10_SCHEMA_ATTR_INFO_META_XSI_NIL;
                }
            }
            else if (strEqual(local_name, "type") != 0)
            {
                if (strEqual(attr.ns_name, XML_XSD10_INSTANCE_NS) != 0)
                {
                    attr.meta_type = XML_XSD10_SCHEMA_ATTR_INFO_META_XSI_TYPE;
                }
            }
            else if (strEqual(local_name, "schemaLocation") != 0)
            {
                if (strEqual(attr.ns_name, XML_XSD10_INSTANCE_NS) != 0)
                {
                    attr.meta_type = XML_XSD10_SCHEMA_ATTR_INFO_META_XSI_SCHEMA_LOC;
                }
            }
            else if (strEqual(local_name, "noNamespaceSchemaLocation") != 0)
            {
                if (strEqual(attr.ns_name, XML_XSD10_INSTANCE_NS) != 0)
                {
                    attr.meta_type = XML_XSD10_SCHEMA_ATTR_INFO_META_XSI_NO_NS_SCHEMA_LOC;
                }
            }
            else if (strEqual(attr.ns_name, XML_XSD10_NAMESPACE_NS) != 0)
            {
                attr.meta_type = XML_XSD10_SCHEMA_ATTR_INFO_META_XMLNS;
            }
        }
        attr.value = value;
        if (owned_value != 0)
        {
            attr.flags |= XML_XSD10_SCHEMA_NODE_INFO_FLAG_OWNED_VALUES;
        }
        if (attr.meta_type != 0)
        {
            attr.state = XML_XSD10_SCHEMAS_ATTR_META;
        }
        return 0;
    }

    /** C memset(info, 0, sizeof(XmlXsd10NodeInfo)) of the shared node info fields. */
    private static void zeroNodeInfo(Xsd10NodeInfo info)
    {
        info.node_type = 0;
        info.node = null;
        info.node_line = 0;
        info.local_name = null;
        info.ns_name = null;
        info.value = null;
        info.val = null;
        info.type_def = null;
        info.flags = 0;
        info.val_needed = 0;
        info.norm_val = 0;
        setDecl(info, null);
        info.depth = 0;
        info.idc_table = null;
        info.idc_matchers = null;
        info.regex_ctxt = null;
        info.ns_bindings = null;
        info.nb_ns_bindings = 0;
        info.size_ns_bindings = 0;
        info.has_keyrefs = 0;
        info.applied_x_path = 0;
    }

    /** C memset(attr, 0, sizeof(XmlXsd10AttrInfo)). */
    private static void zeroAttrInfo(Xsd10AttrInfo attr)
    {
        zeroNodeInfo(attr);
        attr.decl = null;
        attr.use = null;
        attr.state = 0;
        attr.meta_type = 0;
        attr.vc_value = null;
        attr.parent = null;
    }

    // xml_xsd10_clear_elem_info
    /** Port of xmlSchemaClearElemInfo. */
    static void clearElemInfo(Xsd10ValidCtxt vctxt, Xsd10NodeInfo ielem)
    {
        ielem.has_keyrefs = 0;
        ielem.applied_x_path = 0;
        ielem.local_name = null;
        ielem.ns_name = null;
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
        if (ielem.idc_matchers != null)
        {
            /*
            * REVISIT OPTIMIZE TODO: Use a pool of IDC matchers.
            *   Does it work?
            */
            idcReleaseMatcherList(vctxt, ielem.idc_matchers);
            ielem.idc_matchers = null;
        }
        if (ielem.idc_table != null)
        {
            /*
            * OPTIMIZE TODO: Use a pool of IDC tables??.
            */
            idcFreeIdcTable(ielem.idc_table);
            ielem.idc_table = null;
        }
        if (ielem.regex_ctxt != null)
        {
            XmlRegexp.regExecFree(ielem.regex_ctxt);
            ielem.regex_ctxt = null;
        }
        if (ielem.ns_bindings != null)
        {
            ielem.ns_bindings = null;
            ielem.nb_ns_bindings = 0;
            ielem.size_ns_bindings = 0;
        }
    }

    // xml_xsd10_get_fresh_elem_info
    /** Port of xmlSchemaGetFreshElemInfo. Creates/reuses and initializes the element info item for the current tree depth. */
    static Xsd10NodeInfo getFreshElemInfo(Xsd10ValidCtxt vctxt)
    {
        Xsd10NodeInfo info = null;

        if (vctxt.depth > vctxt.size_elem_infos)
        {
            verrorInt(vctxt, "xmlSchemaGetFreshElemInfo", "inconsistent depth encountered");
            return null;
        }
        if (vctxt.size_elem_infos <= vctxt.depth)
        {
            int old_size = vctxt.size_elem_infos;
            int new_size = growCapacity(old_size, PTR_SIZE, 10, XML_XSD10_MAX_ITEMS);
            if (new_size < 0)
            {
                vErrMemory(vctxt);
                return null;
            }
            /*
            * We need the new memory to be NULLed.
            */
            vctxt.elem_infos = (vctxt.elem_infos == null) ? new Xsd10NodeInfo[new_size] : Arrays.copyOf(vctxt.elem_infos, new_size);
            vctxt.size_elem_infos = new_size;
        }
        else
        {
            info = vctxt.elem_infos[vctxt.depth];
        }

        if (info == null)
        {
            info = new Xsd10NodeInfo();
            vctxt.elem_infos[vctxt.depth] = info;
        }
        else
        {
            if (info.local_name != null)
            {
                verrorInt(vctxt, "xmlSchemaGetFreshElemInfo", "elem info has not been cleared");
                return null;
            }
        }
        zeroNodeInfo(info);
        info.node_type = ELEMENT;
        info.depth = vctxt.depth;

        return info;
    }

    // XSD10_ACTIVATE_ELEM
    private static void activateElem(Xsd10ValidCtxt vctxt)
    {
        vctxt.inode = vctxt.elem_infos[vctxt.depth];
    }

    // XSD10_ACTIVATE_PARENT_ELEM
    private static void activateParentElem(Xsd10ValidCtxt vctxt)
    {
        vctxt.inode = vctxt.elem_infos[vctxt.depth - 1];
    }

    // xml_xsd10_validate_facets
    /** Port of xmlSchemaValidateFacets. */
    static int validateFacets(Xsd10AbstractCtxt actxt, XmlNode node, Xsd10Type type, int val_type, String value, Xsd10Val val, long length, int fire_errors)
    {
        int ret;
        int error = 0;
        int found;

        Xsd10Type tmp_type;
        Xsd10FacetLink facet_link;
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
        * NOTE: Do not jump away, if the facet_set of the given type is
        * empty: until now, "pattern" and "enumeration" facets of the
        * *base types* need to be checked as well.
        */
        boolean do_atomic = true;
        boolean do_list = true;
        if (type.facet_set == null)
        {
            do_atomic = false;
            do_list = false;
        }
        else if (wxsIsAtomic(type) == 0)
        {
            do_atomic = false;
            if (wxsIsList(type) == 0)
            {
                do_list = false;
            }
        }

        if (do_atomic)
        {
            /*
            * Whitespace handling is only of importance for string-based
            * types.
            */
            tmp_type = getPrimitiveType(type);
            if ((tmp_type.built_in_type == XML_XSD10_SCHEMAS_STRING) || (wxsIsAnySimpleType(tmp_type) != 0))
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
                val_type = getValType(val);
            }

            ret = 0;
            for (facet_link = type.facet_set; facet_link != null; facet_link = facet_link.next)
            {
                /*
                * Skip the pattern "whiteSpace": it is used to
                * format the character content beforehand.
                */
                switch (facet_link.facet.type)
                {
                    case XML_XSD10_SCHEMA_FACET_WHITESPACE:
                    case XML_XSD10_SCHEMA_FACET_PATTERN:
                    case XML_XSD10_SCHEMA_FACET_ENUMERATION:
                        continue;
                    case XML_XSD10_SCHEMA_FACET_LENGTH:
                    case XML_XSD10_SCHEMA_FACET_MINLENGTH:
                    case XML_XSD10_SCHEMA_FACET_MAXLENGTH:
                        ret = validateLengthFacetWhtsp(facet_link.facet, val_type, value, val, len, ws);
                        break;
                    default:
                        ret = validateFacetWhtsp(facet_link.facet, ws, val_type, value, val, ws);
                        break;
                }
                if (ret < 0)
                {
                    aerrorInt(actxt, "xmlSchemaValidateFacets", "validating against a atomic type facet");
                    return -1;
                }
                else if (ret > 0)
                {
                    if (fire_errors != 0)
                    {
                        facetErr(actxt, ret, node, value, len[0], type, facet_link.facet, null, null, null);
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
        if (do_list && (wxsIsList(type) != 0))
        {
            /*
            * "length", "minLength" and "maxLength" of list types.
            */
            ret = 0;
            for (facet_link = type.facet_set; facet_link != null; facet_link = facet_link.next)
            {
                switch (facet_link.facet.type)
                {
                    case XML_XSD10_SCHEMA_FACET_LENGTH:
                    case XML_XSD10_SCHEMA_FACET_MINLENGTH:
                    case XML_XSD10_SCHEMA_FACET_MAXLENGTH:
                        ret = validateListSimpleTypeFacet(facet_link.facet, value, length, null);
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
                    if (fire_errors != 0)
                    {
                        facetErr(actxt, ret, node, value, length, type, facet_link.facet, null, null, null);
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
        tmp_type = type;
        do
        {
            for (facet = tmp_type.facets; facet != null; facet = facet.next)
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
            tmp_type = tmp_type.base_type;
        } while ((tmp_type != null) && (tmp_type.type != XML_XSD10_SCHEMA_TYPE_BASIC));
        if ((found != 0) && (ret == 0))
        {
            ret = XML_XSD10_SCHEMAV_CVC_ENUMERATION_VALID;
            if (fire_errors != 0)
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
        tmp_type = type;
        facet = null;
        do
        {
            found = 0;
            for (facet_link = tmp_type.facet_set; facet_link != null; facet_link = facet_link.next)
            {
                if (facet_link.facet.type != XML_XSD10_SCHEMA_FACET_PATTERN)
                {
                    continue;
                }
                found = 1;
                /*
                * NOTE that for patterns, @value needs to be the
                * normalized value.
                */
                ret = XmlRegexp.regexpExec((XmlRegexp) facet_link.facet.regexp, value);
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
                    facet = facet_link.facet;
                }
            }
            if ((found != 0) && (ret != 1))
            {
                ret = XML_XSD10_SCHEMAV_CVC_PATTERN_VALID;
                if (fire_errors != 0)
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
            tmp_type = tmp_type.base_type;
        } while ((tmp_type != null) && (tmp_type.type != XML_XSD10_SCHEMA_TYPE_BASIC));

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
    static int validateQName(Xsd10ValidCtxt vctxt, String value, Xsd10Val[] val, int val_needed)
    {
        int ret;
        String stripped;
        String ns_name;
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
        ns_name = lookupNamespace(vctxt, prefix[0]);

        if (prefix[0] != null)
        {
            /*
            * A namespace must be found if the prefix is
            * NOT NULL.
            */
            if (ns_name == null)
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
        if ((val_needed != 0) && (val != null))
        {
            if (ns_name != null)
            {
                val[0] = newQnameValue(strdup(ns_name), local);
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
        for (i = 0; i < pctxt.constructor.buckets.nb_items; i++)
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
    static int vCheckCvcSimpleType(Xsd10AbstractCtxt actxt, XmlNode node, Xsd10Type type, String value, Xsd10Val[] ret_val, int fire_errors, int normalize, int is_normalized)
    {
        int ret = 0;
        int val_needed = (ret_val != null) ? 1 : 0;
        Xsd10Val[] val = new Xsd10Val[] { null };
        /* xmlSchemaWhitespaceValueType ws; */
        String norm_value = null;

        if ((ret_val != null) && (ret_val[0] != null))
        {
            freeValue(ret_val[0]);
            ret_val[0] = null;
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
        if ((val_needed == 0) && ((type.flags & XML_XSD10_SCHEMAS_TYPE_FACETSNEEDVALUE) != 0))
        {
            val_needed = 1;
        }
        if (value == null)
        {
            value = "";
        }
        if ((wxsIsAnySimpleType(type) != 0) || (wxsIsAtomic(type) != 0))
        {
            Xsd10Type bi_type; /* The built-in type. */
            /*
            * SPEC (1.2.1) "if {variety} is `atomic` then the string must `match`
            * a literal in the `lexical space` of {base type definition}"
            */
            /*
            * Whitespace-normalize.
            */
            // XSD10_NORMALIZE(type)
            if ((is_normalized == 0) && ((normalize != 0) || ((type.flags & XML_XSD10_SCHEMAS_TYPE_NORMVALUENEEDED) != 0)))
            {
                norm_value = normalizeValue(type, value);
                if (norm_value != null)
                {
                    value = norm_value;
                }
                is_normalized = 1;
            }
            if (type.type != XML_XSD10_SCHEMA_TYPE_BASIC)
            {
                /*
                * Get the built-in type.
                */
                bi_type = type.base_type;
                while ((bi_type != null) && (bi_type.type != XML_XSD10_SCHEMA_TYPE_BASIC))
                {
                    bi_type = bi_type.base_type;
                }

                if (bi_type == null)
                {
                    aerrorInt(actxt, "xmlSchemaVCheckCVCSimpleType", "could not get the built-in type");
                    return -1;
                }
            }
            else
            {
                bi_type = type;
            }
            /*
            * NOTATIONs need to be processed here, since they need
            * to lookup in the hashtable of NOTATION declarations of the schema.
            */
            if (actxt.type == XML_XSD10_SCHEMA_CTXT_VALIDATOR)
            {
                switch (bi_type.built_in_type)
                {
                    case XML_XSD10_SCHEMAS_NOTATION:
                        ret = validateNotation((Xsd10ValidCtxt) actxt, ((Xsd10ValidCtxt) actxt).schema, null, value, val, val_needed);
                        break;
                    case XML_XSD10_SCHEMAS_QNAME:
                        ret = validateQName((Xsd10ValidCtxt) actxt, value, val, val_needed);
                        break;
                    default:
                        /* ws = xmlSchemaGetWhiteSpaceFacetValue(type); */
                        if (val_needed != 0)
                        {
                            ret = valPredefTypeNodeNoNormIds(bi_type, value, val, node, ((Xsd10ValidCtxt) actxt).ids);
                        }
                        else
                        {
                            ret = valPredefTypeNodeNoNormIds(bi_type, value, null, node, ((Xsd10ValidCtxt) actxt).ids);
                        }
                        break;
                }
            }
            else if (actxt.type == XML_XSD10_SCHEMA_CTXT_PARSER)
            {
                switch (bi_type.built_in_type)
                {
                    case XML_XSD10_SCHEMAS_NOTATION:
                        ret = validateNotation(null, ((Xsd10ParserCtxt) actxt).schema, node, value, val, val_needed);
                        break;
                    default:
                        /* ws = xmlSchemaGetWhiteSpaceFacetValue(type); */
                        if (val_needed != 0)
                        {
                            ret = valPredefTypeNodeNoNormIds(bi_type, value, val, node,
                                validSchemaDocIds((Xsd10ParserCtxt) actxt, node));
                        }
                        else
                        {
                            ret = valPredefTypeNodeNoNormIds(bi_type, value, null, node,
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
                ret = validateFacets(actxt, node, type, bi_type.built_in_type, value, val[0], 0, fire_errors);
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
            else if ((fire_errors != 0) && (ret > 0))
            {
                simpleTypeErr(actxt, ret, node, value, type, 1);
            }
        }
        else if (wxsIsList(type) != 0)
        {
            Xsd10Type item_type;
            int cur;
            int end;
            String tmp_value = null;
            long len = 0;
            Xsd10Val prev_val = null;
            Xsd10Val[] cur_val = new Xsd10Val[] { null };
            /* 1.2.2 if {variety} is `list` then the string must be a sequence
            * of white space separated tokens, each of which `match`es a literal
            * in the `lexical space` of {item type definition}
            */
            /*
            * Note that XML_SCHEMAS_TYPE_NORMVALUENEEDED will be set if
            * the list type has an enum or pattern facet.
            */
            // XSD10_NORMALIZE(type)
            if ((is_normalized == 0) && ((normalize != 0) || ((type.flags & XML_XSD10_SCHEMAS_TYPE_NORMVALUENEEDED) != 0)))
            {
                norm_value = normalizeValue(type, value);
                if (norm_value != null)
                {
                    value = norm_value;
                }
                is_normalized = 1;
            }
            /*
            * VAL TODO: Optimize validation of empty values.
            * VAL TODO: We do not have computed values for lists.
            */
            item_type = wxsListItemtype(type);
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
                tmp_value = value.substring(cur, end);
                len++;

                if (val_needed != 0)
                {
                    ret = vCheckCvcSimpleType(actxt, node, item_type, tmp_value, cur_val, fire_errors, 0, 1);
                }
                else
                {
                    ret = vCheckCvcSimpleType(actxt, node, item_type, tmp_value, null, fire_errors, 0, 1);
                }
                tmp_value = null;
                if (cur_val[0] != null)
                {
                    /*
                    * Add to list of computed values.
                    */
                    if (val[0] == null)
                    {
                        val[0] = cur_val[0];
                    }
                    else
                    {
                        valueAppend(prev_val, cur_val[0]);
                    }
                    prev_val = cur_val[0];
                    cur_val[0] = null;
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
                ret = validateFacets(actxt, node, type, XML_XSD10_SCHEMAS_UNKNOWN, value, val[0], len, fire_errors);
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
            if ((fire_errors != 0) && (ret > 0))
            {
                /*
                * Report the normalized value.
                */
                normalize = 1;
                // XSD10_NORMALIZE(type)
                if ((is_normalized == 0) && ((normalize != 0) || ((type.flags & XML_XSD10_SCHEMAS_TYPE_NORMVALUENEEDED) != 0)))
                {
                    norm_value = normalizeValue(type, value);
                    if (norm_value != null)
                    {
                        value = norm_value;
                    }
                    is_normalized = 1;
                }
                simpleTypeErr(actxt, ret, node, value, type, 1);
            }
        }
        else if (wxsIsUnion(type) != 0)
        {
            Xsd10TypeLink member_link;
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
            member_link = getUnionSimpleTypeMemberTypes(type);
            if (member_link == null)
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
            while (member_link != null)
            {
                if (val_needed != 0)
                {
                    ret = vCheckCvcSimpleType(actxt, node, (Xsd10Type) member_link.type, value, val, 0, 1, 0);
                }
                else
                {
                    ret = vCheckCvcSimpleType(actxt, node, (Xsd10Type) member_link.type, value, null, 0, 1, 0);
                }
                if (ret <= 0)
                {
                    break;
                }
                member_link = member_link.next;
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
                // XSD10_NORMALIZE(member_link->type)
                if ((is_normalized == 0) && ((normalize != 0) || ((type.flags & XML_XSD10_SCHEMAS_TYPE_NORMVALUENEEDED) != 0)))
                {
                    norm_value = normalizeValue((Xsd10Type) member_link.type, value);
                    if (norm_value != null)
                    {
                        value = norm_value;
                    }
                    is_normalized = 1;
                }
                ret = validateFacets(actxt, node, type, XML_XSD10_SCHEMAS_UNKNOWN, value, val[0], 0, fire_errors);
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
            if ((fire_errors != 0) && (ret > 0))
            {
                simpleTypeErr(actxt, ret, node, value, type, 1);
            }
        }

        if (ret == 0)
        {
            if (ret_val != null)
            {
                ret_val[0] = val[0];
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
    static int vExpandQName(Xsd10ValidCtxt vctxt, String value, String[] ns_name, String[] local_name)
    {
        int ret = 0;

        if ((ns_name == null) || (local_name == null))
        {
            return -1;
        }
        ns_name[0] = null;
        local_name[0] = null;

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
            local_name[0] = dictLookup(vctxt.dict, value, -1);
        }
        else
        {
            local_name[0] = dictLookup(vctxt.dict, local, -1);
        }

        ns_name[0] = lookupNamespace(vctxt, prefix[0]);

        if (prefix[0] != null)
        {
            /*
            * A namespace must be found if the prefix is NOT NULL.
            */
            if (ns_name[0] == null)
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
    static int processXsiType(Xsd10ValidCtxt vctxt, Xsd10AttrInfo iattr, Xsd10Type[] local_type, Xsd10Element elem_decl)
    {
        int ret = 0;
        /*
        * cvc-elt (3.3.4) : (4)
        * AND
        * Schema-Validity Assessment (Element) (cvc-assess-elt)
        *   (1.2.1.2.1) - (1.2.1.2.4)
        * Handle 'xsi:type'.
        */
        if (local_type == null)
        {
            return -1;
        }
        local_type[0] = null;
        if (iattr == null)
        {
            return 0;
        }
        exit:
        {
            String[] ns_name = new String[] { null };
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
            ret = vExpandQName(vctxt, iattr.value, ns_name, local);
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
            local_type[0] = getType(vctxt.schema, local[0], ns_name[0]);
            if (local_type[0] == null)
            {
                String[] str = new String[] { null };

                customErr(vctxt, XML_XSD10_SCHEMAV_CVC_ELT_4_2, null,
                    getBuiltInType(XML_XSD10_SCHEMAS_QNAME),
                    "The QName value '%s' of the xsi:type attribute does not " +
                    "resolve to a type definition",
                    formatQName(str, ns_name[0], local[0]), null);
                ret = vctxt.err;
                break exit;
            }
            if (elem_decl != null)
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
                if (((elem_decl.flags & XML_XSD10_SCHEMAS_ELEM_BLOCK_EXTENSION) != 0) ||
                    ((elem_decl.subtypes.flags & XML_XSD10_SCHEMAS_TYPE_BLOCK_EXTENSION) != 0))
                {
                    set |= XSD10_SUBSET_EXTENSION;
                }

                if (((elem_decl.flags & XML_XSD10_SCHEMAS_ELEM_BLOCK_RESTRICTION) != 0) ||
                    ((elem_decl.subtypes.flags & XML_XSD10_SCHEMAS_TYPE_BLOCK_RESTRICTION) != 0))
                {
                    set |= XSD10_SUBSET_RESTRICTION;
                }

                /*
                * REMOVED and CHANGED since this produced a parser context
                * which adds to the string dict of the schema. So this would
                * change the schema and we don't want this. We don't need
                * the parser context anymore.
                */

                if (checkCosDerivedOk(vctxt, local_type[0], elem_decl.subtypes, set) != 0)
                {
                    String[] str = new String[] { null };

                    customErr(vctxt, XML_XSD10_SCHEMAV_CVC_ELT_4_3, null, null,
                        "The type definition '%s', specified by xsi:type, is " +
                        "blocked or not validly derived from the type definition " +
                        "of the element declaration",
                        formatQName(str, local_type[0].target_namespace, local_type[0].name),
                        null);
                    ret = vctxt.err;
                    local_type[0] = null;
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
        Xsd10Element elem_decl = vctxt.inode.decl;
        Xsd10Type actual_type;

        /*
        * cvc-elt (3.3.4) : 1
        */
        if (elem_decl == null)
        {
            verror(vctxt, XML_XSD10_SCHEMAV_CVC_ELT_1, null, "No matching declaration available");
            return vctxt.err;
        }
        actual_type = wxsElemTypedef(elem_decl);
        /*
        * cvc-elt (3.3.4) : 2
        */
        if ((elem_decl.flags & XML_XSD10_SCHEMAS_ELEM_ABSTRACT) != 0)
        {
            verror(vctxt, XML_XSD10_SCHEMAV_CVC_ELT_2, null, "The element declaration is abstract");
            return vctxt.err;
        }
        if (actual_type == null)
        {
            verror(vctxt, XML_XSD10_SCHEMAV_CVC_TYPE_1, null, "The type definition is absent");
            return XML_XSD10_SCHEMAV_CVC_TYPE_1;
        }
        if (vctxt.nb_attr_infos != 0)
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
                    if ((elem_decl.flags & XML_XSD10_SCHEMAS_ELEM_NILLABLE) == 0)
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
                            if (((elem_decl.flags & XML_XSD10_SCHEMAS_ELEM_FIXED) != 0) && (elem_decl.value != null))
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
                Xsd10Type[] local_type = new Xsd10Type[] { null };

                ret = processXsiType(vctxt, iattr, local_type, elem_decl);
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
                if (local_type[0] != null)
                {
                    vctxt.inode.flags |= XML_XSD10_SCHEMA_ELEM_INFO_LOCAL_TYPE;
                    actual_type = local_type[0];
                }
            }
        }
        /*
        * IDC: Register identity-constraint XPath matchers.
        */
        if ((elem_decl.idcs != null) && (idcRegisterMatchers(vctxt, elem_decl) == -1))
        {
            return -1;
        }
        /*
        * No actual type definition.
        */
        if (actual_type == null)
        {
            verror(vctxt, XML_XSD10_SCHEMAV_CVC_TYPE_1, null, "The type definition is absent");
            return XML_XSD10_SCHEMAV_CVC_TYPE_1;
        }
        /*
        * Remember the actual type definition.
        */
        vctxt.inode.type_def = actual_type;

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
        if (vctxt.nb_attr_infos == 0)
        {
            return 0;
        }
        for (i = 0; i < vctxt.nb_attr_infos; i++)
        {
            iattr = vctxt.attr_infos[i];
            if (iattr.meta_type == 0)
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

        if (vctxt.nb_attr_infos == 0)
        {
            return;
        }
        for (i = 0; i < vctxt.nb_attr_infos; i++)
        {
            attr = vctxt.attr_infos[i];
            if (attr.val != null)
            {
                freeValue(attr.val);
                attr.val = null;
            }
            zeroAttrInfo(attr);
        }
        vctxt.nb_attr_infos = 0;
    }

    /*
    * C stores either an XmlXsd10Element or an XmlXsd10Wildcard (content model transition data)
    * in inode->decl. Xsd10NodeInfo.decl is typed Xsd10Element, so a wildcard stored there is
    * kept in this side table (identity keyed, weak, so dropped node infos are collected).
    */
    private static final java.util.Map<Xsd10NodeInfo, Xsd10Wildcard> wild_decls =
        java.util.Collections.synchronizedMap(new java.util.WeakHashMap<>());

    /** C inode->decl as the generic item it really is (Element, Wildcard or null). */
    private static Xsd10BasicItem getDecl(Xsd10NodeInfo inode)
    {
        if (inode.decl != null)
        {
            return inode.decl;
        }
        return wild_decls.get(inode);
    }

    /** C inode->decl = item. */
    private static void setDecl(Xsd10NodeInfo inode, Object item)
    {
        if (item instanceof Xsd10Wildcard)
        {
            inode.decl = null;
            wild_decls.put(inode, (Xsd10Wildcard) item);
        }
        else
        {
            inode.decl = (item instanceof Xsd10Element) ? (Xsd10Element) item : null;
            if (!wild_decls.isEmpty())
            {
                wild_decls.remove(inode);
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
        Xsd10Type type = vctxt.inode.type_def;
        Xsd10ItemList attr_use_list;
        Xsd10AttributeUse attr_use = null;
        Xsd10Attribute attr_decl = null;
        Xsd10AttrInfo iattr;
        Xsd10AttrInfo tmpiattr;
        int i;
        int j;
        int found;
        int nb_attrs;
        int nb_uses;
        int xpath_res = 0;
        int res;
        int wild_i_ds = 0;
        int fixed;
        XmlNode def_attr_owner_elem = null;

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
        attr_use_list = (Xsd10ItemList) type.attr_uses;
        /*
        * @nb_attrs is the number of attributes present in the instance.
        */
        nb_attrs = vctxt.nb_attr_infos;
        if (attr_use_list != null)
        {
            nb_uses = attr_use_list.nb_items;
        }
        else
        {
            nb_uses = 0;
        }
        for (i = 0; i < nb_uses; i++)
        {
            found = 0;
            attr_use = (Xsd10AttributeUse) attr_use_list.items[i];
            attr_decl = wxsAttruseDecl(attr_use);
            for (j = 0; j < nb_attrs; j++)
            {
                iattr = vctxt.attr_infos[j];
                /*
                * SPEC (cvc-complex-type) (3)
                * Skip meta attributes.
                */
                if (iattr.meta_type != 0)
                {
                    continue;
                }
                if (at(iattr.local_name, 0) != at(attr_decl.name, 0))
                {
                    continue;
                }
                if (strEqual(iattr.local_name, attr_decl.name) == 0)
                {
                    continue;
                }
                if (strEqual(iattr.ns_name, attr_decl.target_namespace) == 0)
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
                iattr.use = attr_use;
                /*
                * Context-determined declaration.
                */
                iattr.decl = attr_decl;
                iattr.type_def = attr_decl.subtypes;
                break;
            }

            if (found != 0)
            {
                continue;
            }

            if (attr_use.occurs == XML_XSD10_SCHEMAS_ATTR_USE_REQUIRED)
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
                tmpiattr.use = attr_use;
                tmpiattr.decl = attr_decl;
            }
            else if ((attr_use.occurs == XML_XSD10_SCHEMAS_ATTR_USE_OPTIONAL) &&
                ((attr_use.def_value != null) || (attr_decl.def_value != null)))
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
                tmpiattr.use = attr_use;
                tmpiattr.decl = attr_decl;
                tmpiattr.type_def = attr_decl.subtypes;
                tmpiattr.local_name = attr_decl.name;
                tmpiattr.ns_name = attr_decl.target_namespace;
            }
        }

        if (vctxt.nb_attr_infos == 0)
        {
            return 0;
        }
        /*
        * Validate against the wildcard.
        */
        if (type.attribute_wildcard != null)
        {
            /*
            * SPEC (cvc-complex-type)
            * (3.2.1) "There must be an {attribute wildcard}."
            */
            for (i = 0; i < nb_attrs; i++)
            {
                iattr = vctxt.attr_infos[i];
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
                if (checkCvcWildcardNamespace(type.attribute_wildcard, iattr.ns_name) == 0)
                {
                    /*
                    * Handle process_contents.
                    *
                    * SPEC (cvc-wildcard):
                    * process_contents | context-determined declaration:
                    * "strict"          "mustFind"
                    * "lax"             "none"
                    * "skip"            "skip"
                    */
                    if (type.attribute_wildcard.process_contents == XML_XSD10_SCHEMAS_ANY_SKIP)
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
                    iattr.decl = getAttributeDecl(vctxt.schema, iattr.local_name, iattr.ns_name);
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
                        iattr.type_def = wxsAttrTypedef(iattr.decl);
                        if (isDerivedFromBuiltInType(iattr.type_def, XML_XSD10_SCHEMAS_ID) != 0)
                        {
                            /*
                            * SPEC (5.1) "There must be no more than one
                            * item in `wild IDs`."
                            */
                            if (wild_i_ds != 0)
                            {
                                /* VAL TODO */
                                iattr.state = XML_XSD10_SCHEMAS_ATTR_ERR_WILD_DUPLICATE_ID;
                                continue;
                            }
                            wild_i_ds++;
                            /*
                            * SPEC (cvc-complex-type)
                            * (5.2) "If `wild IDs` is non-empty, there must not
                            * be any attribute uses among the {attribute uses}
                            * whose {attribute declaration}'s {type definition}
                            * is or is derived from ID."
                            */
                            if (attr_use_list != null)
                            {
                                for (j = 0; j < attr_use_list.nb_items; j++)
                                {
                                    if (isDerivedFromBuiltInType(
                                        wxsAttruseTypedef((Xsd10BasicItem) attr_use_list.items[j]),
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
                    else if (type.attribute_wildcard.process_contents == XML_XSD10_SCHEMAS_ANY_LAX)
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

        if (vctxt.nb_attr_infos == 0)
        {
            return 0;
        }

        /*
        * Get the owner element; needed for creation of default attributes.
        * This fixes bug #341337, reported by David Grohmann.
        */
        if ((vctxt.options & XML_XSD10_SCHEMA_VAL_VC_I_CREATE) != 0)
        {
            Xsd10NodeInfo ielem = vctxt.elem_infos[vctxt.depth];
            if ((ielem != null) && (ielem.node != null) && (nodeDom(ielem.node) != null))
            {
                def_attr_owner_elem = ielem.node;
            }
        }
        /*
        * Validate values, create default attributes, evaluate IDCs.
        */
        for (i = 0; i < vctxt.nb_attr_infos; i++)
        {
            iattr = vctxt.attr_infos[i];
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
            if (iattr.type_def == null)
            {
                iattr.state = XML_XSD10_SCHEMAS_ATTR_ERR_NO_TYPE;
                continue;
            }

            vctxt.inode = iattr;
            fixed = 0;
            xpath_res = 0;

            if (vctxt.xpath_states != null)
            {
                /*
                * Evaluate IDCs.
                */
                xpath_res = xPathEvaluate(vctxt, ATTRIBUTE);
                if (xpath_res == -1)
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
                    if ((xpath_res != 0) || (def_attr_owner_elem != null))
                    {
                        if (iattr.use.def_value != null)
                        {
                            iattr.value = iattr.use.def_value;
                            iattr.val = iattr.use.def_val;
                        }
                        else
                        {
                            iattr.value = iattr.decl.def_value;
                            iattr.val = iattr.decl.def_val;
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

                    if (def_attr_owner_elem != null)
                    {
                        String norm_value;
                        String value;

                        value = iattr.value;
                        /*
                        * Normalize the value.
                        */
                        norm_value = normalizeValue(iattr.type_def, iattr.value);
                        if (norm_value != null)
                        {
                            value = norm_value;
                        }

                        if (iattr.ns_name == null)
                        {
                            if (nodeAppendAttr(def_attr_owner_elem, iattr.local_name, null, value) == null)
                            {
                                verrorInt(vctxt, "xmlSchemaVAttributesComplex", "calling xmlNewProp()");
                                activateElem(vctxt);
                                return -1;
                            }
                        }
                        else
                        {
                            XmlNs ns;

                            ns = treeSearchNsByHref(def_attr_owner_elem, iattr.ns_name);
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
                                    ns = treeSearchNs(def_attr_owner_elem, prefix);
                                    if (counter > 1000)
                                    {
                                        verrorInt(vctxt, "xmlSchemaVAttributesComplex",
                                            "could not compute a ns prefix for a " +
                                            "default/fixed attribute");
                                        activateElem(vctxt);
                                        return -1;
                                    }
                                } while (ns != null);
                                ns = nsDeclare(vctxt.validation_root, prefix, iattr.ns_name);
                            }
                            /*
                            * TODO:
                            * http://lists.w3.org/Archives/Public/www-xml-schema-comments/2005JulSep/0406.html
                            * If we have QNames: do we need to ensure there's a
                            * prefix defined for the QName?
                            */
                            nodeAppendAttr(def_attr_owner_elem, iattr.local_name, ns, value);
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
                if ((xpath_res != 0) || (fixed != 0))
                {
                    iattr.flags |= XML_XSD10_SCHEMA_NODE_INFO_VALUE_NEEDED;
                    /*
                    * Request a computed value.
                    */
                    Xsd10Val[] aval = new Xsd10Val[] { iattr.val };
                    res = vCheckCvcSimpleType(vctxt, iattr.node, iattr.type_def, iattr.value, aval, 1, 1, 0);
                    iattr.val = aval[0];
                }
                else
                {
                    res = vCheckCvcSimpleType(vctxt, iattr.node, iattr.type_def, iattr.value, null, 1, 0, 0);
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
                    if ((iattr.use != null) && (iattr.use.def_value != null))
                    {
                        if (iattr.use.def_val == null)
                        {
                            /* VAL TODO: A default value was not precomputed. */
                            break eval_idcs;
                        }
                        iattr.vc_value = iattr.use.def_value;
                        if (areValuesEqual(iattr.val, iattr.use.def_val) == 0)
                        {
                            iattr.state = XML_XSD10_SCHEMAS_ATTR_ERR_FIXED_VALUE;
                        }
                    }
                    else
                    {
                        if (iattr.decl.def_val == null)
                        {
                            /* VAL TODO: A default value was not precomputed. */
                            break eval_idcs;
                        }
                        iattr.vc_value = iattr.decl.def_value;
                        if (areValuesEqual(iattr.val, iattr.decl.def_val) == 0)
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
            if (xpath_res != 0)
            {
                if (xPathProcessHistory(vctxt, vctxt.depth + 1) == -1)
                {
                    verrorInt(vctxt, "xmlSchemaVAttributesComplex", "calling xmlSchemaXPathEvaluate()");
                    activateElem(vctxt);
                    return -1;
                }
            }
            else if (vctxt.xpath_states != null)
            {
                xPathPop(vctxt);
            }
        }

        /*
        * Report errors.
        */
        for (i = 0; i < vctxt.nb_attr_infos; i++)
        {
            iattr = vctxt.attr_infos[i];
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
                        formatQName(str, iattr.decl.target_namespace, iattr.decl.name),
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
                        iattr.value, iattr.vc_value);
                    break;
                case XML_XSD10_SCHEMAS_ATTR_ERR_WILD_STRICT_NO_DECL:
                    verror(vctxt, XML_XSD10_SCHEMAV_CVC_WILDCARD, null,
                        "No matching global attribute declaration available, but " +
                        "demanded by the strict wildcard");
                    break;
                case XML_XSD10_SCHEMAS_ATTR_UNKNOWN:
                    if (iattr.meta_type != 0)
                    {
                        break;
                    }
                    /*
                    * MAYBE VAL TODO: One might report different error messages
                    * for the following errors.
                    */
                    if (type.attribute_wildcard == null)
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
        if (wild.process_contents == XML_XSD10_SCHEMAS_ANY_SKIP)
        {
            /*
            * URGENT VAL TODO: Either we need to position the stream to the
            * next sibling, or walk the whole subtree.
            */
            skip[0] = 1;
            return 0;
        }
        {
            Xsd10Element decl = getElem(vctxt.schema, vctxt.inode.local_name, vctxt.inode.ns_name);
            if (decl != null)
            {
                setDecl(vctxt.inode, decl);
                return 0;
            }
        }
        if (wild.process_contents == XML_XSD10_SCHEMAS_ANY_STRICT)
        {
            /* VAL TODO: Change to proper error code. */
            verror(vctxt, XML_XSD10_SCHEMAV_CVC_ELT_1, null, /* WXS_BASIC_CAST wild */
                "No matching global element declaration available, but " +
                "demanded by the strict wildcard");
            return vctxt.err;
        }
        if (vctxt.nb_attr_infos != 0)
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
                Xsd10Type[] lt = new Xsd10Type[] { ielem.type_def };
                int r = processXsiType(vctxt, iattr, lt, null);
                ielem.type_def = lt[0];
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
        vctxt.inode.type_def = getBuiltInType(XML_XSD10_SCHEMAS_ANYTYPE);
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
        if (wxsIsComplex(inode.type_def) != 0)
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
            if ((wxsHasSimpleContent(inode.type_def) == 0) &&
                ((wxsHasMixedContent(inode.type_def) == 0) || (wxsEmptiable(inode.type_def) == 0)))
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
        if (wxsIsSimple(inode.type_def) != 0)
        {
            ret = vCheckCvcSimpleType(vctxt, null, inode.type_def, value, val, 1, 1, 0);
        }
        else if (wxsHasSimpleContent(inode.type_def) != 0)
        {
            ret = vCheckCvcSimpleType(vctxt, null, inode.type_def.content_type_def, value, val, 1, 1, 0);
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
        vctxt.nb_attr_infos = 0;
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

        if (vctxt.nb_attr_infos != 0)
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
                vctxt.skip_depth = vctxt.depth - 1;
                break end_elem;
            }
            if ((inode.type_def == null) || ((inode.flags & XML_XSD10_SCHEMA_NODE_INFO_ERR_BAD_TYPE) != 0))
            {
                /*
                * 1. the type definition might be missing if the element was
                *    error prone
                * 2. it might be abstract.
                */
                break end_elem;
            }
            boolean character_content = false;
            /*
            * Check the content model.
            */
            if ((inode.type_def.content_type == XML_XSD10_SCHEMA_CONTENT_MIXED) ||
                (inode.type_def.content_type == XML_XSD10_SCHEMA_CONTENT_ELEMENTS))
            {
                /*
                * Workaround for "anyType".
                */
                if (inode.type_def.built_in_type == XML_XSD10_SCHEMAS_ANYTYPE)
                {
                    character_content = true;
                }
                else if ((inode.flags & XML_XSD10_SCHEMA_ELEM_INFO_ERR_BAD_CONTENT) == 0)
                {
                    String[] values = new String[10];
                    int[] terminal = new int[] { 0 };
                    int[] nbval = new int[] { 10 };
                    int[] nbneg = new int[] { 0 };

                    skip_nilled:
                    {
                        if (inode.regex_ctxt == null)
                        {
                            /*
                            * Create the regex context.
                            */
                            inode.regex_ctxt = XmlRegexp.regExecNew((XmlRegexp) inode.type_def.cont_model, CONTENT_MODEL_CALLBACK, vctxt);
                            if (inode.regex_ctxt == null)
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
                        XmlRegexp.regExecNextValues(inode.regex_ctxt, nbval, nbneg, values, terminal);
                        ret = XmlRegexp.regExecPushString(inode.regex_ctxt, null, null);
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

            if (!character_content)
            {
                // skip_nilled:
                if (inode.type_def.content_type == XML_XSD10_SCHEMA_CONTENT_ELEMENTS)
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
                if (wxsIsSimple(inode.type_def) != 0)
                {
                    ret = vCheckINodeDataType(vctxt, inode, inode.type_def, inode.value);
                }
                else if (wxsHasSimpleContent(inode.type_def) != 0)
                {
                    ret = vCheckINodeDataType(vctxt, inode, inode.type_def.content_type_def, inode.value);
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
                    if (wxsIsSimple(inode.type_def) != 0)
                    {
                        ret = vCheckINodeDataType(vctxt, inode, inode.type_def, inode.decl.value);
                    }
                    else if (wxsHasSimpleContent(inode.type_def) != 0)
                    {
                        ret = vCheckINodeDataType(vctxt, inode, inode.type_def.content_type_def, inode.decl.value);
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
                    XmlNode text_child;
                    String norm_value;
                    /*
                    * VAL TODO: Normalize the value.
                    */
                    norm_value = normalizeValue(inode.type_def, inode.decl.value);
                    if (norm_value != null)
                    {
                        text_child = nodeNewText(nodeDom(inode.node), norm_value);
                    }
                    else
                    {
                        text_child = nodeNewText(nodeDom(inode.node), inode.decl.value);
                    }
                    if (text_child == null)
                    {
                        verrorInt(vctxt, "xmlSchemaValidatorPopElem", "calling xmlNewDocText()");
                        vctxt.err = -1;
                        return -1;
                    }
                    else
                    {
                        nodeAddChild(inode.node, text_child);
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
                if (wxsIsSimple(inode.type_def) != 0)
                {
                    /*
                    * SPEC (cvc-type) (3.1)
                    * "If the type definition is a simple type definition, ..."
                    * (3.1.3) "If clause 3.2 of Element Locally Valid
                    * (Element) ($3.3.4) did not apply, then the `normalized value`
                    * must be `valid` with respect to the type definition as defined
                    * by String Valid ($3.14.4).
                    */
                    ret = vCheckINodeDataType(vctxt, inode, inode.type_def, inode.value);
                }
                else if (wxsHasSimpleContent(inode.type_def) != 0)
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
                    ret = vCheckINodeDataType(vctxt, inode, inode.type_def.content_type_def, inode.value);
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
                        if (wxsHasMixedContent(inode.type_def) != 0)
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
                        else if (wxsHasSimpleContent(inode.type_def) != 0)
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
        if (vctxt.depth == vctxt.skip_depth)
        {
            vctxt.skip_depth = -1;
        }
        /*
        * Evaluate the history of XPath state objects.
        */
        if ((inode.applied_x_path != 0) && (xPathProcessHistory(vctxt, vctxt.depth) == -1))
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
        if ((inode.idc_matchers != null) && ((vctxt.has_keyrefs != 0) || (vctxt.create_idc_node_tables != 0)))
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
        if (vctxt.inode.has_keyrefs != 0)
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
        if (inode.idc_table != null)
        {
            if ((vctxt.depth > 0) && ((vctxt.has_keyrefs != 0) || (vctxt.create_idc_node_tables != 0)))
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
        * Reset the keyref_depth if needed.
        */
        if (vctxt.aidcs != null)
        {
            Xsd10IDCAug aidc = vctxt.aidcs;
            do
            {
                if (aidc.keyref_depth == vctxt.depth)
                {
                    /*
                    * A 'keyref_depth' of a key/unique IDC matches the current
                    * depth, this means that we are leaving the scope of the
                    * top-most keyref IDC which refers to this IDC.
                    */
                    aidc.keyref_depth = -1;
                }
                aidc = aidc.next;
            } while (aidc != null);
        }
        vctxt.depth--;
        vctxt.inode = vctxt.elem_infos[vctxt.depth];
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
        pielem = vctxt.elem_infos[vctxt.depth - 1];
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

            ptype = pielem.type_def;

            if (ptype.built_in_type == XML_XSD10_SCHEMAS_ANYTYPE)
            {
                /*
                * Workaround for "anyType": we have currently no content model
                * assigned for "anyType", so handle it explicitly.
                * "anyType" has an unbounded, lax "any" wildcard.
                */
                setDecl(vctxt.inode, getElem(vctxt.schema, vctxt.inode.local_name, vctxt.inode.ns_name));

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
                        Xsd10Type[] lt = new Xsd10Type[] { ielem.type_def };
                        ret = processXsiType(vctxt, iattr, lt, null);
                        ielem.type_def = lt[0];
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
                        vctxt.inode.type_def = getBuiltInType(XML_XSD10_SCHEMAS_ANYTYPE);
                    }
                }
                return 0;
            }

            switch (ptype.content_type)
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
                    XmlRegExecCtxt regex_ctxt;
                    String[] values = new String[10];
                    int[] terminal = new int[] { 0 };
                    int[] nbval = new int[] { 10 };
                    int[] nbneg = new int[] { 0 };

                    /* VAL TODO: Optimized "anyType" validation.*/

                    if (ptype.cont_model == null)
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

                    regex_ctxt = pielem.regex_ctxt;
                    if (regex_ctxt == null)
                    {
                        /*
                        * Create the regex context.
                        */
                        regex_ctxt = XmlRegexp.regExecNew((XmlRegexp) ptype.cont_model, CONTENT_MODEL_CALLBACK, vctxt);
                        if (regex_ctxt == null)
                        {
                            verrorInt(vctxt, "xmlSchemaValidateChildElem", "failed to create a regex context");
                            return -1;
                        }
                        pielem.regex_ctxt = regex_ctxt;
                    }

                    /*
                    * SPEC (2.4) "If the {content type} is element-only or mixed,
                    * then the sequence of the element information item's
                    * element information item [children], if any, taken in
                    * order, is `valid` with respect to the {content type}'s
                    * particle, as defined in Element Sequence Locally Valid
                    * (Particle) ($3.9.4)."
                    */
                    ret = XmlRegexp.regExecPushString2(regex_ctxt, vctxt.inode.local_name, vctxt.inode.ns_name, vctxt.inode);
                    if (vctxt.err == XML_XSD10_SCHEMAV_INTERNAL)
                    {
                        verrorInt(vctxt, "xmlSchemaValidateChildElem", "calling xmlRegExecPushString2()");
                        return -1;
                    }
                    if (ret < 0)
                    {
                        XmlRegexp.regExecErrInfo(regex_ctxt, null, nbval, nbneg, values, terminal);
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
        * Pop this element and set the skip_depth to skip
        * all further content of the parent element.
        */
        vctxt.skip_depth = vctxt.depth;
        vctxt.inode.flags |= XML_XSD10_SCHEMA_NODE_INFO_ERR_NOT_EXPECTED;
        pielem.flags |= XML_XSD10_SCHEMA_ELEM_INFO_ERR_BAD_CONTENT;
        return ret;
    }

    private static final int XML_XSD10_SCHEMA_PUSH_TEXT_PERSIST = 1;
    private static final int XML_XSD10_SCHEMA_PUSH_TEXT_CREATED = 2;
    private static final int XML_XSD10_SCHEMA_PUSH_TEXT_VOLATILE = 3;

    // xml_xsd10_v_push_text
    /** Port of xmlSchemaVPushText. */
    static int vPushText(Xsd10ValidCtxt vctxt, int node_type, String value, int len, int mode, int[] consumed)
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
        if (vctxt.inode.type_def.content_type == XML_XSD10_SCHEMA_CONTENT_EMPTY)
        {
            verror(vctxt, XML_XSD10_SCHEMAV_CVC_COMPLEX_TYPE_2_1, null,
                "Character content is not allowed, " +
                "because the content type is empty");
            return vctxt.err;
        }

        if (vctxt.inode.type_def.content_type == XML_XSD10_SCHEMA_CONTENT_ELEMENTS)
        {
            if ((node_type != TEXT) || (isBlank(value, len) == 0))
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
        if ((vctxt.inode.type_def.content_type == XML_XSD10_SCHEMA_CONTENT_MIXED) &&
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

        if ((vctxt.skip_depth != -1) && (vctxt.depth >= vctxt.skip_depth))
        {
            verrorInt(vctxt, "xmlSchemaValidateElem", "in skip-state");
            return -1;
        }
        exit:
        {
            if (vctxt.xsi_assemble != 0)
            {
                /*
                * We will stop validation if there was an error during
                * dynamic schema construction.
                * Note that we simply set @skip_depth to 0, this could
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
                    vctxt.skip_depth = 0;
                    return ret;
                }
                /*
                 * Augment the IDC definitions for the main schema and all imported ones
                 * NOTE: main schema is the first in the imported list
                 */
                hashScan(vctxt.schema.schemas_imports, Xsd10Valid::augmentImportedIdc, vctxt);
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
                if (vctxt.depth == vctxt.skip_depth)
                {
                    break exit;
                }
                if ((getDecl(vctxt.inode) == null) && (vctxt.inode.type_def == null))
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
                setDecl(vctxt.inode, getElem(vctxt.schema, vctxt.inode.local_name, vctxt.inode.ns_name));
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
                        vctxt.skip_depth = vctxt.depth;
                        break exit;
                    }
                    /*
                    * The declaration might be set by the wildcard validation,
                    * when the process_contents is "lax" or "strict".
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

            if (vctxt.inode.type_def == null)
            {
                vctxt.inode.flags |= XML_XSD10_SCHEMA_NODE_INFO_ERR_BAD_TYPE;
                ret = XML_XSD10_SCHEMAV_CVC_TYPE_1;
                verror(vctxt, ret, null, "The type definition is absent");
                break exit;
            }
            if ((vctxt.inode.type_def.flags & XML_XSD10_SCHEMAS_TYPE_ABSTRACT) != 0)
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
            if (vctxt.xpath_states != null)
            {
                ret = xPathEvaluate(vctxt, ELEMENT);
                vctxt.inode.applied_x_path = 1;
                if (ret == -1)
                {
                    verrorInt(vctxt, "xmlSchemaValidateElem", "calling xmlSchemaXPathEvaluate()");
                    return -1;
                }
            }
            /*
            * Validate attributes.
            */
            if (wxsIsComplex(vctxt.inode.type_def) != 0)
            {
                if ((vctxt.nb_attr_infos != 0) || (vctxt.inode.type_def.attr_uses != null))
                {
                    ret = vAttributesComplex(vctxt);
                }
            }
            else if (vctxt.nb_attr_infos != 0)
            {
                ret = vAttributesSimple(vctxt);
            }
            /*
            * Clear registered attributes.
            */
            if (vctxt.nb_attr_infos != 0)
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
            vctxt.skip_depth = vctxt.depth;
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
        ret.node_q_names = itemListCreate();
        if (ret.node_q_names == null)
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
        vctxt.validation_root = null;
        vctxt.doc = null;
        /* (The reader field is not ported.) */
        vctxt.has_keyrefs = 0;
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

        if (vctxt.idc_nodes != null)
        {
            vctxt.idc_nodes = null;
            vctxt.nb_idc_nodes = 0;
            vctxt.size_idc_nodes = 0;
        }

        if (vctxt.idc_keys != null)
        {
            for (int i = 0; i < vctxt.nb_idc_keys; i++)
            {
                idcFreeKey(vctxt.idc_keys[i]);
            }
            vctxt.idc_keys = null;
            vctxt.nb_idc_keys = 0;
            vctxt.size_idc_keys = 0;
        }

        /*
        * Note that we won't delete the XPath state pool here.
        */
        if (vctxt.xpath_states != null)
        {
            freeIdcStateObjList(vctxt.xpath_states);
            vctxt.xpath_states = null;
        }
        /*
        * Attribute info.
        */
        if (vctxt.nb_attr_infos != 0)
        {
            clearAttrInfos(vctxt);
        }
        /*
        * Element info.
        */
        if (vctxt.elem_infos != null)
        {
            for (int i = 0; i < vctxt.size_elem_infos; i++)
            {
                Xsd10NodeInfo ei = vctxt.elem_infos[i];
                if (ei == null)
                {
                    break;
                }
                clearElemInfo(vctxt, ei);
            }
        }
        itemListClear(vctxt.node_q_names);
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
        if (vctxt.idc_matcher_cache != null)
        {
            Xsd10IDCMatcher matcher = vctxt.idc_matcher_cache;
            Xsd10IDCMatcher tmp;

            while (matcher != null)
            {
                tmp = matcher;
                matcher = matcher.next_cached;
                idcFreeMatcherList(tmp);
            }
            vctxt.idc_matcher_cache = null;
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
        if (ctxt.idc_keys != null)
        {
            for (int i = 0; i < ctxt.nb_idc_keys; i++)
            {
                idcFreeKey(ctxt.idc_keys[i]);
            }
        }

        if (ctxt.xpath_states != null)
        {
            freeIdcStateObjList(ctxt.xpath_states);
            ctxt.xpath_states = null;
        }
        if (ctxt.xpath_state_pool != null)
        {
            freeIdcStateObjList(ctxt.xpath_state_pool);
            ctxt.xpath_state_pool = null;
        }

        if (ctxt.attr_infos != null)
        {
            /* Just a paranoid call to the cleanup. */
            if (ctxt.nb_attr_infos != 0)
            {
                clearAttrInfos(ctxt);
            }
        }
        if (ctxt.elem_infos != null)
        {
            for (int i = 0; i < ctxt.size_elem_infos; i++)
            {
                Xsd10NodeInfo ei = ctxt.elem_infos[i];
                if (ei == null)
                {
                    break;
                }
                clearElemInfo(ctxt, ei);
            }
        }
        if (ctxt.node_q_names != null)
        {
            itemListFree(ctxt.node_q_names);
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
        XmlNode val_root;
        String ns_name;

        /* DOC VAL TODO: Move this to the start function. */
        if (vctxt.validation_root != null)
        {
            val_root = vctxt.validation_root;
        }
        else
        {
            val_root = domRootElement(vctxt.doc);
        }
        if (val_root == null)
        {
            /* VAL TODO: Error code? */
            verror(vctxt, 1, null, "The document has no document element");
            return 1;
        }
        vctxt.depth = -1;
        vctxt.validation_root = val_root;
        node = val_root;
        /* 0: loop body, 1: leave_node, 2: next_sibling (the C gotos) */
        int state = 0;
        while (node != null)
        {
            if (state == 0)
            {
                state = 1;
                if ((vctxt.skip_depth != -1) && (vctxt.depth >= vctxt.skip_depth))
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
                    ielem.node_line = nodeLine(node);
                    ielem.local_name = nodeName(node);
                    if (nodeNs(node) != null)
                    {
                        ielem.ns_name = nodeNsUri(node);
                    }
                    ielem.flags |= XML_XSD10_SCHEMA_ELEM_INFO_EMPTY;
                    /*
                    * Register attributes.
                    * DOC VAL TODO: We do not register namespace declaration
                    * attributes yet.
                    */
                    vctxt.nb_attr_infos = 0;
                    if (nodeFirstAttr(node) != null)
                    {
                        attr = nodeFirstAttr(node);
                        do
                        {
                            String content;

                            if (nodeNs(attrNode(attr)) != null)
                            {
                                ns_name = nodeNsUri(attrNode(attr));
                            }
                            else
                            {
                                ns_name = null;
                            }
                            content = treeNodeGetContent(attrNode(attr));
                            ret = validatorPushAttribute(vctxt,
                                attrNode(attr),
                                /*
                                * Note that we give it the line number of the
                                * parent element.
                                */
                                ielem.node_line,
                                attrName(attr), ns_name, 0,
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
                    else if (!((vctxt.skip_depth != -1) && (vctxt.depth >= vctxt.skip_depth)))
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
                    if (node == val_root)
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
        vctxt.skip_depth = -1;
        vctxt.has_keyrefs = 0;
        vctxt.create_idc_node_tables = 0;
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

            vctxt.xsi_assemble = 1;
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
            pctxt.xsi_assemble = 1;
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
            pctxt.constructor.main_schema = vctxt.schema;
            /*
            * Take ownership of the constructor to be able to free it.
            */
            pctxt.owns_constructor = 1;
        }
        /*
        * Augment the IDC definitions for the main schema and all imported ones
        * NOTE: main schema if the first in the imported list
        */
        hashScan(vctxt.schema.schemas_imports, Xsd10Valid::augmentImportedIdc, vctxt);

        return 0;
    }

    // xml_xsd10_post_run
    /** Port of xmlSchemaPostRun. */
    static void postRun(Xsd10ValidCtxt vctxt)
    {
        if (vctxt.xsi_assemble != 0)
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
        ctxt.validation_root = elem;
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
        ctxt.validation_root = ctxt.node;
        return vStart(ctxt);
    }
}
