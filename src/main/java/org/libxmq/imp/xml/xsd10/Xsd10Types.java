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

/* Portions of this file are ported from libxml2 (xmlschemastypes.c, and the parts of
   xmlschemas.c, tree.c, parser.c, xmlstring.c and valid.c it uses, version 2.16.0 as
   found in the libxmq fork in 3rdparty/libxml2).
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

import java.math.BigDecimal;
import java.math.MathContext;
import java.math.RoundingMode;
import java.nio.charset.StandardCharsets;
import java.util.Iterator;
import java.util.Map;
import java.util.Objects;

import org.libxmq.imp.xml.XmlEntityDecl;
import org.libxmq.imp.xml.XmlNode;
import org.libxmq.imp.xml.XmlNs;
import org.libxmq.imp.xml.XmlUri;

import static org.libxmq.imp.xml.XmlDomApi.*;
import static org.libxmq.imp.xml.xsd10.Xsd10Const.*;

/** Port of xml_xsd10_types.c: the built in simple types, values, facets and the ID table.
    The C code parses the values as NUL terminated unsigned byte strings (libxml2 xmlChar). The
    port does the same: the values are converted to UTF-8 byte arrays with a terminating 0 and all
    accesses go through {@link #at} which returns 0 past the end, so no input can throw. */
public final class Xsd10Types
{
    private Xsd10Types()
    {
    }

    private static final String XSD10_TYPES_NS = "http://www.w3.org/2001/XMLSchema";

    // Byte string helpers (the C XmlXsd10Char pointers) ///////////////////////////

    /** UTF-8 bytes of s plus a terminating 0, null for null. */
    private static byte[] b(String s)
    {
        if (s == null) return null;
        byte[] u = XmlUri.toUtf8(s);
        byte[] r = new byte[u.length + 1];
        System.arraycopy(u, 0, r, 0, u.length);
        return r;
    }

    /** The unsigned byte at i, 0 outside of the array (past the C terminator). */
    private static int at(byte[] s, int i)
    {
        if (s == null || i < 0 || i >= s.length) return 0;
        return s[i] & 0xff;
    }

    /** C strlen from start. */
    private static int clen(byte[] s, int start)
    {
        int i = start;
        while (at(s, i) != 0) i++;
        return i - start;
    }

    /** The C string at start as a Java String. */
    private static String str(byte[] s, int start)
    {
        if (s == null) return null;
        return strn(s, start, clen(s, start));
    }

    /** len bytes at start as a Java String. */
    private static String strn(byte[] s, int start, int len)
    {
        byte[] r = new byte[len];
        for (int i = 0; i < len; i++) r[i] = (byte) at(s, start + i);
        return XmlUri.fromUtf8(r);
    }

    /** Port of xmlStrndup on bytes: a new NUL terminated byte string. */
    private static byte[] cbytes(byte[] s, int start, int len)
    {
        if (s == null || len < 0) return null;
        byte[] r = new byte[len + 1];
        for (int i = 0; i < len; i++) r[i] = (byte) at(s, start + i);
        return r;
    }

    private static boolean isBlankCh(int c)
    {
        return (c == 0x20) || ((0x9 <= c) && (c <= 0xa)) || (c == 0xd);
    }

    private static boolean isWspReplaceCh(int c)
    {
        return (c == 0x9) || (c == 0xa) || (c == 0xd);
    }

    private static boolean isWspSpaceCh(int c)
    {
        return c == 0x20;
    }

    private static boolean isDigit(int c)
    {
        return c >= '0' && c <= '9';
    }

    // The regexp hooks ////////////////////////////////////////////////////////////

    /* The regexp implementation for pattern facets. */
    private static final Xsd10RegexpHooks xsd10_types_regexp_hooks_ = new Xsd10RegexpHooks();

    // xml_xsd10_set_regexp_hooks
    /** Port of xml_xsd10_set_regexp_hooks.
        @param hooks the hooks, null to remove them */
    public static void setRegexpHooks(Xsd10RegexpHooks hooks)
    {
        if (hooks == null)
        {
            xsd10_types_regexp_hooks_.compile = null;
            xsd10_types_regexp_hooks_.exec = null;
            xsd10_types_regexp_hooks_.free = null;
        }
        else
        {
            xsd10_types_regexp_hooks_.compile = hooks.compile;
            xsd10_types_regexp_hooks_.exec = hooks.exec;
            xsd10_types_regexp_hooks_.free = hooks.free;
        }
    }

    // xsd10_types_regexp_exec
    /* libxml2 xmlRegexpExec: -1 without a regexp. */
    private static int typesRegexpExec(Object regexp, String value)
    {
        if ((regexp == null) || (value == null) || (xsd10_types_regexp_hooks_.exec == null))
            return -1;
        return xsd10_types_regexp_hooks_.exec.exec(regexp, value);
    }

    // xsd10_types_regexp_free
    private static void typesRegexpFree(Object regexp)
    {
        if ((regexp != null) && (xsd10_types_regexp_hooks_.free != null))
            xsd10_types_regexp_hooks_.free.free(regexp);
    }

    // String helpers (libxml2 xmlstring.c) ///////////////////////////////////////

    // xsd10_types_str_equal
    /* Port of xmlStrEqual. */
    private static int typesStrEqual(byte[] str1, byte[] str2)
    {
        if (str1 == str2) return 1;
        if (str1 == null) return 0;
        if (str2 == null) return 0;
        int i = 0;
        while (true)
        {
            int a = at(str1, i), c = at(str2, i);
            if (a != c) return 0;
            if (a == 0) return 1;
            i++;
        }
    }

    // xsd10_types_strcmp
    /* Port of xmlStrcmp. */
    private static int typesStrcmp(String str1, String str2)
    {
        if (str1 == str2) return 0;
        if (str1 == null) return -1;
        if (str2 == null) return 1;
        byte[] a = b(str1), c = b(str2);
        int i = 0;
        while (true)
        {
            int x = at(a, i), y = at(c, i);
            if (x != y) return x - y;
            if (x == 0) return 0;
            i++;
        }
    }

    // xsd10_types_utf8_strlen
    /* Port of xmlUTF8Strlen. */
    private static int typesUtf8Strlen(byte[] utf)
    {
        long ret = 0;
        int p = 0;

        if (utf == null)
            return -1;

        while (at(utf, p) != 0)
        {
            if ((at(utf, p) & 0x80) != 0)
            {
                if ((at(utf, p + 1) & 0xc0) != 0x80)
                    return -1;
                if ((at(utf, p) & 0xe0) == 0xe0)
                {
                    if ((at(utf, p + 2) & 0xc0) != 0x80)
                        return -1;
                    if ((at(utf, p) & 0xf0) == 0xf0)
                    {
                        if ((at(utf, p) & 0xf8) != 0xf0 || (at(utf, p + 3) & 0xc0) != 0x80)
                            return -1;
                        p += 4;
                    }
                    else
                    {
                        p += 3;
                    }
                }
                else
                {
                    p += 2;
                }
            }
            else
            {
                p++;
            }
            ret++;
        }
        return (ret > Integer.MAX_VALUE ? 0 : (int) ret);
    }

    // xsd10_types_get_utf8_char
    /* Port of xmlGetUTF8Char. */
    private static int typesGetUtf8Char(byte[] utf, int p, int[] len)
    {
        int c;

        if ((utf == null) || (len == null))
        {
            if (len != null)
                len[0] = 0;
            return -1;
        }

        c = at(utf, p);
        if (c < 0x80)
        {
            if (len[0] < 1)
            {
                len[0] = 0;
                return -1;
            }
            /* 1-byte code */
            len[0] = 1;
        }
        else
        {
            if ((len[0] < 2) || ((at(utf, p + 1) & 0xc0) != 0x80))
            {
                len[0] = 0;
                return -1;
            }
            if (c < 0xe0)
            {
                if (c < 0xc2)
                {
                    len[0] = 0;
                    return -1;
                }
                /* 2-byte code */
                len[0] = 2;
                c = (c & 0x1f) << 6;
                c |= at(utf, p + 1) & 0x3f;
            }
            else
            {
                if ((len[0] < 3) || ((at(utf, p + 2) & 0xc0) != 0x80))
                {
                    len[0] = 0;
                    return -1;
                }
                if (c < 0xf0)
                {
                    /* 3-byte code */
                    len[0] = 3;
                    c = (c & 0xf) << 12;
                    c |= (at(utf, p + 1) & 0x3f) << 6;
                    c |= at(utf, p + 2) & 0x3f;
                    if ((c < 0x800) || ((c >= 0xd800) && (c < 0xe000)))
                    {
                        len[0] = 0;
                        return -1;
                    }
                }
                else
                {
                    if ((len[0] < 4) || ((at(utf, p + 3) & 0xc0) != 0x80))
                    {
                        len[0] = 0;
                        return -1;
                    }
                    len[0] = 4;
                    /* 4-byte code */
                    c = (c & 0x7) << 18;
                    c |= (at(utf, p + 1) & 0x3f) << 12;
                    c |= (at(utf, p + 2) & 0x3f) << 6;
                    c |= at(utf, p + 3) & 0x3f;
                    if ((c < 0x10000) || (c >= 0x110000))
                    {
                        len[0] = 0;
                        return -1;
                    }
                }
            }
        }
        return c;
    }

    // Name, NCName, QName and NMToken (libxml2 parser.c, tree.c) /////////////////

    // xsd10_types_is_name_start_char
    /* Port of xmlIsNameStartCharNew (XML 1.0 fifth edition). */
    private static boolean typesIsNameStartChar(int c)
    {
        return (c != ' ') && (c != '>') && (c != '/') && /* accelerators */
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
             ((c >= 0x10000) && (c <= 0xEFFFF)));
    }

    // xsd10_types_is_name_char
    /* Port of xmlIsNameCharNew (XML 1.0 fifth edition). */
    private static boolean typesIsNameChar(int c)
    {
        return (c != ' ') && (c != '>') && (c != '/') && /* accelerators */
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
             ((c >= 0x10000) && (c <= 0xEFFFF)));
    }

    private static final int XSD10_TYPES_SCAN_NC = 1;
    private static final int XSD10_TYPES_SCAN_NMTOKEN = 2;

    // xsd10_types_scan_name
    /* Port of xmlScanName (without a size limit and without XML_SCAN_OLD10):
       returns the index of the end of the name, ptr if the name is invalid. */
    private static int typesScanName(byte[] s, int ptr, int flags)
    {
        int stop = (flags & XSD10_TYPES_SCAN_NC) != 0 ? ':' : 0;
        int[] len = new int[1];

        while (true)
        {
            int c;

            c = at(s, ptr);
            if (c < 0x80)
            {
                if (c == stop)
                    break;
                len[0] = 1;
            }
            else
            {
                len[0] = 4;
                c = typesGetUtf8Char(s, ptr, len);
                if (c < 0)
                    break;
            }

            if ((flags & XSD10_TYPES_SCAN_NMTOKEN) != 0 ?
                    !typesIsNameChar(c) :
                    !typesIsNameStartChar(c))
                break;

            ptr += len[0];
            flags |= XSD10_TYPES_SCAN_NMTOKEN;
        }

        return ptr;
    }

    // xsd10_types_validate_scanned
    /* Port of xmlValidateNCName (also used for Name and NMToken with other scan flags). */
    private static int typesValidateScanned(byte[] value, int space, int flags)
    {
        int cur;

        if (value == null)
            return -1;

        cur = 0;

        if (space != 0)
        {
            while (isBlankCh(at(value, cur)))
                cur++;
        }

        int v = cur;
        cur = typesScanName(value, v, flags);
        if (cur == v)
            return 1;

        if (space != 0)
        {
            while (isBlankCh(at(value, cur)))
                cur++;
        }

        return at(value, cur) != 0 ? 1 : 0;
    }

    // xsd10_types_validate_ncname
    /* Port of xmlValidateNCName. */
    private static int typesValidateNcname(byte[] value, int space)
    {
        return typesValidateScanned(value, space, XSD10_TYPES_SCAN_NC);
    }

    // xsd10_types_validate_name
    /* Port of xmlValidateName. */
    private static int typesValidateName(byte[] value, int space)
    {
        return typesValidateScanned(value, space, 0);
    }

    // xsd10_types_validate_nmtoken
    /* Port of xmlValidateNMToken. */
    private static int typesValidateNmtoken(byte[] value, int space)
    {
        return typesValidateScanned(value, space, XSD10_TYPES_SCAN_NMTOKEN);
    }

    // xsd10_types_validate_qname
    /* Port of xmlValidateQName. */
    private static int typesValidateQname(byte[] value, int space)
    {
        int cur;

        if (value == null)
            return -1;

        cur = 0;

        if (space != 0)
        {
            while (isBlankCh(at(value, cur)))
                cur++;
        }

        int v = cur;
        cur = typesScanName(value, v, XSD10_TYPES_SCAN_NC);
        if (cur == v)
            return 1;

        if (at(value, cur) == ':')
        {
            cur += 1;
            v = cur;
            cur = typesScanName(value, v, XSD10_TYPES_SCAN_NC);
            if (cur == v)
                return 1;
        }

        if (space != 0)
        {
            while (isBlankCh(at(value, cur)))
                cur++;
        }

        return at(value, cur) != 0 ? 1 : 0;
    }

    // xsd10_types_split_qname2
    /* Port of xmlSplitQName2: null if the name has no prefix, otherwise the local part
       and prefix[0] the prefix. */
    private static String typesSplitQname2(byte[] name, String[] prefix)
    {
        int len = 0;

        if (prefix == null) return null;
        prefix[0] = null;
        if (name == null) return null;

        /* nasty but valid */
        if (at(name, 0) == ':')
            return null;

        /*
         * we are not trying to validate but just to cut, and yes it will
         * work even if this is as set of UTF-8 encoded chars
         */
        while ((at(name, len) != 0) && (at(name, len) != ':'))
            len++;

        if ((at(name, len) == 0) || (at(name, len + 1) == 0))
            return null;

        prefix[0] = strn(name, 0, len);
        return str(name, len + 1);
    }

    // xsd10_types_search_ns
    /* Replaces libxml2 xmlSearchNs(node->doc, node, prefix): the in scope namespace uri of the
       prefix (null for the default namespace), null if not declared. */
    private static String typesSearchNs(XmlNode node, String prefix, int[] found)
    {
        XmlNs ns;

        found[0] = 0;
        if (node == null)
            return null;
        ns = nsLookupPrefix(node, prefix);
        if ((ns == null) || (nsHref(ns) == null))
            return null;
        found[0] = 1;
        return nsHref(ns);
    }

    // The ID table ///////////////////////////////////////////////////////////////

    // xml_xsd10_ids_new
    /** Port of xml_xsd10_ids_new.
        @return a new empty ID table */
    public static Xsd10Ids idsNew()
    {
        return new Xsd10Ids();
    }

    // xml_xsd10_ids_free
    /** Port of xml_xsd10_ids_free.
        @param ids the table (may be null) */
    public static void idsFree(Xsd10Ids ids)
    {
        if (ids == null)
            return;
        ids.ids.clear();
        ids.atypes.clear();
        ids.refs.clear();
    }

    // xml_xsd10_ids_attr_type
    /** Port of xml_xsd10_ids_attr_type.
        @param ids the table @param attr the attribute @return the attribute type, 0 if none */
    public static int idsAttrType(Xsd10Ids ids, XmlNode attr)
    {
        if ((ids == null) || (attr == null))
            return 0;
        Integer t = ids.atypes.get(attr);
        return t == null ? 0 : t;
    }

    // xml_xsd10_ids_set_attr_type
    /** Port of xml_xsd10_ids_set_attr_type.
        @param ids the table @param attr the attribute @param atype the type, 0 removes it */
    public static void idsSetAttrType(Xsd10Ids ids, XmlNode attr, int atype)
    {
        if ((ids == null) || (attr == null))
            return;
        if (atype == 0)
            ids.atypes.remove(attr);
        else
            ids.atypes.put(attr, atype);
    }

    // xml_xsd10_ids_add_id
    /** Port of xmlAddIDSafe (xmlAddIDInternal).
        @param ids the table @param attr the attribute @param value the id
        @return 1 if added, 0 if a duplicate or no value */
    public static int idsAddId(Xsd10Ids ids, XmlNode attr, String value)
    {
        if ((value == null) || (value.isEmpty()))
            return 0;
        if ((attr == null) || (ids == null))
            return 0;

        if (ids.ids.get(value) != null)
            return 0;

        /* An attribute holds one id (libxml2 xmlRemoveID of the previous one). */
        Iterator<Map.Entry<String, XmlNode>> it = ids.ids.entrySet().iterator();
        while (it.hasNext())
        {
            if (it.next().getValue() == attr)
            {
                it.remove();
                break;
            }
        }

        ids.ids.put(value, attr);
        idsSetAttrType(ids, attr, XML_XSD10_ATTRIBUTE_ID);
        return 1;
    }

    // xml_xsd10_ids_lookup
    /** Port of xml_xsd10_ids_lookup.
        @param ids the table @param value the id @return the attribute or null */
    public static XmlNode idsLookup(Xsd10Ids ids, String value)
    {
        if ((ids == null) || (value == null))
            return null;
        return ids.ids.get(value);
    }

    // xml_xsd10_ids_add_ref
    /** Port of xmlAddRef (the list of references of the document).
        @param ids the table @param attr the attribute @param value the referenced id */
    public static void idsAddRef(Xsd10Ids ids, XmlNode attr, String value)
    {
        if ((ids == null) || (value == null) || (attr == null))
            return;
        Xsd10Ids.IdRef r = new Xsd10Ids.IdRef();
        r.value = value;
        r.attr = attr;
        ids.refs.add(r);
    }

    // xml_xsd10_ids_num_refs
    /** Port of xml_xsd10_ids_num_refs.
        @param ids the table @return the number of references */
    public static long idsNumRefs(Xsd10Ids ids)
    {
        return (ids != null ? ids.refs.size() : 0);
    }

    // xml_xsd10_ids_ref_value
    /** Port of xml_xsd10_ids_ref_value.
        @param ids the table @param i the index @return the value or null */
    public static String idsRefValue(Xsd10Ids ids, long i)
    {
        if ((ids == null) || (Long.compareUnsigned(i, ids.refs.size()) >= 0))
            return null;
        return ids.refs.get((int) i).value;
    }

    // xml_xsd10_ids_ref_attr
    /** Port of xml_xsd10_ids_ref_attr.
        @param ids the table @param i the index @return the attribute or null */
    public static XmlNode idsRefAttr(Xsd10Ids ids, long i)
    {
        if ((ids == null) || (Long.compareUnsigned(i, ids.refs.size()) >= 0))
            return null;
        return ids.refs.get((int) i).attr;
    }

    // The built in types /////////////////////////////////////////////////////////

    /*
     * libxml2 builds the built in types in a global hash table on first use
     * (xmlSchemaInitTypes, protected by a mutex). Here they are static data,
     * fully initialized at class load: the array xsd10_types_builtin_ is indexed
     * by the XmlXsd10ValType of the type (index 0 is unused).
     */
    private static final Xsd10Type[] xsd10_types_builtin_ = new Xsd10Type[XML_XSD10_SCHEMAS_ANYSIMPLETYPE + 1];

    private static final int XSD10_TYPES_PRIMITIVE = (XML_XSD10_SCHEMAS_TYPE_BUILTIN_PRIMITIVE | XML_XSD10_SCHEMAS_TYPE_VARIETY_ATOMIC);
    private static final int XSD10_TYPES_ATOMIC = XML_XSD10_SCHEMAS_TYPE_VARIETY_ATOMIC;
    private static final int XSD10_TYPES_LIST = (XML_XSD10_SCHEMAS_TYPE_VARIETY_LIST | XML_XSD10_SCHEMAS_TYPE_HAS_FACETS);

    /* The minLength facet of the built in list types, port of xmlSchemaNewMinLengthFacet(1):
       a nonNegativeInteger value "+1.0". */
    private static Xsd10Facet minLengthFacet()
    {
        Xsd10Val v = new Xsd10Val();
        v.type = XML_XSD10_SCHEMAS_NNINTEGER;
        v.decimal.str = "+1.0";
        v.decimal.integralPlaces = 1;
        v.decimal.fractionalPlaces = 1;
        Xsd10Facet f = new Xsd10Facet();
        f.type = XML_XSD10_SCHEMA_FACET_MINLENGTH;
        f.val = v;
        return f;
    }

    private static Xsd10Type ref(int t)
    {
        return xsd10_types_builtin_[t];
    }

    /* Port of xmlSchemaInitBasicType: one built in type. */
    private static void def(String name, int t, Xsd10Type base, int flags, Xsd10Facet facets,
                            Xsd10BasicItem subtypes, int contentType, Xsd10Wildcard wildcard)
    {
        Xsd10Type ty = xsd10_types_builtin_[t];
        ty.type = XML_XSD10_SCHEMA_TYPE_BASIC;
        ty.name = name;
        ty.subtypes = subtypes;
        ty.flags = flags;
        ty.contentType = contentType;
        ty.baseType = base;
        ty.facets = facets;
        ty.attributeWildcard = wildcard;
        ty.builtInType = t;
        ty.targetNamespace = XSD10_TYPES_NS;
    }

    private static void simple(String name, int t, int base, int flags)
    {
        def(name, t, ref(base), flags, null, null, XML_XSD10_SCHEMA_CONTENT_BASIC, null);
    }

    private static void listdef(String name, int t, int item)
    {
        def(name, t, ref(XML_XSD10_SCHEMAS_ANYSIMPLETYPE), XSD10_TYPES_LIST, minLengthFacet(),
            ref(item), XML_XSD10_SCHEMA_CONTENT_BASIC, null);
    }

    static
    {
        for (int i = 0; i < xsd10_types_builtin_.length; i++) xsd10_types_builtin_[i] = new Xsd10Type();

        /* The content type of xs:anyType (3.4.7 Built-in Complex Type Definition): a particle
           holding a sequence holding a particle (minOccurs 0, maxOccurs unbounded) of a lax
           wildcard, plus a lax attribute wildcard. */
        Xsd10Wildcard anyWildcard = new Xsd10Wildcard();
        anyWildcard.type = XML_XSD10_SCHEMA_TYPE_ANY;
        anyWildcard.processContents = XML_XSD10_SCHEMAS_ANY_LAX;
        anyWildcard.any = 1;
        Xsd10Wildcard anyAttributeWildcard = new Xsd10Wildcard();
        anyAttributeWildcard.type = 0;
        anyAttributeWildcard.processContents = XML_XSD10_SCHEMAS_ANY_LAX;
        anyAttributeWildcard.any = 1;
        Xsd10Particle anyInnerParticle = new Xsd10Particle();
        anyInnerParticle.type = XML_XSD10_SCHEMA_TYPE_PARTICLE;
        anyInnerParticle.children = anyWildcard;
        anyInnerParticle.minOccurs = 0;
        anyInnerParticle.maxOccurs = XML_XSD10_UNBOUNDED;
        Xsd10ModelGroup anySequence = new Xsd10ModelGroup();
        anySequence.type = XML_XSD10_SCHEMA_TYPE_SEQUENCE;
        anySequence.children = anyInnerParticle;
        Xsd10Particle anyParticle = new Xsd10Particle();
        anyParticle.type = XML_XSD10_SCHEMA_TYPE_PARTICLE;
        anyParticle.children = anySequence;
        anyParticle.minOccurs = 1;
        anyParticle.maxOccurs = 1;

        /* 0 XML_XSD10_SCHEMAS_UNKNOWN, unused */
        def(null, XML_XSD10_SCHEMAS_UNKNOWN, null, 0, null, null, XML_XSD10_SCHEMA_CONTENT_UNKNOWN, null);
        simple("string", XML_XSD10_SCHEMAS_STRING, XML_XSD10_SCHEMAS_ANYSIMPLETYPE, XSD10_TYPES_PRIMITIVE);
        simple("normalizedString", XML_XSD10_SCHEMAS_NORMSTRING, XML_XSD10_SCHEMAS_STRING, XSD10_TYPES_ATOMIC);
        simple("decimal", XML_XSD10_SCHEMAS_DECIMAL, XML_XSD10_SCHEMAS_ANYSIMPLETYPE, XSD10_TYPES_PRIMITIVE);
        simple("time", XML_XSD10_SCHEMAS_TIME, XML_XSD10_SCHEMAS_ANYSIMPLETYPE, XSD10_TYPES_PRIMITIVE);
        simple("gDay", XML_XSD10_SCHEMAS_GDAY, XML_XSD10_SCHEMAS_ANYSIMPLETYPE, XSD10_TYPES_PRIMITIVE);
        simple("gMonth", XML_XSD10_SCHEMAS_GMONTH, XML_XSD10_SCHEMAS_ANYSIMPLETYPE, XSD10_TYPES_PRIMITIVE);
        simple("gMonthDay", XML_XSD10_SCHEMAS_GMONTHDAY, XML_XSD10_SCHEMAS_ANYSIMPLETYPE, XSD10_TYPES_PRIMITIVE);
        simple("gYear", XML_XSD10_SCHEMAS_GYEAR, XML_XSD10_SCHEMAS_ANYSIMPLETYPE, XSD10_TYPES_PRIMITIVE);
        simple("gYearMonth", XML_XSD10_SCHEMAS_GYEARMONTH, XML_XSD10_SCHEMAS_ANYSIMPLETYPE, XSD10_TYPES_PRIMITIVE);
        simple("date", XML_XSD10_SCHEMAS_DATE, XML_XSD10_SCHEMAS_ANYSIMPLETYPE, XSD10_TYPES_PRIMITIVE);
        simple("dateTime", XML_XSD10_SCHEMAS_DATETIME, XML_XSD10_SCHEMAS_ANYSIMPLETYPE, XSD10_TYPES_PRIMITIVE);
        simple("duration", XML_XSD10_SCHEMAS_DURATION, XML_XSD10_SCHEMAS_ANYSIMPLETYPE, XSD10_TYPES_PRIMITIVE);
        simple("float", XML_XSD10_SCHEMAS_FLOAT, XML_XSD10_SCHEMAS_ANYSIMPLETYPE, XSD10_TYPES_PRIMITIVE);
        simple("double", XML_XSD10_SCHEMAS_DOUBLE, XML_XSD10_SCHEMAS_ANYSIMPLETYPE, XSD10_TYPES_PRIMITIVE);
        simple("boolean", XML_XSD10_SCHEMAS_BOOLEAN, XML_XSD10_SCHEMAS_ANYSIMPLETYPE, XSD10_TYPES_PRIMITIVE);
        simple("token", XML_XSD10_SCHEMAS_TOKEN, XML_XSD10_SCHEMAS_NORMSTRING, XSD10_TYPES_ATOMIC);
        simple("language", XML_XSD10_SCHEMAS_LANGUAGE, XML_XSD10_SCHEMAS_TOKEN, XSD10_TYPES_ATOMIC);
        simple("NMTOKEN", XML_XSD10_SCHEMAS_NMTOKEN, XML_XSD10_SCHEMAS_TOKEN, XSD10_TYPES_ATOMIC);
        listdef("NMTOKENS", XML_XSD10_SCHEMAS_NMTOKENS, XML_XSD10_SCHEMAS_NMTOKEN);
        simple("Name", XML_XSD10_SCHEMAS_NAME, XML_XSD10_SCHEMAS_TOKEN, XSD10_TYPES_ATOMIC);
        simple("QName", XML_XSD10_SCHEMAS_QNAME, XML_XSD10_SCHEMAS_ANYSIMPLETYPE, XSD10_TYPES_PRIMITIVE);
        simple("NCName", XML_XSD10_SCHEMAS_NCNAME, XML_XSD10_SCHEMAS_NAME, XSD10_TYPES_ATOMIC);
        simple("ID", XML_XSD10_SCHEMAS_ID, XML_XSD10_SCHEMAS_NCNAME, XSD10_TYPES_ATOMIC);
        simple("IDREF", XML_XSD10_SCHEMAS_IDREF, XML_XSD10_SCHEMAS_NCNAME, XSD10_TYPES_ATOMIC);
        listdef("IDREFS", XML_XSD10_SCHEMAS_IDREFS, XML_XSD10_SCHEMAS_IDREF);
        simple("ENTITY", XML_XSD10_SCHEMAS_ENTITY, XML_XSD10_SCHEMAS_NCNAME, XSD10_TYPES_ATOMIC);
        listdef("ENTITIES", XML_XSD10_SCHEMAS_ENTITIES, XML_XSD10_SCHEMAS_ENTITY);
        simple("NOTATION", XML_XSD10_SCHEMAS_NOTATION, XML_XSD10_SCHEMAS_ANYSIMPLETYPE, XSD10_TYPES_PRIMITIVE);
        simple("anyURI", XML_XSD10_SCHEMAS_ANYURI, XML_XSD10_SCHEMAS_ANYSIMPLETYPE, XSD10_TYPES_PRIMITIVE);
        simple("integer", XML_XSD10_SCHEMAS_INTEGER, XML_XSD10_SCHEMAS_DECIMAL, XSD10_TYPES_ATOMIC);
        simple("nonPositiveInteger", XML_XSD10_SCHEMAS_NPINTEGER, XML_XSD10_SCHEMAS_INTEGER, XSD10_TYPES_ATOMIC);
        simple("negativeInteger", XML_XSD10_SCHEMAS_NINTEGER, XML_XSD10_SCHEMAS_NPINTEGER, XSD10_TYPES_ATOMIC);
        simple("nonNegativeInteger", XML_XSD10_SCHEMAS_NNINTEGER, XML_XSD10_SCHEMAS_INTEGER, XSD10_TYPES_ATOMIC);
        simple("positiveInteger", XML_XSD10_SCHEMAS_PINTEGER, XML_XSD10_SCHEMAS_NNINTEGER, XSD10_TYPES_ATOMIC);
        simple("int", XML_XSD10_SCHEMAS_INT, XML_XSD10_SCHEMAS_LONG, XSD10_TYPES_ATOMIC);
        simple("unsignedInt", XML_XSD10_SCHEMAS_UINT, XML_XSD10_SCHEMAS_ULONG, XSD10_TYPES_ATOMIC);
        simple("long", XML_XSD10_SCHEMAS_LONG, XML_XSD10_SCHEMAS_INTEGER, XSD10_TYPES_ATOMIC);
        simple("unsignedLong", XML_XSD10_SCHEMAS_ULONG, XML_XSD10_SCHEMAS_NNINTEGER, XSD10_TYPES_ATOMIC);
        simple("short", XML_XSD10_SCHEMAS_SHORT, XML_XSD10_SCHEMAS_INT, XSD10_TYPES_ATOMIC);
        simple("unsignedShort", XML_XSD10_SCHEMAS_USHORT, XML_XSD10_SCHEMAS_UINT, XSD10_TYPES_ATOMIC);
        simple("byte", XML_XSD10_SCHEMAS_BYTE, XML_XSD10_SCHEMAS_SHORT, XSD10_TYPES_ATOMIC);
        simple("unsignedByte", XML_XSD10_SCHEMAS_UBYTE, XML_XSD10_SCHEMAS_USHORT, XSD10_TYPES_ATOMIC);
        simple("hexBinary", XML_XSD10_SCHEMAS_HEXBINARY, XML_XSD10_SCHEMAS_ANYSIMPLETYPE, XSD10_TYPES_PRIMITIVE);
        simple("base64Binary", XML_XSD10_SCHEMAS_BASE64BINARY, XML_XSD10_SCHEMAS_ANYSIMPLETYPE, XSD10_TYPES_PRIMITIVE);
        /* anyType is its own base type, its content type is mixed. */
        def("anyType", XML_XSD10_SCHEMAS_ANYTYPE, ref(XML_XSD10_SCHEMAS_ANYTYPE), 0, null,
            anyParticle, XML_XSD10_SCHEMA_CONTENT_MIXED, anyAttributeWildcard);
        def("anySimpleType", XML_XSD10_SCHEMAS_ANYSIMPLETYPE, ref(XML_XSD10_SCHEMAS_ANYTYPE), 0, null, null,
            XML_XSD10_SCHEMA_CONTENT_BASIC, null);
    }

    private static Xsd10Type floatDef()
    {
        return xsd10_types_builtin_[XML_XSD10_SCHEMAS_FLOAT];
    }

    // xml_xsd10_init_types
    /** Port of xmlSchemaInitTypes.
        @return 0 */
    public static int initTypes()
    {
        return 0;
    }

    // xml_xsd10_cleanup_types
    /** Port of xmlSchemaCleanupTypes. */
    public static void cleanupTypes()
    {
    }

    // xml_xsd10_is_built_in_type_facet
    /** Port of xmlSchemaIsBuiltInTypeFacet.
        @param type the built in type @param facetType the facet type
        @return 1 if allowed, 0 if not, -1 on error */
    public static int isBuiltInTypeFacet(Xsd10Type type, int facetType)
    {
        if (type == null)
            return -1;
        if (type.type != XML_XSD10_SCHEMA_TYPE_BASIC)
            return -1;
        switch (type.builtInType)
        {
            case XML_XSD10_SCHEMAS_BOOLEAN:
                if ((facetType == XML_XSD10_SCHEMA_FACET_PATTERN) ||
                    (facetType == XML_XSD10_SCHEMA_FACET_WHITESPACE))
                    return 1;
                else
                    return 0;
            case XML_XSD10_SCHEMAS_STRING:
            case XML_XSD10_SCHEMAS_NOTATION:
            case XML_XSD10_SCHEMAS_QNAME:
            case XML_XSD10_SCHEMAS_ANYURI:
            case XML_XSD10_SCHEMAS_BASE64BINARY:
            case XML_XSD10_SCHEMAS_HEXBINARY:
                if ((facetType == XML_XSD10_SCHEMA_FACET_LENGTH) ||
                    (facetType == XML_XSD10_SCHEMA_FACET_MINLENGTH) ||
                    (facetType == XML_XSD10_SCHEMA_FACET_MAXLENGTH) ||
                    (facetType == XML_XSD10_SCHEMA_FACET_PATTERN) ||
                    (facetType == XML_XSD10_SCHEMA_FACET_ENUMERATION) ||
                    (facetType == XML_XSD10_SCHEMA_FACET_WHITESPACE))
                    return 1;
                else
                    return 0;
            case XML_XSD10_SCHEMAS_DECIMAL:
                if ((facetType == XML_XSD10_SCHEMA_FACET_TOTALDIGITS) ||
                    (facetType == XML_XSD10_SCHEMA_FACET_FRACTIONDIGITS) ||
                    (facetType == XML_XSD10_SCHEMA_FACET_PATTERN) ||
                    (facetType == XML_XSD10_SCHEMA_FACET_WHITESPACE) ||
                    (facetType == XML_XSD10_SCHEMA_FACET_ENUMERATION) ||
                    (facetType == XML_XSD10_SCHEMA_FACET_MAXINCLUSIVE) ||
                    (facetType == XML_XSD10_SCHEMA_FACET_MAXEXCLUSIVE) ||
                    (facetType == XML_XSD10_SCHEMA_FACET_MININCLUSIVE) ||
                    (facetType == XML_XSD10_SCHEMA_FACET_MINEXCLUSIVE))
                    return 1;
                else
                    return 0;
            case XML_XSD10_SCHEMAS_TIME:
            case XML_XSD10_SCHEMAS_GDAY:
            case XML_XSD10_SCHEMAS_GMONTH:
            case XML_XSD10_SCHEMAS_GMONTHDAY:
            case XML_XSD10_SCHEMAS_GYEAR:
            case XML_XSD10_SCHEMAS_GYEARMONTH:
            case XML_XSD10_SCHEMAS_DATE:
            case XML_XSD10_SCHEMAS_DATETIME:
            case XML_XSD10_SCHEMAS_DURATION:
            case XML_XSD10_SCHEMAS_FLOAT:
            case XML_XSD10_SCHEMAS_DOUBLE:
                if ((facetType == XML_XSD10_SCHEMA_FACET_PATTERN) ||
                    (facetType == XML_XSD10_SCHEMA_FACET_ENUMERATION) ||
                    (facetType == XML_XSD10_SCHEMA_FACET_WHITESPACE) ||
                    (facetType == XML_XSD10_SCHEMA_FACET_MAXINCLUSIVE) ||
                    (facetType == XML_XSD10_SCHEMA_FACET_MAXEXCLUSIVE) ||
                    (facetType == XML_XSD10_SCHEMA_FACET_MININCLUSIVE) ||
                    (facetType == XML_XSD10_SCHEMA_FACET_MINEXCLUSIVE))
                    return 1;
                else
                    return 0;
            default:
                break;
        }
        return 0;
    }

    // xml_xsd10_get_built_in_type
    /** Port of xmlSchemaGetBuiltInType: the type struct for a built-in type by its type id.
        @param type the XML_XSD10_SCHEMAS_ type id @return the type or null */
    public static Xsd10Type getBuiltInType(int type)
    {
        if ((type >= XML_XSD10_SCHEMAS_STRING) && (type <= XML_XSD10_SCHEMAS_ANYSIMPLETYPE))
            return xsd10_types_builtin_[type];
        return null;
    }

    // xml_xsd10_get_predefined_type
    /** Port of xmlSchemaGetPredefinedType: lookup a type in the default XML Schemas type library.
        @param name the type name @param ns the namespace @return the type or null */
    public static Xsd10Type getPredefinedType(String name, String ns)
    {
        if (name == null)
            return null;
        /* libxml2 looks up (name, ns) in a hash table where all types have the xsd namespace. */
        if ((ns == null) || !ns.equals(XSD10_TYPES_NS))
            return null;
        for (int i = XML_XSD10_SCHEMAS_STRING; i <= XML_XSD10_SCHEMAS_ANYSIMPLETYPE; i++)
        {
            if (xsd10_types_builtin_[i].name.equals(name))
                return xsd10_types_builtin_[i];
        }
        return null;
    }

    // xml_xsd10_get_built_in_list_simple_type_item_type
    /** Port of xmlSchemaGetBuiltInListSimpleTypeItemType.
        @param type the list type @return the item type or null */
    public static Xsd10Type getBuiltInListSimpleTypeItemType(Xsd10Type type)
    {
        if ((type == null) || (type.type != XML_XSD10_SCHEMA_TYPE_BASIC))
            return null;
        switch (type.builtInType)
        {
            case XML_XSD10_SCHEMAS_NMTOKENS:
                return ref(XML_XSD10_SCHEMAS_NMTOKEN);
            case XML_XSD10_SCHEMAS_IDREFS:
                return ref(XML_XSD10_SCHEMAS_IDREF);
            case XML_XSD10_SCHEMAS_ENTITIES:
                return ref(XML_XSD10_SCHEMAS_ENTITY);
            default:
                return null;
        }
    }

    // Base types support /////////////////////////////////////////////////////////

    // xsd10_types_new_value
    /* Port of xmlSchemaNewValue: allocate a new simple type value. */
    private static Xsd10Val typesNewValue(int type)
    {
        Xsd10Val value = new Xsd10Val();
        value.type = type;
        return value;
    }

    /** The char at i of an ASCII decimal string, 0 past the end (C terminator). */
    private static int dc(String s, int i)
    {
        if (s == null || i < 0 || i >= s.length()) return 0;
        return s.charAt(i);
    }

    // xsd10_types_decimal_get_fractional_part
    /* Port of xmlSchemaValDecimalGetFractionalPart: the index of the fractional part. */
    private static int typesDecimalGetFractionalPart(Xsd10Val.Decimal decimal)
    {
        /* 2 = sign+dot */
        return 2 + decimal.integralPlaces;
    }

    // xsd10_types_decimal_is_integer
    /* Port of xmlSchemaValDecimalIsInteger. */
    private static boolean typesDecimalIsInteger(Xsd10Val.Decimal decimal)
    {
        return decimal.fractionalPlaces == 1 && dc(decimal.str, typesDecimalGetFractionalPart(decimal)) == '0';
    }

    // xsd10_types_decimal_get_significant_digit_count
    /* Port of xmlSchemaValDecimalGetSignificantDigitCount (unsigned). */
    private static long typesDecimalGetSignificantDigitCount(Xsd10Val.Decimal decimal)
    {
        long fractionalPlaces = typesDecimalIsInteger(decimal) ? 0 : Integer.toUnsignedLong(decimal.fractionalPlaces);
        long integralPlaces = Integer.toUnsignedLong(decimal.integralPlaces);
        if (integralPlaces == 1 && dc(decimal.str, 1) == '0')
        {
            integralPlaces = 0;
        }
        long sum = (integralPlaces + fractionalPlaces) & 0xffffffffL;
        if (sum == 0)
        {
            /* 0, but that's still 1 significant digit */
            return 1;
        }
        return sum;
    }

    /** C strcmp of s1 and s2 from index from. */
    private static int strcmpFrom(String s1, String s2, int from)
    {
        int i = from;
        while (true)
        {
            int a = dc(s1, i), c = dc(s2, i);
            if (a != c) return a - c;
            if (a == 0) return 0;
            i++;
        }
    }

    // xsd10_types_decimal_compare
    /* Port of xmlSchemaValDecimalCompare: compares two decimals, returns a positive value if
       lhs > rhs, negative if lhs < rhs, or 0 if lhs == rhs. */
    private static int typesDecimalCompare(Xsd10Val.Decimal lhs, Xsd10Val.Decimal rhs)
    {
        int sign = 1;
        /* may be +0 and -0 for some reason, handle */
        if (strcmpFrom(lhs.str, "x0.0", 1) == 0 &&
            strcmpFrom(rhs.str, "x0.0", 1) == 0)
        {
            return 0;
        }
        /* first take care of sign */
        if (dc(lhs.str, 0) != dc(rhs.str, 0))
        {
            /* ASCII- > ASCII+ */
            return dc(rhs.str, 0) - dc(lhs.str, 0);
        }
        /* signs are equal, but if negative the comparison must be reversed */
        if (dc(lhs.str, 0) == '-')
        {
            sign = -1;
        }
        /* internal representation never contains leading zeroes, longer decimal representation = larger number */
        if (lhs.integralPlaces != rhs.integralPlaces)
        {
            return (lhs.integralPlaces - rhs.integralPlaces) * sign;
        }
        /* same length, only digits => lexicographical sorting == numerical sorting. +1 to skip the sign */
        return strcmpFrom(lhs.str, rhs.str, 1) * sign;
    }

    // xsd10_types_decimal_compare_with_integer
    /* Port of xmlSchemaValDecimalCompareWithInteger. */
    private static int typesDecimalCompareWithInteger(Xsd10Val.Decimal lhs, long rhs)
    {
        Xsd10Val.Decimal tmpVal = new Xsd10Val.Decimal();
        String buf = (rhs >= 0 ? "+" : "") + rhs + ".0";
        /* 3 = sign+dot+0+NULL */
        tmpVal.integralPlaces = buf.length() - 3;
        tmpVal.str = buf;
        tmpVal.fractionalPlaces = 1;
        return typesDecimalCompare(lhs, tmpVal);
    }

    // xml_xsd10_value_append
    /** Port of xmlSchemaValueAppend: appends a next sibling to a list of computed values.
        @param prev the previous value @param cur the appended value @return 0 or -1 */
    public static int valueAppend(Xsd10Val prev, Xsd10Val cur)
    {
        if ((prev == null) || (cur == null))
            return -1;
        prev.next = cur;
        return 0;
    }

    // xml_xsd10_value_get_next
    /** Port of xmlSchemaValueGetNext.
        @param cur the value @return the next value or null */
    public static Xsd10Val valueGetNext(Xsd10Val cur)
    {
        if (cur == null)
            return null;
        return cur.next;
    }

    // xml_xsd10_value_get_as_string
    /** Port of xmlSchemaValueGetAsString: accessor for the string value of a computed value.
        @param val the value @return the string or null */
    public static String valueGetAsString(Xsd10Val val)
    {
        if (val == null)
            return null;
        switch (val.type)
        {
            case XML_XSD10_SCHEMAS_STRING:
            case XML_XSD10_SCHEMAS_NORMSTRING:
            case XML_XSD10_SCHEMAS_ANYSIMPLETYPE:
            case XML_XSD10_SCHEMAS_TOKEN:
            case XML_XSD10_SCHEMAS_LANGUAGE:
            case XML_XSD10_SCHEMAS_NMTOKEN:
            case XML_XSD10_SCHEMAS_NAME:
            case XML_XSD10_SCHEMAS_NCNAME:
            case XML_XSD10_SCHEMAS_ID:
            case XML_XSD10_SCHEMAS_IDREF:
            case XML_XSD10_SCHEMAS_ENTITY:
            case XML_XSD10_SCHEMAS_ANYURI:
                return val.str;
            default:
                break;
        }
        return null;
    }

    // xml_xsd10_value_get_as_boolean
    /** Port of xmlSchemaValueGetAsBoolean: 1 if true and 0 if false, or in case of an error.
        @param val the value @return the boolean */
    public static int valueGetAsBoolean(Xsd10Val val)
    {
        if ((val == null) || (val.type != XML_XSD10_SCHEMAS_BOOLEAN))
            return 0;
        return val.b;
    }

    // xml_xsd10_new_string_value
    /** Port of xmlSchemaNewStringValue: only XML_XSD10_SCHEMAS_STRING is supported.
        @param type the type @param value the string @return the value or null */
    public static Xsd10Val newStringValue(int type, String value)
    {
        Xsd10Val val;

        if (type != XML_XSD10_SCHEMAS_STRING)
            return null;
        val = new Xsd10Val();
        val.type = type;
        val.str = value;
        return val;
    }

    // xml_xsd10_new_notation_value
    /** Port of xmlSchemaNewNOTATIONValue.
        @param name the local name @param ns the namespace @return the value */
    public static Xsd10Val newNotationValue(String name, String ns)
    {
        Xsd10Val val;

        val = typesNewValue(XML_XSD10_SCHEMAS_NOTATION);
        val.qname.name = name;
        if (ns != null)
            val.qname.uri = ns;
        return val;
    }

    // xml_xsd10_new_qname_value
    /** Port of xmlSchemaNewQNameValue.
        @param namespaceName the namespace @param localName the local name @return the value */
    public static Xsd10Val newQnameValue(String namespaceName, String localName)
    {
        Xsd10Val val;

        val = typesNewValue(XML_XSD10_SCHEMAS_QNAME);
        val.qname.name = localName;
        val.qname.uri = namespaceName;
        return val;
    }

    // xml_xsd10_free_value
    /** Port of xmlSchemaFreeValue (nothing to free in Java).
        @param value the value */
    public static void freeValue(Xsd10Val value)
    {
    }

    // Convenience macros and functions ///////////////////////////////////////////

    private static boolean isTzoChar(int c)
    {
        return (c == 0) || (c == 'Z') || (c == '+') || (c == '-');
    }

    private static boolean validYear(long yr) { return yr != 0; }
    private static boolean validMonth(long mon) { return (mon >= 1) && (mon <= 12); }
    /* VALID_DAY should only be used when month is unknown */
    private static boolean validDay(long day) { return (day >= 1) && (day <= 31); }
    private static boolean validHour(long hr) { return (hr >= 0) && (hr <= 23); }
    private static boolean validMin(long min) { return (min >= 0) && (min <= 59); }
    private static boolean validSec(double sec) { return (sec >= 0) && (sec < 60); }
    private static boolean validTzo(long tzo) { return (tzo >= -840) && (tzo <= 840); }
    private static boolean isLeap(long y)
    {
        return ((y % 4 == 0) && (y % 100 != 0)) || (y % 400 == 0);
    }

    private static final int[] xsd10_types_days_in_month_ =
        { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    private static final int[] xsd10_types_days_in_month_leap_ =
        { 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

    /** Array access that does not throw (C would read outside of the table). */
    private static long tab(int[] t, long i)
    {
        if (i < 0 || i >= t.length) return 0;
        return t[(int) i];
    }

    private static long tab(long[] t, long i)
    {
        if (i < 0 || i >= t.length) return 0;
        return t[(int) i];
    }

    private static long maxDayInMonth(long yr, long mon)
    {
        return isLeap(yr) ? tab(xsd10_types_days_in_month_leap_, mon - 1) : tab(xsd10_types_days_in_month_, mon - 1);
    }

    private static boolean validMday(Xsd10Val.Date dt)
    {
        return isLeap(dt.year) ?
            (dt.day <= tab(xsd10_types_days_in_month_leap_, dt.mon - 1)) :
            (dt.day <= tab(xsd10_types_days_in_month_, dt.mon - 1));
    }

    private static boolean validDate(Xsd10Val.Date dt)
    {
        return validYear(dt.year) && validMonth(dt.mon) && validMday(dt);
    }

    private static boolean validEndOfDay(Xsd10Val.Date dt)
    {
        return dt.hour == 24 && dt.min == 0 && dt.sec == 0;
    }

    private static boolean validTime(Xsd10Val.Date dt)
    {
        return (((dt.hour < 24) && (dt.min < 60) && validSec(dt.sec)) || validEndOfDay(dt)) &&
            validTzo(dt.tzo);
    }

    private static boolean validDatetime(Xsd10Val.Date dt)
    {
        return validDate(dt) && validTime(dt);
    }

    private static final int XSD10_TYPES_SECS_PER_MIN = 60;
    private static final int XSD10_TYPES_MINS_PER_HOUR = 60;
    private static final int XSD10_TYPES_HOURS_PER_DAY = 24;
    private static final int XSD10_TYPES_SECS_PER_HOUR = (XSD10_TYPES_MINS_PER_HOUR * XSD10_TYPES_SECS_PER_MIN);
    private static final int XSD10_TYPES_SECS_PER_DAY = (XSD10_TYPES_HOURS_PER_DAY * XSD10_TYPES_SECS_PER_HOUR);
    private static final int XSD10_TYPES_MINS_PER_DAY = (XSD10_TYPES_HOURS_PER_DAY * XSD10_TYPES_MINS_PER_HOUR);

    private static final long[] xsd10_types_day_in_year_by_month_ =
        { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334 };
    private static final long[] xsd10_types_day_in_leap_year_by_month_ =
        { 0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335 };

    private static long dayInYear(long day, long month, long year)
    {
        return (isLeap(year) ?
                tab(xsd10_types_day_in_leap_year_by_month_, month - 1) :
                tab(xsd10_types_day_in_year_by_month_, month - 1)) + day;
    }

    // xsd10_types_parse_gyear
    /* Port of _xmlSchemaParseGYear: parses a xs:gYear without time zone and fills in the
       appropriate field of the dt structure. str[0] is updated to point just after the
       xs:gYear. Returns 0 or the error code. */
    private static int typesParseGyear(Xsd10Val.Date dt, byte[] s, int[] str)
    {
        int cur = str[0], firstChar;
        int isneg = 0, digcnt = 0;

        if (((at(s, cur) < '0') || (at(s, cur) > '9')) &&
            (at(s, cur) != '-') && (at(s, cur) != '+'))
            return -1;

        if (at(s, cur) == '-')
        {
            isneg = 1;
            cur++;
        }

        firstChar = cur;

        while ((at(s, cur) >= '0') && (at(s, cur) <= '9'))
        {
            int digit = at(s, cur) - '0';

            if (dt.year > Long.MAX_VALUE / 10)
                return 2;
            dt.year *= 10;
            if (dt.year > Long.MAX_VALUE - digit)
                return 2;
            dt.year += digit;
            cur++;
            digcnt++;
        }

        /* year must be at least 4 digits (CCYY); over 4
         * digits cannot have a leading zero. */
        if ((digcnt < 4) || ((digcnt > 4) && (at(s, firstChar) == '0')))
            return 1;

        if (isneg != 0)
            dt.year = -dt.year;

        if (!validYear(dt.year))
            return 2;

        str[0] = cur;
        return 0;
    }

    /* Port of PARSE_2_DIGITS: the 2-digits integer at cur[0] or -1 if invalid; cur[0] is
       advanced by 2 in both cases. */
    private static int parse2Digits(byte[] s, int[] cur)
    {
        int c = cur[0];
        int r;
        if ((at(s, c) < '0') || (at(s, c) > '9') ||
            (at(s, c + 1) < '0') || (at(s, c + 1) > '9'))
            r = -1;
        else
            r = (at(s, c) - '0') * 10 + (at(s, c + 1) - '0');
        cur[0] = c + 2;
        return r;
    }

    // xsd10_types_parse_gmonth
    /* Port of _xmlSchemaParseGMonth: parses a xs:gMonth without time zone. */
    private static int typesParseGmonth(Xsd10Val.Date dt, byte[] s, int[] str)
    {
        int[] cur = { str[0] };
        int value;

        value = parse2Digits(s, cur);
        if (value < 0)
            return 1;

        if (!validMonth(value))
            return 2;

        dt.mon = value;

        str[0] = cur[0];
        return 0;
    }

    // xsd10_types_parse_gday
    /* Port of _xmlSchemaParseGDay: parses a xs:gDay without time zone. */
    private static int typesParseGday(Xsd10Val.Date dt, byte[] s, int[] str)
    {
        int[] cur = { str[0] };
        int value;

        value = parse2Digits(s, cur);
        if (value < 0)
            return 1;

        if (!validDay(value))
            return 2;

        dt.day = value;
        str[0] = cur[0];
        return 0;
    }

    // xsd10_types_parse_time
    /* Port of _xmlSchemaParseTime: parses a xs:time without time zone and fills in the
       appropriate fields of the dt structure. str[0] is updated to point just after the
       xs:time. In case of error, values of dt fields are undefined. */
    private static int typesParseTime(Xsd10Val.Date dt, byte[] s, int[] str)
    {
        int[] cur = { str[0] };
        int value;

        value = parse2Digits(s, cur);
        if (value < 0)
            return 1;
        if (at(s, cur[0]) != ':')
            return 1;
        if (!validHour(value) && value != 24 /* Allow end-of-day hour */)
            return 2;
        cur[0]++;

        /* the ':' insures this string is xs:time */
        dt.hour = value;

        value = parse2Digits(s, cur);
        if (value < 0)
            return 1;
        if (!validMin(value))
            return 2;
        dt.min = value;

        if (at(s, cur[0]) != ':')
            return 1;
        cur[0]++;

        /* PARSE_FLOAT */
        value = parse2Digits(s, cur);
        if (value < 0)
            return 1;
        dt.sec = value;
        if (at(s, cur[0]) == '.')
        {
            double mult = 1;
            cur[0]++;
            if ((at(s, cur[0]) < '0') || (at(s, cur[0]) > '9'))
                return 1;
            while ((at(s, cur[0]) >= '0') && (at(s, cur[0]) <= '9'))
            {
                mult /= 10;
                dt.sec += (at(s, cur[0]) - '0') * mult;
                cur[0]++;
            }
        }

        if (!validTime(dt))
            return 2;

        str[0] = cur[0];
        return 0;
    }

    // xsd10_types_parse_time_zone
    /* Port of _xmlSchemaParseTimeZone: parses a time zone without time zone and fills in the
       appropriate field of the dt structure. str[0] is updated to point just after the time zone. */
    private static int typesParseTimeZone(Xsd10Val.Date dt, byte[] s, int[] str)
    {
        int[] cur;

        if (str == null)
            return -1;
        cur = new int[] { str[0] };

        switch (at(s, cur[0]))
        {
            case 0:
                dt.tz_flag = 0;
                dt.tzo = 0;
                break;

            case 'Z':
                dt.tz_flag = 1;
                dt.tzo = 0;
                cur[0]++;
                break;

            case '+':
            case '-':
            {
                int isneg = 0, tmp = 0;
                isneg = (at(s, cur[0]) == '-') ? 1 : 0;

                cur[0]++;

                tmp = parse2Digits(s, cur);
                if (tmp < 0)
                    return 1;
                if (!validHour(tmp))
                    return 2;

                if (at(s, cur[0]) != ':')
                    return 1;
                cur[0]++;

                dt.tzo = tmp * 60;

                tmp = parse2Digits(s, cur);
                if (tmp < 0)
                    return 1;
                if (!validMin(tmp))
                    return 2;

                dt.tzo += tmp;
                if (isneg != 0)
                    dt.tzo = -dt.tzo;

                if (!validTzo(dt.tzo))
                    return 2;

                dt.tz_flag = 1;
                break;
            }
            default:
                return 1;
        }

        str[0] = cur[0];
        return 0;
    }

    // xsd10_types_base64_decode
    /* Port of _xmlSchemaBase64Decode: converts a base64 encoded character to its base 64 value.
       Returns 0-63 (value), 64 (pad), or -1 (not recognized). */
    private static int typesBase64Decode(int ch)
    {
        if (('A' <= ch) && (ch <= 'Z')) return ch - 'A';
        if (('a' <= ch) && (ch <= 'z')) return ch - 'a' + 26;
        if (('0' <= ch) && (ch <= '9')) return ch - '0' + 52;
        if ('+' == ch) return 62;
        if ('/' == ch) return 63;
        if ('=' == ch) return 64;
        return -1;
    }

    // XML Schema Dates/Times Datatypes Handling //////////////////////////////////

    /* XSD10_TYPES_RETURN_TYPE_IF_VALID: 1 = done (dt.type set), -1 = error, 0 = continue. */
    private static int returnTypeIfValid(Xsd10Val dt, byte[] s, int[] cur, int t)
    {
        if (isTzoChar(at(s, cur[0])))
        {
            int ret = typesParseTimeZone(dt.date, s, cur);
            if (ret == 0)
            {
                if (at(s, cur[0]) != 0)
                    return -1;
                dt.type = t;
                return 1;
            }
        }
        return 0;
    }

    // xsd10_types_validate_dates
    /* Port of xmlSchemaValidateDates: check that dateTime conforms to the lexical space of one
       of the date types. if true a value is computed and returned in val.
       Returns 0 if this validates, a positive error code number otherwise
       and -1 in case of internal or API error. */
    private static int typesValidateDates(int type, byte[] dateTime, Xsd10Val[] val, int collapse)
    {
        Xsd10Val dt;
        int ret;
        int[] cur = { 0 };
        int r;

        if (dateTime == null)
            return -1;

        if (collapse != 0)
            while (isBlankCh(at(dateTime, cur[0]))) cur[0]++;

        if ((at(dateTime, cur[0]) != '-') && (at(dateTime, cur[0]) < '0') && (at(dateTime, cur[0]) > '9'))
            return 1;

        dt = typesNewValue(XML_XSD10_SCHEMAS_UNKNOWN);

        done:
        {
            if ((at(dateTime, cur[0]) == '-') && (at(dateTime, cur[0] + 1) == '-'))
            {
                /*
                 * It's an incomplete date (xs:gMonthDay, xs:gMonth or
                 * xs:gDay)
                 */
                cur[0] += 2;

                /* is it an xs:gDay? */
                if (at(dateTime, cur[0]) == '-')
                {
                    if (type == XML_XSD10_SCHEMAS_GMONTH)
                        return 1;
                    ++cur[0];
                    ret = typesParseGday(dt.date, dateTime, cur);
                    if (ret != 0)
                        return 1;

                    r = returnTypeIfValid(dt, dateTime, cur, XML_XSD10_SCHEMAS_GDAY);
                    if (r > 0) break done;
                    return 1;
                }

                /*
                 * it should be an xs:gMonthDay or xs:gMonth
                 */
                ret = typesParseGmonth(dt.date, dateTime, cur);
                if (ret != 0)
                    return 1;

                /*
                 * a '-' char could indicate this type is xs:gMonthDay or
                 * a negative time zone offset. Check for xs:gMonthDay first.
                 * Also the first three char's of a negative tzo (-MM:SS) can
                 * appear to be a valid day; so even if the day portion
                 * of the xs:gMonthDay verifies, we must insure it was not
                 * a tzo.
                 */
                if (at(dateTime, cur[0]) == '-')
                {
                    int rewnd = cur[0];
                    cur[0]++;

                    ret = typesParseGday(dt.date, dateTime, cur);
                    if ((ret == 0) && ((at(dateTime, cur[0]) == 0) || (at(dateTime, cur[0]) != ':')))
                    {
                        /*
                         * we can use the VALID_MDAY macro to validate the month
                         * and day because the leap year test will flag year zero
                         * as a leap year (even though zero is an invalid year).
                         */
                        if (validMday(dt.date))
                        {
                            r = returnTypeIfValid(dt, dateTime, cur, XML_XSD10_SCHEMAS_GMONTHDAY);
                            if (r > 0) break done;
                            return 1;
                        }
                    }

                    /*
                     * not xs:gMonthDay so rewind and check if just xs:gMonth
                     * with an optional time zone.
                     */
                    cur[0] = rewnd;
                }

                r = returnTypeIfValid(dt, dateTime, cur, XML_XSD10_SCHEMAS_GMONTH);
                if (r > 0) break done;
                return 1;
            }

            /*
             * It's a right-truncated date or an xs:time.
             * Try to parse an xs:time then fallback on right-truncated dates.
             */
            if ((at(dateTime, cur[0]) >= '0') && (at(dateTime, cur[0]) <= '9'))
            {
                ret = typesParseTime(dt.date, dateTime, cur);
                if (ret == 0)
                {
                    /* it's an xs:time */
                    r = returnTypeIfValid(dt, dateTime, cur, XML_XSD10_SCHEMAS_TIME);
                    if (r > 0) break done;
                    if (r < 0) return 1;
                }
            }

            /* fallback on date parsing */
            cur[0] = 0;

            ret = typesParseGyear(dt.date, dateTime, cur);
            if (ret != 0)
                return 1;

            /* is it an xs:gYear? */
            r = returnTypeIfValid(dt, dateTime, cur, XML_XSD10_SCHEMAS_GYEAR);
            if (r > 0) break done;
            if (r < 0) return 1;

            if (at(dateTime, cur[0]) != '-')
                return 1;
            cur[0]++;

            ret = typesParseGmonth(dt.date, dateTime, cur);
            if (ret != 0)
                return 1;

            /* is it an xs:gYearMonth? */
            r = returnTypeIfValid(dt, dateTime, cur, XML_XSD10_SCHEMAS_GYEARMONTH);
            if (r > 0) break done;
            if (r < 0) return 1;

            if (at(dateTime, cur[0]) != '-')
                return 1;
            cur[0]++;

            ret = typesParseGday(dt.date, dateTime, cur);
            if ((ret != 0) || !validDate(dt.date))
                return 1;

            /* is it an xs:date? */
            r = returnTypeIfValid(dt, dateTime, cur, XML_XSD10_SCHEMAS_DATE);
            if (r > 0) break done;
            if (r < 0) return 1;

            if (at(dateTime, cur[0]) != 'T')
                return 1;
            cur[0]++;

            /* it should be an xs:dateTime */
            ret = typesParseTime(dt.date, dateTime, cur);
            if (ret != 0)
                return 1;

            ret = typesParseTimeZone(dt.date, dateTime, cur);
            if (collapse != 0)
                while (isBlankCh(at(dateTime, cur[0]))) cur[0]++;
            if ((ret != 0) || (at(dateTime, cur[0]) != 0) || (!validDatetime(dt.date)))
                return 1;

            dt.type = XML_XSD10_SCHEMAS_DATETIME;
        }

        /* done: */
        if ((type != XML_XSD10_SCHEMAS_UNKNOWN) && (type != dt.type))
            return 1;

        if (val != null)
            val[0] = dt;

        return 0;
    }

    // xsd10_types_validate_duration
    /* Port of xmlSchemaValidateDuration: check that duration conforms to the lexical space of
       the duration type. if true a value is computed and returned in val.
       Returns 0 if this validates, a positive error code number otherwise
       and -1 in case of internal or API error. */
    private static int typesValidateDuration(Xsd10Type type, byte[] duration, Xsd10Val[] val, int collapse)
    {
        int cur = 0;
        Xsd10Val dur;
        int isneg = 0;
        int seq = 0;
        long days, secs = 0;
        double sec_frac = 0.0;
        final int[] desig = { 'Y', 'M', 'D', 'H', 'M', 'S' };

        if (duration == null)
            return -1;

        if (collapse != 0)
            while (isBlankCh(at(duration, cur))) cur++;

        if (at(duration, cur) == '-')
        {
            isneg = 1;
            cur++;
        }

        /* duration must start with 'P' (after sign) */
        if (at(duration, cur++) != 'P')
            return 1;

        if (at(duration, cur) == 0)
            return 1;

        dur = typesNewValue(XML_XSD10_SCHEMAS_DURATION);

        while (at(duration, cur) != 0)
        {
            long num = 0;
            int has_digits = 0;
            int has_frac = 0;

            /* input string should be empty or invalid date/time item */
            if (seq >= desig.length)
                return 1;

            /* T designator must be present for time items */
            if (at(duration, cur) == 'T')
            {
                if (seq > 3)
                    return 1;
                cur++;
                seq = 3;
            }
            else if (seq == 3)
                return 1;

            /* Parse integral part. */
            while (at(duration, cur) >= '0' && at(duration, cur) <= '9')
            {
                long digit = at(duration, cur) - '0';

                if (num > Long.MAX_VALUE / 10)
                    return 1;
                num *= 10;
                if (num > Long.MAX_VALUE - digit)
                    return 1;
                num += digit;

                has_digits = 1;
                cur++;
            }

            if (at(duration, cur) == '.')
            {
                /* Parse fractional part. */
                double mult = 1.0;
                cur++;
                has_frac = 1;
                while (at(duration, cur) >= '0' && at(duration, cur) <= '9')
                {
                    mult /= 10.0;
                    sec_frac += (at(duration, cur) - '0') * mult;
                    has_digits = 1;
                    cur++;
                }
            }

            while (at(duration, cur) != desig[seq])
            {
                seq++;
                /* No T designator or invalid char. */
                if (seq == 3 || seq == desig.length)
                    return 1;
            }
            cur++;

            if (has_digits == 0 || (has_frac != 0 && (seq != 5)))
                return 1;

            switch (seq)
            {
                case 0:
                    /* Year */
                    if (num > Long.MAX_VALUE / 12)
                        return 1;
                    dur.dur.mon = num * 12;
                    break;
                case 1:
                    /* Month */
                    if (dur.dur.mon > Long.MAX_VALUE - num)
                        return 1;
                    dur.dur.mon += num;
                    break;
                case 2:
                    /* Day */
                    dur.dur.day = num;
                    break;
                case 3:
                    /* Hour */
                    days = num / XSD10_TYPES_HOURS_PER_DAY;
                    if (dur.dur.day > Long.MAX_VALUE - days)
                        return 1;
                    dur.dur.day += days;
                    secs = (num % XSD10_TYPES_HOURS_PER_DAY) * XSD10_TYPES_SECS_PER_HOUR;
                    break;
                case 4:
                    /* Minute */
                    days = num / XSD10_TYPES_MINS_PER_DAY;
                    if (dur.dur.day > Long.MAX_VALUE - days)
                        return 1;
                    dur.dur.day += days;
                    secs += (num % XSD10_TYPES_MINS_PER_DAY) * XSD10_TYPES_SECS_PER_MIN;
                    break;
                case 5:
                    /* Second */
                    days = num / XSD10_TYPES_SECS_PER_DAY;
                    if (dur.dur.day > Long.MAX_VALUE - days)
                        return 1;
                    dur.dur.day += days;
                    secs += num % XSD10_TYPES_SECS_PER_DAY;
                    break;
                default:
                    break;
            }

            seq++;
        }

        days = secs / XSD10_TYPES_SECS_PER_DAY;
        if (dur.dur.day > Long.MAX_VALUE - days)
            return 1;
        dur.dur.day += days;
        dur.dur.sec = (secs % XSD10_TYPES_SECS_PER_DAY) + sec_frac;

        if (isneg != 0)
        {
            dur.dur.mon = -dur.dur.mon;
            dur.dur.day = -dur.dur.day;
            dur.dur.sec = -dur.dur.sec;
        }

        if (val != null)
            val[0] = dur;

        return 0;
    }

    // xsd10_types_strip
    /* Port of xmlSchemaStrip: removes the leading and ending spaces of a string,
       returns the new string or null if no change was required. */
    private static byte[] typesStrip(byte[] value)
    {
        int start = 0, end, f;

        if (value == null) return null;
        while ((at(value, start) != 0) && (isBlankCh(at(value, start)))) start++;
        end = start;
        while (at(value, end) != 0) end++;
        f = end;
        end--;
        while ((end > start) && (isBlankCh(at(value, end)))) end--;
        end++;
        if ((start == 0) && (f == end)) return null;
        return cbytes(value, start, end - start);
    }

    // xsd10_types_white_space_replace
    /* Port of xmlSchemaWhiteSpaceReplace: replaces 0xd, 0x9 and 0xa with a space. */
    private static byte[] typesWhiteSpaceReplace(byte[] value)
    {
        int cur = 0;
        byte[] ret;
        int mcur;

        if (value == null)
            return null;

        while ((at(value, cur) != 0) &&
            ((at(value, cur) != 0xd) && (at(value, cur) != 0x9) && (at(value, cur) != 0xa)))
        {
            cur++;
        }
        if (at(value, cur) == 0)
            return null;
        ret = cbytes(value, 0, clen(value, 0));
        mcur = cur;
        do
        {
            int c = at(ret, mcur);
            if ((c == 0xd) || (c == 0x9) || (c == 0xa))
                ret[mcur] = ' ';
            mcur++;
        } while (at(ret, mcur) != 0);
        return ret;
    }

    // xml_xsd10_white_space_replace
    /** Port of xmlSchemaWhiteSpaceReplace.
        @param value the string @return the replaced string or null if no change was required */
    public static String whiteSpaceReplace(String value)
    {
        return str(typesWhiteSpaceReplace(b(value)), 0);
    }

    // xsd10_types_collapse_string
    /* Port of xmlSchemaCollapseString: removes and normalize white spaces in the string,
       returns the new string or null if no change was required. */
    private static byte[] typesCollapseString(byte[] value)
    {
        int start = 0, end, f;
        int g;
        long col = 0;

        if (value == null) return null;
        while ((at(value, start) != 0) && (isBlankCh(at(value, start)))) start++;
        end = start;
        while (at(value, end) != 0)
        {
            if ((at(value, end) == ' ') && (isBlankCh(at(value, end + 1))))
            {
                col = end - start;
                break;
            }
            else if ((at(value, end) == 0xa) || (at(value, end) == 0x9) || (at(value, end) == 0xd))
            {
                col = end - start;
                break;
            }
            end++;
        }
        if (col == 0)
        {
            f = end;
            end--;
            while ((end > start) && (isBlankCh(at(value, end)))) end--;
            end++;
            if ((start == 0) && (f == end)) return null;
            return cbytes(value, start, end - start);
        }
        byte[] s = cbytes(value, start, clen(value, start));
        g = (int) col;
        end = g;
        while (at(s, end) != 0)
        {
            if (isBlankCh(at(s, end)))
            {
                end++;
                while (isBlankCh(at(s, end))) end++;
                if (at(s, end) != 0)
                    s[g++] = ' ';
            }
            else
                s[g++] = s[end++];
        }
        s[g] = 0;
        return s;
    }

    // xml_xsd10_collapse_string
    /** Port of xmlSchemaCollapseString.
        @param value the string @return the collapsed string or null if no change was required */
    public static String collapseString(String value)
    {
        return str(typesCollapseString(b(value)), 0);
    }

    // xsd10_types_val_atomic_list_node
    /* Port of xmlSchemaValAtomicListNode: check that a value conforms to the lexical space of the
       predefined list type. if true a value is computed and returned in ret.
       Returns the number of items if this validates, a negative error code number otherwise. */
    private static int typesValAtomicListNode(Xsd10Type type, byte[] value, Xsd10Val[] ret, XmlNode node, Xsd10Ids ids)
    {
        byte[] val;
        int cur, endval;
        int nb_values = 0;
        int tmp = 0;

        if (value == null)
        {
            return -1;
        }
        val = cbytes(value, 0, clen(value, 0));
        if (ret != null)
        {
            ret[0] = null;
        }
        cur = 0;
        /*
         * Split the list
         */
        while (isBlankCh(at(val, cur))) val[cur++] = 0;
        while (at(val, cur) != 0)
        {
            if (isBlankCh(at(val, cur)))
            {
                val[cur] = 0;
                cur++;
                while (isBlankCh(at(val, cur))) val[cur++] = 0;
            }
            else
            {
                nb_values++;
                cur++;
                while ((at(val, cur) != 0) && (!isBlankCh(at(val, cur)))) cur++;
            }
        }
        if (nb_values == 0)
        {
            return nb_values;
        }
        endval = cur;
        cur = 0;
        while ((at(val, cur) == 0) && (cur != endval)) cur++;
        while (cur != endval)
        {
            /* xmlSchemaValPredefTypeNode */
            tmp = typesValAtomicType(type, cbytes(val, cur, clen(val, cur)), null, node, 0,
                XML_XSD10_SCHEMA_WHITESPACE_UNKNOWN, 1, 1, 0, ids);
            if (tmp != 0)
                break;
            while (at(val, cur) != 0) cur++;
            while ((at(val, cur) == 0) && (cur != endval)) cur++;
        }
        /* TODO what return value ? c.f. bug #158628 */
        if (tmp == 0)
            return nb_values;
        return -1;
    }

    // xsd10_types_parse_uint
    /* Port of xmlSchemaParseUInt: parse an unsigned long into a decimal.
       Returns the number of significant digits in the number or
       -1 if overflow of the capacity and -2 if it's not a number. */
    private static int typesParseUint(byte[] s, int[] str, Xsd10Val.Decimal val)
    {
        int tmp, cur = str[0];
        int ret = 0, i = 0;

        if (!((at(s, cur) >= '0') && (at(s, cur) <= '9')))
            return -2;

        while (at(s, cur) == '0')
        {        /* ignore leading zeroes */
            cur++;
        }
        /* back up in case there is nothing after the leading zeroes */
        if (!(at(s, cur) >= '0' && at(s, cur) <= '9'))
        {
            --cur;
        }
        tmp = cur;
        while ((at(s, tmp) != 0) && (at(s, tmp) >= '0') && (at(s, tmp) <= '9'))
        {
            i++; tmp++; ret++;
        }
        val.fractionalPlaces = 1;
        val.integralPlaces = i;
        val.str = "+" + new String(s, cur, i, StandardCharsets.ISO_8859_1) + ".0";

        str[0] = tmp;
        return ret;
    }

    // xsd10_types_check_language_type
    /* Port of xmlSchemaCheckLanguageType: check that a value conforms to the lexical space of the
       language datatype. Must conform to [a-zA-Z]{1,8}(-[a-zA-Z0-9]{1,8})*
       Returns 1 if this validates, 0 otherwise. */
    private static int typesCheckLanguageType(byte[] value)
    {
        int first = 1, len = 0;
        int cur = 0;

        if (value == null)
            return 0;

        while (at(value, cur) != 0)
        {
            int c = at(value, cur);
            if (!(((c >= 'a') && (c <= 'z')) || ((c >= 'A') && (c <= 'Z'))
                || (c == '-')
                || ((first == 0) && ((c >= '0') && (c <= '9')))))
                return 0;
            if (c == '-')
            {
                if ((len < 1) || (len > 8))
                    return 0;
                len = 0;
                first = 0;
            }
            else
                len++;
            cur++;
        }
        if ((len < 1) || (len > 8))
            return 0;

        return 1;
    }

    // xsd10_types_is_unparsed_entity
    /* Replaces libxml2 xmlGetDocEntity + etype check: is name an unparsed entity declared in the
       dtd of the document of node? */
    private static int typesIsUnparsedEntity(XmlNode node, byte[] name)
    {
        if (nodeDom(node) == null)
            return 0;
        XmlNode ent = domGetEntity(nodeDom(node), str(name, 0));
        if (!(ent instanceof XmlEntityDecl))
            return 0;
        return (((XmlEntityDecl) ent).etype() == XmlEntityDecl.EXTERNAL_GENERAL_UNPARSED) ? 1 : 0;
    }

    // xsd10_types_has_notation
    /* Replaces libxml2 xmlGetDtdNotationDesc(doc->intSubset, name). */
    private static int typesHasNotation(XmlNode node, byte[] name)
    {
        if (nodeDom(node) == null)
            return 0;
        XmlNode dtd = domDtd(nodeDom(node));

        if (!(dtd instanceof org.libxmq.imp.xml.XmlDtd))
            return 0;
        String s = str(name, 0);
        for (org.libxmq.imp.xml.XmlDtdDecl n : ((org.libxmq.imp.xml.XmlDtd) dtd).notations())
        {
            String nn = nodeName(n);
            if ((nn != null) && nn.equals(s))
                return 1;
        }
        return 0;
    }

    /** C strtod/strtof prefix of a validated float lexical value: the longest prefix that is a
        number (leading white space skipped, an exponent only with digits). */
    private static String cFloatPrefix(byte[] s)
    {
        int p = 0;
        while (at(s, p) == ' ' || (at(s, p) >= 0x9 && at(s, p) <= 0xd)) p++;
        int start = p;
        if (at(s, p) == '+' || at(s, p) == '-') p++;
        int digits = 0;
        while (isDigit(at(s, p))) { p++; digits++; }
        if (at(s, p) == '.')
        {
            p++;
            while (isDigit(at(s, p))) { p++; digits++; }
        }
        if (digits == 0) return null;
        int end = p;
        if (at(s, p) == 'e' || at(s, p) == 'E')
        {
            int q = p + 1;
            if (at(s, q) == '+' || at(s, q) == '-') q++;
            if (isDigit(at(s, q)))
            {
                while (isDigit(at(s, q))) q++;
                end = q;
            }
        }
        return new String(s, start, end - start, StandardCharsets.ISO_8859_1);
    }

    // xsd10_types_val_atomic_type
    /* Port of xmlSchemaValAtomicType: check that a value conforms to the lexical space of the
       atomic type. if true a value is computed and returned in val.
       This checks the value space for list types as well (IDREFS, NMTOKENS).
       ids replaces the ID table of the libxml2 document (may be null).
       Returns 0 if this validates, a positive error code number otherwise
       and -1 in case of internal or API error. */
    private static int typesValAtomicType(Xsd10Type type, byte[] value, Xsd10Val[] val, XmlNode node, int flags,
                                          int ws, int normOnTheFly, int applyNorm, int createStringValue,
                                          Xsd10Ids ids)
    {
        Xsd10Val v;
        byte[] norm = null;
        int ret = 0;

        if (type == null)
            return -1;

        /*
         * validating a non existent text node is similar to validating
         * an empty one.
         */
        if (value == null)
            value = new byte[1];

        if (val != null)
            val[0] = null;
        if (flags == 0)
        {
            if ((type.builtInType != XML_XSD10_SCHEMAS_STRING) &&
                (type.builtInType != XML_XSD10_SCHEMAS_ANYTYPE) &&
                (type.builtInType != XML_XSD10_SCHEMAS_ANYSIMPLETYPE))
            {
                if (type.builtInType == XML_XSD10_SCHEMAS_NORMSTRING)
                    norm = typesWhiteSpaceReplace(value);
                else
                    norm = typesCollapseString(value);
                if (norm != null)
                    value = norm;
            }
        }

        switch (type.builtInType)
        {
            case XML_XSD10_SCHEMAS_UNKNOWN:
                return -1;
            case XML_XSD10_SCHEMAS_ANYTYPE:
            case XML_XSD10_SCHEMAS_ANYSIMPLETYPE:
                if ((createStringValue != 0) && (val != null))
                {
                    v = typesNewValue(XML_XSD10_SCHEMAS_ANYSIMPLETYPE);
                    v.str = str(value, 0);
                    val[0] = v;
                }
                return 0;
            case XML_XSD10_SCHEMAS_STRING:
                if (normOnTheFly == 0)
                {
                    int cur = 0;

                    if (ws == XML_XSD10_SCHEMA_WHITESPACE_REPLACE)
                    {
                        while (at(value, cur) != 0)
                        {
                            int c = at(value, cur);
                            if ((c == 0xd) || (c == 0xa) || (c == 0x9))
                                return 1;
                            else
                                cur++;
                        }
                    }
                    else if (ws == XML_XSD10_SCHEMA_WHITESPACE_COLLAPSE)
                    {
                        while (at(value, cur) != 0)
                        {
                            int c = at(value, cur);
                            if ((c == 0xd) || (c == 0xa) || (c == 0x9))
                            {
                                return 1;
                            }
                            else if (isWspSpaceCh(c))
                            {
                                cur++;
                                if (isWspSpaceCh(at(value, cur)))
                                    return 1;
                            }
                            else
                            {
                                cur++;
                            }
                        }
                    }
                }
                if ((createStringValue != 0) && (val != null))
                {
                    if (applyNorm != 0)
                    {
                        if (ws == XML_XSD10_SCHEMA_WHITESPACE_COLLAPSE)
                            norm = typesCollapseString(value);
                        else if (ws == XML_XSD10_SCHEMA_WHITESPACE_REPLACE)
                            norm = typesWhiteSpaceReplace(value);
                        if (norm != null)
                            value = norm;
                    }
                    v = typesNewValue(XML_XSD10_SCHEMAS_STRING);
                    v.str = str(value, 0);
                    val[0] = v;
                }
                return 0;
            case XML_XSD10_SCHEMAS_NORMSTRING:
            {
                if (normOnTheFly != 0)
                {
                    if (applyNorm != 0)
                    {
                        byte[] prev = norm;
                        if (ws == XML_XSD10_SCHEMA_WHITESPACE_COLLAPSE)
                            norm = typesCollapseString(value);
                        else
                            norm = typesWhiteSpaceReplace(value);
                        if (norm != null)
                            value = norm;
                        else
                            norm = prev;
                    }
                }
                else
                {
                    int cur = 0;
                    while (at(value, cur) != 0)
                    {
                        int c = at(value, cur);
                        if ((c == 0xd) || (c == 0xa) || (c == 0x9))
                            return 1;
                        else
                            cur++;
                    }
                }
                if (val != null)
                {
                    v = typesNewValue(XML_XSD10_SCHEMAS_NORMSTRING);
                    v.str = str(value, 0);
                    val[0] = v;
                }
                return 0;
            }
            case XML_XSD10_SCHEMAS_DECIMAL:
            {
                int cur = 0;
                int numStart, numEnd;
                int integralPlaces = 0, fractionalPlaces = 0;
                char sign;

                if (at(value, cur) == 0)
                    return 1;

                /*
                 * xs:decimal has a whitespace-facet value of 'collapse'.
                 */
                if (normOnTheFly != 0)
                    while (isBlankCh(at(value, cur))) cur++;

                /*
                 * First we handle an optional sign.
                 */
                sign = '+';
                if (at(value, cur) == '-')
                {
                    sign = '-';
                    cur++;
                }
                else if (at(value, cur) == '+')
                    cur++;
                /*
                 * Disallow: "", "-", "- "
                 */
                if (at(value, cur) == 0)
                    return 1;

                /*
                 * Skip leading zeroes.
                 */
                while (at(value, cur) == '0')
                {
                    cur++;
                }

                numStart = cur;

                while ((at(value, cur) >= '0') && (at(value, cur) <= '9'))
                {
                    ++cur;
                    ++integralPlaces;
                }
                if (at(value, cur) == '.')
                {
                    ++cur;
                }
                while ((at(value, cur) >= '0') && (at(value, cur) <= '9'))
                {
                    ++cur;
                    ++fractionalPlaces;
                }

                /*  disallow "." */
                if (fractionalPlaces == 0 && integralPlaces == 0
                    && (numStart == 0 || at(value, numStart - 1) != '0'))
                {
                    return 1;
                }

                numEnd = cur;

                /*  find if there are trailing FRACTIONAL zeroes, and deal with them if necessary */
                while (numEnd > numStart && fractionalPlaces != 0 && at(value, numEnd - 1) == '0')
                {
                    --numEnd;
                    --fractionalPlaces;
                }

                if (normOnTheFly != 0)
                    while (isBlankCh(at(value, cur))) cur++;
                if (at(value, cur) != 0)
                    return 1; /* error if any extraneous chars */
                if (val != null)
                {
                    v = typesNewValue(XML_XSD10_SCHEMAS_DECIMAL);
                    /*  create a standardized representation */
                    String integral = new String(value, numStart, integralPlaces, StandardCharsets.ISO_8859_1);
                    String fractional = new String(value, numEnd - fractionalPlaces, fractionalPlaces, StandardCharsets.ISO_8859_1);
                    if (integralPlaces == 0)
                    {
                        integral = "0";
                        integralPlaces = 1;
                    }
                    if (fractionalPlaces == 0)
                    {
                        fractional = "0";
                        fractionalPlaces = 1;
                    }
                    v.decimal.str = sign + integral + "." + fractional;
                    v.decimal.integralPlaces = integralPlaces;
                    v.decimal.fractionalPlaces = fractionalPlaces;
                    val[0] = v;
                }
                return 0;
            }
            case XML_XSD10_SCHEMAS_TIME:
            case XML_XSD10_SCHEMAS_GDAY:
            case XML_XSD10_SCHEMAS_GMONTH:
            case XML_XSD10_SCHEMAS_GMONTHDAY:
            case XML_XSD10_SCHEMAS_GYEAR:
            case XML_XSD10_SCHEMAS_GYEARMONTH:
            case XML_XSD10_SCHEMAS_DATE:
            case XML_XSD10_SCHEMAS_DATETIME:
                ret = typesValidateDates(type.builtInType, value, val, normOnTheFly);
                break;
            case XML_XSD10_SCHEMAS_DURATION:
                ret = typesValidateDuration(type, value, val, normOnTheFly);
                break;
            case XML_XSD10_SCHEMAS_FLOAT:
            case XML_XSD10_SCHEMAS_DOUBLE:
            {
                int cur = 0;
                int neg = 0;
                int digits_before = 0;
                int digits_after = 0;

                if (normOnTheFly != 0)
                    while (isBlankCh(at(value, cur))) cur++;

                if ((at(value, cur) == 'N') && (at(value, cur + 1) == 'a') && (at(value, cur + 2) == 'N'))
                {
                    cur += 3;
                    if (at(value, cur) != 0)
                        return 1;
                    if (val != null)
                    {
                        if (type == floatDef())
                        {
                            v = typesNewValue(XML_XSD10_SCHEMAS_FLOAT);
                            v.f = Float.NaN;
                        }
                        else
                        {
                            v = typesNewValue(XML_XSD10_SCHEMAS_DOUBLE);
                            v.d = Double.NaN;
                        }
                        val[0] = v;
                    }
                    return 0;
                }
                if (at(value, cur) == '-')
                {
                    neg = 1;
                    cur++;
                }
                if ((at(value, cur) == 'I') && (at(value, cur + 1) == 'N') && (at(value, cur + 2) == 'F'))
                {
                    cur += 3;
                    if (at(value, cur) != 0)
                        return 1;
                    if (val != null)
                    {
                        if (type == floatDef())
                        {
                            v = typesNewValue(XML_XSD10_SCHEMAS_FLOAT);
                            if (neg != 0)
                                v.f = Float.NEGATIVE_INFINITY;
                            else
                                v.f = Float.POSITIVE_INFINITY;
                        }
                        else
                        {
                            v = typesNewValue(XML_XSD10_SCHEMAS_DOUBLE);
                            if (neg != 0)
                                v.d = Double.NEGATIVE_INFINITY;
                            else
                                v.d = Double.POSITIVE_INFINITY;
                        }
                        val[0] = v;
                    }
                    return 0;
                }
                if ((neg == 0) && (at(value, cur) == '+'))
                    cur++;
                if ((at(value, cur) == 0) || (at(value, cur) == '+') || (at(value, cur) == '-'))
                    return 1;
                while ((at(value, cur) >= '0') && (at(value, cur) <= '9'))
                {
                    cur++;
                    digits_before++;
                }
                if (at(value, cur) == '.')
                {
                    cur++;
                    while ((at(value, cur) >= '0') && (at(value, cur) <= '9'))
                    {
                        cur++;
                        digits_after++;
                    }
                }
                if ((digits_before == 0) && (digits_after == 0))
                    return 1;
                if ((at(value, cur) == 'e') || (at(value, cur) == 'E'))
                {
                    cur++;
                    if ((at(value, cur) == '-') || (at(value, cur) == '+'))
                        cur++;
                    while ((at(value, cur) >= '0') && (at(value, cur) <= '9'))
                        cur++;
                }
                if (normOnTheFly != 0)
                    while (isBlankCh(at(value, cur))) cur++;

                if (at(value, cur) != 0)
                    return 1;
                if (val != null)
                {
                    /* sscanf "%f" / "%lf" (strtof / strtod) */
                    String num = cFloatPrefix(value);
                    if (type == floatDef())
                    {
                        v = typesNewValue(XML_XSD10_SCHEMAS_FLOAT);
                        if (num == null)
                            return 1;
                        try
                        {
                            v.f = Float.parseFloat(num);
                        }
                        catch (NumberFormatException e)
                        {
                            return 1;
                        }
                        val[0] = v;
                    }
                    else
                    {
                        v = typesNewValue(XML_XSD10_SCHEMAS_DOUBLE);
                        if (num == null)
                            return 1;
                        try
                        {
                            v.d = Double.parseDouble(num);
                        }
                        catch (NumberFormatException e)
                        {
                            return 1;
                        }
                        val[0] = v;
                    }
                }
                return 0;
            }
            case XML_XSD10_SCHEMAS_BOOLEAN:
            {
                int cur = 0;

                if (normOnTheFly != 0)
                {
                    while (isBlankCh(at(value, cur))) cur++;
                    if (at(value, cur) == '0')
                    {
                        ret = 0;
                        cur++;
                    }
                    else if (at(value, cur) == '1')
                    {
                        ret = 1;
                        cur++;
                    }
                    else if (at(value, cur) == 't')
                    {
                        cur++;
                        if ((at(value, cur++) == 'r') && (at(value, cur++) == 'u') &&
                            (at(value, cur++) == 'e'))
                        {
                            ret = 1;
                        }
                        else
                            return 1;
                    }
                    else if (at(value, cur) == 'f')
                    {
                        cur++;
                        if ((at(value, cur++) == 'a') && (at(value, cur++) == 'l') &&
                            (at(value, cur++) == 's') && (at(value, cur++) == 'e'))
                        {
                            ret = 0;
                        }
                        else
                            return 1;
                    }
                    else
                        return 1;
                    if (at(value, cur) != 0)
                    {
                        while (isBlankCh(at(value, cur))) cur++;
                        if (at(value, cur) != 0)
                            return 1;
                    }
                }
                else
                {
                    if ((at(value, 0) == '0') && (at(value, 1) == 0))
                        ret = 0;
                    else if ((at(value, 0) == '1') && (at(value, 1) == 0))
                        ret = 1;
                    else if ((at(value, 0) == 't') && (at(value, 1) == 'r')
                        && (at(value, 2) == 'u') && (at(value, 3) == 'e')
                        && (at(value, 4) == 0))
                        ret = 1;
                    else if ((at(value, 0) == 'f') && (at(value, 1) == 'a')
                        && (at(value, 2) == 'l') && (at(value, 3) == 's')
                        && (at(value, 4) == 'e') && (at(value, 5) == 0))
                        ret = 0;
                    else
                        return 1;
                }
                if (val != null)
                {
                    v = typesNewValue(XML_XSD10_SCHEMAS_BOOLEAN);
                    v.b = ret;
                    val[0] = v;
                }
                return 0;
            }
            case XML_XSD10_SCHEMAS_TOKEN:
            {
                int cur = 0;

                if (normOnTheFly == 0)
                {
                    while (at(value, cur) != 0)
                    {
                        int c = at(value, cur);
                        if ((c == 0xd) || (c == 0xa) || (c == 0x9))
                        {
                            return 1;
                        }
                        else if (c == ' ')
                        {
                            cur++;
                            if (at(value, cur) == 0)
                                return 1;
                            if (at(value, cur) == ' ')
                                return 1;
                        }
                        else
                        {
                            cur++;
                        }
                    }
                }
                if (val != null)
                {
                    v = typesNewValue(XML_XSD10_SCHEMAS_TOKEN);
                    v.str = str(value, 0);
                    val[0] = v;
                }
                return 0;
            }
            case XML_XSD10_SCHEMAS_LANGUAGE:
                if ((norm == null) && (normOnTheFly != 0))
                {
                    norm = typesCollapseString(value);
                    if (norm != null)
                        value = norm;
                }

                if (typesCheckLanguageType(value) == 1)
                {
                    if (val != null)
                    {
                        v = typesNewValue(XML_XSD10_SCHEMAS_LANGUAGE);
                        v.str = str(value, 0);
                        val[0] = v;
                    }
                    return 0;
                }
                return 1;
            case XML_XSD10_SCHEMAS_NMTOKEN:
                if (typesValidateNmtoken(value, 1) == 0)
                {
                    if (val != null)
                    {
                        v = typesNewValue(XML_XSD10_SCHEMAS_NMTOKEN);
                        v.str = str(value, 0);
                        val[0] = v;
                    }
                    return 0;
                }
                return 1;
            case XML_XSD10_SCHEMAS_NMTOKENS:
                ret = typesValAtomicListNode(ref(XML_XSD10_SCHEMAS_NMTOKEN),
                                             value, val, node, ids);
                if (ret > 0)
                    ret = 0;
                else
                    ret = 1;
                break;
            case XML_XSD10_SCHEMAS_NAME:
                ret = typesValidateName(value, 1);
                if ((ret == 0) && (val != null))
                {
                    v = typesNewValue(XML_XSD10_SCHEMAS_NAME);
                    int start = 0, end;
                    while (isBlankCh(at(value, start))) start++;
                    end = start;
                    while ((at(value, end) != 0) && (!isBlankCh(at(value, end)))) end++;
                    v.str = strn(value, start, end - start);
                    val[0] = v;
                }
                break;
            case XML_XSD10_SCHEMAS_QNAME:
            {
                String uri = null;
                String local = null;

                ret = typesValidateQname(value, 1);
                if (ret != 0)
                    break;
                if (node != null)
                {
                    String[] prefix = new String[1];
                    int[] found = new int[1];

                    local = typesSplitQname2(value, prefix);
                    uri = typesSearchNs(node, prefix[0], found);
                    if ((found[0] == 0) && (prefix[0] != null))
                    {
                        return 1;
                    }
                }
                if (val != null)
                {
                    v = typesNewValue(XML_XSD10_SCHEMAS_QNAME);
                    if (local != null)
                        v.qname.name = local;
                    else
                        v.qname.name = str(value, 0);
                    if (uri != null)
                        v.qname.uri = uri;
                    val[0] = v;
                }
                break;
            }
            case XML_XSD10_SCHEMAS_NCNAME:
                ret = typesValidateNcname(value, 1);
                if ((ret == 0) && (val != null))
                {
                    v = typesNewValue(XML_XSD10_SCHEMAS_NCNAME);
                    v.str = str(value, 0);
                    val[0] = v;
                }
                break;
            case XML_XSD10_SCHEMAS_ID:
                ret = typesValidateNcname(value, 1);
                if ((ret == 0) && (val != null))
                {
                    v = typesNewValue(XML_XSD10_SCHEMAS_ID);
                    v.str = str(value, 0);
                    val[0] = v;
                }
                if ((ret == 0) && (node != null) && (ids != null) &&
                    (nodeKind(node) == ATTRIBUTE))
                {
                    /*
                     * NOTE: the IDness might have already be declared in the DTD
                     */
                    if (idsAttrType(ids, node) != XML_XSD10_ATTRIBUTE_ID)
                    {
                        byte[] strip;
                        int res;

                        strip = typesStrip(value);
                        if (strip != null)
                        {
                            res = idsAddId(ids, node, str(strip, 0));
                        }
                        else
                            res = idsAddId(ids, node, str(value, 0));
                        if (res < 0)
                        {
                            return -1;
                        }
                        else if (res == 0)
                        {
                            ret = 2;
                        }
                    }
                }
                break;
            case XML_XSD10_SCHEMAS_IDREF:
                ret = typesValidateNcname(value, 1);
                if ((ret == 0) && (val != null))
                {
                    v = typesNewValue(XML_XSD10_SCHEMAS_IDREF);
                    v.str = str(value, 0);
                    val[0] = v;
                }
                if ((ret == 0) && (node != null) && (ids != null) &&
                    (nodeKind(node) == ATTRIBUTE))
                {
                    byte[] strip;

                    strip = typesStrip(value);
                    if (strip != null)
                    {
                        idsAddRef(ids, node, str(strip, 0));
                    }
                    else
                        idsAddRef(ids, node, str(value, 0));
                    idsSetAttrType(ids, node, XML_XSD10_ATTRIBUTE_IDREF);
                }
                break;
            case XML_XSD10_SCHEMAS_IDREFS:
                ret = typesValAtomicListNode(ref(XML_XSD10_SCHEMAS_IDREF),
                                             value, val, node, ids);
                if (ret < 0)
                    ret = 2;
                else
                    ret = 0;
                if ((ret == 0) && (node != null) &&
                    (nodeKind(node) == ATTRIBUTE))
                {
                    idsSetAttrType(ids, node, XML_XSD10_ATTRIBUTE_IDREFS);
                }
                break;
            case XML_XSD10_SCHEMAS_ENTITY:
            {
                byte[] strip;

                ret = typesValidateNcname(value, 1);
                if (node == null)
                    ret = 3;
                if (ret == 0)
                {
                    int ent;

                    strip = typesStrip(value);
                    if (strip != null)
                    {
                        ent = typesIsUnparsedEntity(node, strip);
                    }
                    else
                    {
                        ent = typesIsUnparsedEntity(node, value);
                    }
                    if (ent == 0)
                        ret = 4;
                }
                if ((ret == 0) && (val != null))
                {
                    /* TODO */
                }
                if ((ret == 0) && (node != null) &&
                    (nodeKind(node) == ATTRIBUTE))
                {
                    idsSetAttrType(ids, node, XML_XSD10_ATTRIBUTE_ENTITY);
                }
                break;
            }
            case XML_XSD10_SCHEMAS_ENTITIES:
                if (node == null)
                    return 3;
                ret = typesValAtomicListNode(ref(XML_XSD10_SCHEMAS_ENTITY),
                                             value, val, node, ids);
                if (ret <= 0)
                    ret = 1;
                else
                    ret = 0;
                if ((ret == 0) && (node != null) &&
                    (nodeKind(node) == ATTRIBUTE))
                {
                    idsSetAttrType(ids, node, XML_XSD10_ATTRIBUTE_ENTITIES);
                }
                break;
            case XML_XSD10_SCHEMAS_NOTATION:
            {
                String uri = null;
                String local = null;

                ret = typesValidateQname(value, 1);
                if ((ret == 0) && (node != null))
                {
                    String[] prefix = new String[1];

                    local = typesSplitQname2(value, prefix);
                    if (prefix[0] != null)
                    {
                        String href;
                        int[] found = new int[1];

                        href = typesSearchNs(node, prefix[0], found);
                        if (found[0] == 0)
                            ret = 1;
                        else if (val != null)
                            uri = href;
                    }
                    if ((local != null) && ((val == null) || (ret != 0)))
                    {
                        local = null;
                    }
                }
                if (node == null)
                    ret = 3;
                if (ret == 0)
                {
                    if (typesHasNotation(node, value) != 0)
                        ret = 0;
                    else
                        ret = 1;
                }
                if ((ret == 0) && (val != null))
                {
                    v = typesNewValue(XML_XSD10_SCHEMAS_NOTATION);
                    if (local != null)
                        v.qname.name = local;
                    else
                        v.qname.name = str(value, 0);
                    if (uri != null)
                        v.qname.uri = uri;

                    val[0] = v;
                }
                break;
            }
            case XML_XSD10_SCHEMAS_ANYURI:
            {
                if (at(value, 0) != 0)
                {
                    XmlUri uri;
                    byte[] tmpval;
                    if ((norm == null) && (normOnTheFly != 0))
                    {
                        norm = typesCollapseString(value);
                        if (norm != null)
                            value = norm;
                    }
                    tmpval = cbytes(value, 0, clen(value, 0));
                    for (int cur = 0; at(tmpval, cur) != 0; ++cur)
                    {
                        int c = at(tmpval, cur);
                        if (c < 32 || c >= 127 || c == ' ' ||
                            c == '<' || c == '>' || c == '"' ||
                            c == '{' || c == '}' || c == '|' ||
                            c == '\\' || c == '^' || c == '`' ||
                            c == '\'')
                            tmpval[cur] = '_';
                    }
                    uri = XmlUri.parse(str(tmpval, 0));
                    if (uri == null)
                        return 1;
                }

                if (val != null)
                {
                    v = typesNewValue(XML_XSD10_SCHEMAS_ANYURI);
                    v.str = str(value, 0);
                    val[0] = v;
                }
                return 0;
            }
            case XML_XSD10_SCHEMAS_HEXBINARY:
            {
                int cur = 0, start;
                int total, i = 0;

                if (normOnTheFly != 0)
                    while (isBlankCh(at(value, cur))) cur++;

                start = cur;
                while (((at(value, cur) >= '0') && (at(value, cur) <= '9')) ||
                       ((at(value, cur) >= 'A') && (at(value, cur) <= 'F')) ||
                       ((at(value, cur) >= 'a') && (at(value, cur) <= 'f')))
                {
                    i++;
                    cur++;
                }
                if (normOnTheFly != 0)
                    while (isBlankCh(at(value, cur))) cur++;

                if (at(value, cur) != 0)
                    return 1;
                if ((i % 2) != 0)
                    return 1;

                if (val != null)
                {
                    v = typesNewValue(XML_XSD10_SCHEMAS_HEXBINARY);
                    /*
                     * Copy only the normalized piece.
                     * CRITICAL TODO: Check this.
                     */
                    byte[] h = cbytes(value, start, i);

                    total = i / 2;      /* number of octets */

                    for (int k = 0; k < i; k++)
                    {
                        if ((h[k] & 0xff) >= 'a')
                            h[k] = (byte) ((h[k] & 0xff) - ('a' - 'A'));
                    }

                    v.hex.str = str(h, 0);
                    v.hex.total = total;
                    val[0] = v;
                }
                return 0;
            }
            case XML_XSD10_SCHEMAS_BASE64BINARY:
            {
                /* ISSUE:
                 *
                 * Ignore all stray characters? (yes, currently)
                 * Worry about long lines? (no, currently)
                 */
                int cur = 0;
                int total, i = 0, pad = 0;

                for (; at(value, cur) != 0; ++cur)
                {
                    int decc;

                    decc = typesBase64Decode(at(value, cur));
                    if (decc < 0)
                    {
                        /* ignored */
                    }
                    else if (decc < 64)
                        i++;
                    else
                        break;
                }
                for (; at(value, cur) != 0; ++cur)
                {
                    int decc;

                    decc = typesBase64Decode(at(value, cur));
                    if (decc < 0)
                    {
                        /* ignored */
                    }
                    else if (decc < 64)
                        return 1;
                    if (decc == 64)
                        pad++;
                }

                total = 3 * (i / 4);
                if (pad == 0)
                {
                    if (i % 4 != 0)
                        return 1;
                }
                else if (pad == 1)
                {
                    int decc;

                    if (i % 4 != 3)
                        return 1;
                    for (decc = typesBase64Decode(at(value, cur));
                         ((decc < 0) || (decc > 63)) && (cur > 0);
                         decc = typesBase64Decode(at(value, cur)))
                        --cur;
                    /* 16bits in 24bits means 2 pad bits: nnnnnn nnmmmm mmmm00*/
                    /* 00111100 -> 0x3c */
                    if ((decc & ~0x3c) != 0)
                        return 1;
                    total += 2;
                }
                else if (pad == 2)
                {
                    int decc;

                    if (i % 4 != 2)
                        return 1;
                    for (decc = typesBase64Decode(at(value, cur));
                         ((decc < 0) || (decc > 63)) && (cur > 0);
                         decc = typesBase64Decode(at(value, cur)))
                        --cur;
                    /* 8bits in 12bits means 4 pad bits: nnnnnn nn0000 */
                    /* 00110000 -> 0x30 */
                    if ((decc & ~0x30) != 0)
                        return 1;
                    total += 1;
                }
                else
                    return 1;

                if (val != null)
                {
                    v = typesNewValue(XML_XSD10_SCHEMAS_BASE64BINARY);
                    StringBuilder base = new StringBuilder(i + pad);
                    for (cur = 0; at(value, cur) != 0; ++cur)
                        if (typesBase64Decode(at(value, cur)) >= 0)
                        {
                            base.append((char) at(value, cur));
                        }
                    v.base64.str = base.toString();
                    v.base64.total = total;
                    val[0] = v;
                }
                return 0;
            }
            case XML_XSD10_SCHEMAS_INTEGER:
            case XML_XSD10_SCHEMAS_PINTEGER:
            case XML_XSD10_SCHEMAS_NPINTEGER:
            case XML_XSD10_SCHEMAS_NINTEGER:
            case XML_XSD10_SCHEMAS_NNINTEGER:
            case XML_XSD10_SCHEMAS_LONG:
            case XML_XSD10_SCHEMAS_BYTE:
            case XML_XSD10_SCHEMAS_SHORT:
            case XML_XSD10_SCHEMAS_INT:
            case XML_XSD10_SCHEMAS_UINT:
            case XML_XSD10_SCHEMAS_ULONG:
            case XML_XSD10_SCHEMAS_USHORT:
            case XML_XSD10_SCHEMAS_UBYTE:
            {
                int[] cur = { 0 };
                Xsd10Val.Decimal decimal = new Xsd10Val.Decimal();
                char sign = '+';

                if (normOnTheFly != 0)
                    while (isBlankCh(at(value, cur[0]))) cur[0]++;
                if (at(value, cur[0]) == '-')
                {
                    sign = '-';
                    cur[0]++;
                }
                else if (at(value, cur[0]) == '+')
                    cur[0]++;
                ret = typesParseUint(value, cur, decimal);
                /* add sign */
                if (ret < 0)
                    return 1;
                decimal.str = sign + decimal.str.substring(1);
                if (normOnTheFly != 0)
                    while (isBlankCh(at(value, cur[0]))) cur[0]++;
                if (at(value, cur[0]) != 0)
                    return 1;
                if (type.builtInType == XML_XSD10_SCHEMAS_NPINTEGER)
                {
                    if (typesDecimalCompareWithInteger(decimal, 0) > 0)
                        return 1;
                }
                else if (type.builtInType == XML_XSD10_SCHEMAS_PINTEGER)
                {
                    if (sign == '-')
                        return 1;
                    if (typesDecimalCompareWithInteger(decimal, 0) <= 0)
                        return 1;
                }
                else if (type.builtInType == XML_XSD10_SCHEMAS_NINTEGER)
                {
                    if (typesDecimalCompareWithInteger(decimal, 0) >= 0)
                        return 1;
                }
                else if (type.builtInType == XML_XSD10_SCHEMAS_NNINTEGER)
                {
                    if (typesDecimalCompareWithInteger(decimal, 0) < 0)
                        return 1;
                }
                else if (type.builtInType == XML_XSD10_SCHEMAS_LONG)
                {
                    /* (u)int64_t may not be available on 32 bit platform, just use decimal */
                    Xsd10Val.Decimal tmpDecimal = new Xsd10Val.Decimal();
                    tmpDecimal.fractionalPlaces = 1;
                    tmpDecimal.integralPlaces = 19;
                    tmpDecimal.str = "+9223372036854775807.0";
                    if (typesDecimalCompare(decimal, tmpDecimal) > 0)
                        return 1;
                    tmpDecimal.str = "-9223372036854775808.0";
                    if (typesDecimalCompare(decimal, tmpDecimal) < 0)
                        return 1;
                }
                else if (type.builtInType == XML_XSD10_SCHEMAS_ULONG)
                {
                    Xsd10Val.Decimal tmpDecimal = new Xsd10Val.Decimal();
                    tmpDecimal.fractionalPlaces = 1;
                    tmpDecimal.integralPlaces = 20;
                    tmpDecimal.str = "+18446744073709551615.0";
                    if (typesDecimalCompare(decimal, tmpDecimal) > 0)
                        return 1;
                    if (typesDecimalCompareWithInteger(decimal, 0) < 0)
                        return 1;
                }
                else if (type.builtInType == XML_XSD10_SCHEMAS_INT)
                {
                    if (typesDecimalCompareWithInteger(decimal, 0x7fffffff) > 0) /* INT32_MAX */
                        return 1;
                    if (typesDecimalCompareWithInteger(decimal, -0x7fffffff - 1) < 0) /* INT32_MIN */
                        return 1;
                }
                else if (type.builtInType == XML_XSD10_SCHEMAS_SHORT)
                {
                    if (typesDecimalCompareWithInteger(decimal, 0x7fff) > 0) /* INT16_MAX */
                        return 1;
                    if (typesDecimalCompareWithInteger(decimal, -0x8000) < 0) /* INT16_MIN */
                        return 1;
                }
                else if (type.builtInType == XML_XSD10_SCHEMAS_BYTE)
                {
                    if (typesDecimalCompareWithInteger(decimal, 0x7f) > 0) /* INT8_MAX */
                        return 1;
                    if (typesDecimalCompareWithInteger(decimal, -0x80) < 0) /* INT8_MIN */
                        return 1;
                }
                else if (type.builtInType == XML_XSD10_SCHEMAS_UINT)
                {
                    Xsd10Val.Decimal tmpDecimal = new Xsd10Val.Decimal();
                    tmpDecimal.fractionalPlaces = 1;
                    tmpDecimal.integralPlaces = 10;
                    tmpDecimal.str = "+4294967295.0";
                    if (typesDecimalCompare(decimal, tmpDecimal) > 0)
                        return 1;
                    if (typesDecimalCompareWithInteger(decimal, 0) < 0)
                        return 1;
                }
                else if (type.builtInType == XML_XSD10_SCHEMAS_USHORT)
                {
                    if (typesDecimalCompareWithInteger(decimal, 0xffff) > 0) /* UINT16_MAX */
                        return 1;
                    if (typesDecimalCompareWithInteger(decimal, 0) < 0)
                        return 1;
                }
                else if (type.builtInType == XML_XSD10_SCHEMAS_UBYTE)
                {
                    if (typesDecimalCompareWithInteger(decimal, 0xff) > 0) /* UINT8_MAX */
                        return 1;
                    if (typesDecimalCompareWithInteger(decimal, 0) < 0)
                        return 1;
                }
                if (val != null)
                {
                    v = typesNewValue(type.builtInType);
                    v.decimal = decimal;
                    val[0] = v;
                }
                return 0;
            }
            default:
                break;
        }

        /* done: */
        return ret;
    }

    // xml_xsd10_val_predef_type_node
    /** Port of xmlSchemaValPredefTypeNode.
        @param type the built in type @param value the value @param val the computed value or null
        @param node the node (may be null) @return 0 if valid, a positive error code otherwise, -1 on error */
    public static int valPredefTypeNode(Xsd10Type type, String value, Xsd10Val[] val, XmlNode node)
    {
        return typesValAtomicType(type, b(value), val, node, 0,
            XML_XSD10_SCHEMA_WHITESPACE_UNKNOWN, 1, 1, 0, null);
    }

    // xml_xsd10_val_predef_type_node_ids
    /** Port of xml_xsd10_val_predef_type_node_ids (xmlSchemaValPredefTypeNode with an ID table).
        @param type the built in type @param value the value @param val the computed value or null
        @param node the node (may be null) @param ids the ID table (may be null)
        @return 0 if valid, a positive error code otherwise, -1 on error */
    public static int valPredefTypeNodeIds(Xsd10Type type, String value, Xsd10Val[] val, XmlNode node, Xsd10Ids ids)
    {
        return typesValAtomicType(type, b(value), val, node, 0,
            XML_XSD10_SCHEMA_WHITESPACE_UNKNOWN, 1, 1, 0, ids);
    }

    // xml_xsd10_val_predef_type_node_no_norm
    /** Port of xmlSchemaValPredefTypeNodeNoNorm: this one does not apply any normalization to the value.
        @param type the built in type @param value the value @param val the computed value or null
        @param node the node (may be null) @return 0 if valid, a positive error code otherwise, -1 on error */
    public static int valPredefTypeNodeNoNorm(Xsd10Type type, String value, Xsd10Val[] val, XmlNode node)
    {
        return typesValAtomicType(type, b(value), val, node, 1,
            XML_XSD10_SCHEMA_WHITESPACE_UNKNOWN, 1, 0, 1, null);
    }

    // xml_xsd10_val_predef_type_node_no_norm_ids
    /** Port of xml_xsd10_val_predef_type_node_no_norm_ids.
        @param type the built in type @param value the value @param val the computed value or null
        @param node the node (may be null) @param ids the ID table (may be null)
        @return 0 if valid, a positive error code otherwise, -1 on error */
    public static int valPredefTypeNodeNoNormIds(Xsd10Type type, String value, Xsd10Val[] val, XmlNode node, Xsd10Ids ids)
    {
        return typesValAtomicType(type, b(value), val, node, 1,
            XML_XSD10_SCHEMA_WHITESPACE_UNKNOWN, 1, 0, 1, ids);
    }

    // xml_xsd10_validate_predefined_type
    /** Port of xmlSchemaValidatePredefinedType.
        @param type the built in type @param value the value @param val the computed value or null
        @return 0 if valid, a positive error code otherwise, -1 on error */
    public static int validatePredefinedType(Xsd10Type type, String value, Xsd10Val[] val)
    {
        return valPredefTypeNode(type, value, val, null);
    }

    // xsd10_types_compare_decimals
    /* Port of xmlSchemaCompareDecimals: -1 if x < y, 0 if x == y, 1 if x > y and -2 in case of error. */
    private static int typesCompareDecimals(Xsd10Val x, Xsd10Val y)
    {
        int res = typesDecimalCompare(x.decimal, y.decimal);
        if (res > 0)
        {
            return 1;
        }
        if (res < 0)
        {
            return -1;
        }
        return 0;
    }

    private static final long[][] dayRange = {
        { 0, 28, 59, 89, 120, 150, 181, 212, 242, 273, 303, 334, },
        { 0, 31, 62, 92, 123, 153, 184, 215, 245, 276, 306, 337 } };

    // xsd10_types_compare_durations
    /* Port of xmlSchemaCompareDurations: -1 if x < y, 0 if x == y, 1 if x > y, 2 if x <> y, and -2 in
       case of error. */
    private static int typesCompareDurations(Xsd10Val x, Xsd10Val y)
    {
        long carry, mon, day;
        double sec;
        int invert = 1;
        long xmon, xday, myear, minday, maxday;

        if ((x == null) || (y == null))
            return -2;

        /* months */
        mon = x.dur.mon - y.dur.mon;

        /* seconds */
        sec = x.dur.sec - y.dur.sec;
        carry = (long) (sec / XSD10_TYPES_SECS_PER_DAY);
        sec -= ((double) carry) * XSD10_TYPES_SECS_PER_DAY;

        /* days */
        day = x.dur.day - y.dur.day + carry;

        /* easy test */
        if (mon == 0)
        {
            if (day == 0)
                if (sec == 0.0)
                    return 0;
                else if (sec < 0.0)
                    return -1;
                else
                    return 1;
            else if (day < 0)
                return -1;
            else
                return 1;
        }

        if (mon > 0)
        {
            if ((day >= 0) && (sec >= 0.0))
                return 1;
            else
            {
                xmon = mon;
                xday = -day;
            }
        }
        else if ((day <= 0) && (sec <= 0.0))
        {
            return -1;
        }
        else
        {
            invert = -1;
            xmon = -mon;
            xday = day;
        }

        myear = xmon / 12;
        if (myear == 0)
        {
            minday = 0;
            maxday = 0;
        }
        else
        {
            if (myear > Long.MAX_VALUE / 366)
                return -2;
            /* FIXME: This doesn't take leap year exceptions every 100/400 years
               into account. */
            maxday = 365 * myear + (myear + 3) / 4;
            /* FIXME: Needs to be calculated separately */
            minday = maxday - 1;
        }

        xmon = xmon % 12;
        minday += tab(dayRange[0], xmon);
        maxday += tab(dayRange[1], xmon);

        if ((maxday == minday) && (maxday == xday))
            return 0; /* can this really happen ? */
        if (maxday < xday)
            return -invert;
        if (minday > xday)
            return invert;

        /* indeterminate */
        return 2;
    }

    /*
     * macros for adding date/times and durations
     */
    private static double fquotient(double a, double b)
    {
        return Math.floor(a / b);
    }

    private static double modulo(double a, double b)
    {
        return a - fquotient(a, b) * b;
    }

    private static double fquotientRange(double a, double low, double high)
    {
        return fquotient(a - low, high - low);
    }

    private static double moduloRange(double a, double low, double high)
    {
        return modulo(a - low, high - low) + low;
    }

    /** C conversion of a double to unsigned int (as done for the bit fields), truncated to bits. */
    private static int d2bits(double d, int bits)
    {
        long l = (long) d;
        return (int) (l & ((1L << bits) - 1));
    }

    // xsd10_types_dup_val
    /* Port of xmlSchemaDupVal: makes a copy of v. */
    private static Xsd10Val typesDupVal(Xsd10Val v)
    {
        Xsd10Val ret = typesNewValue(v.type);
        ret.copyValueFrom(v);
        ret.next = null;
        return ret;
    }

    // xml_xsd10_copy_value
    /** Port of xmlSchemaCopyValue: copies the precomputed value.
        @param val the value @return the copy or null if a copy for a data-type is not implemented */
    public static Xsd10Val copyValue(Xsd10Val val)
    {
        Xsd10Val ret = null, prev = null, cur;

        /*
         * Copy the string values.
         */
        while (val != null)
        {
            switch (val.type)
            {
                case XML_XSD10_SCHEMAS_ANYTYPE:
                case XML_XSD10_SCHEMAS_IDREFS:
                case XML_XSD10_SCHEMAS_ENTITIES:
                case XML_XSD10_SCHEMAS_NMTOKENS:
                    return null;
                default:
                    /* The strings are immutable, a copy of the struct copies them. */
                    cur = typesDupVal(val);
                    break;
            }
            if (ret == null)
                ret = cur;
            else
                prev.next = cur;
            prev = cur;
            val = val.next;
        }
        return ret;
    }

    // xsd10_types_date_add
    /* Port of _xmlSchemaDateAdd: compute a new date/time from dt and dur. This function assumes dt
       is either XML_SCHEMAS_DATETIME, XML_SCHEMAS_DATE, XML_SCHEMAS_GYEARMONTH,
       or XML_SCHEMAS_GYEAR. The returned value is the same type as dt. */
    private static Xsd10Val typesDateAdd(Xsd10Val dt, Xsd10Val dur)
    {
        Xsd10Val ret, tmp;
        long carry, tempdays, temp;
        Xsd10Val.Date r, d;
        Xsd10Val.Duration u;

        if ((dt == null) || (dur == null))
            return null;

        ret = typesNewValue(dt.type);

        /* make a copy so we don't alter the original value */
        tmp = typesDupVal(dt);

        r = ret.date;
        d = tmp.date;
        u = dur.dur;

        /* normalization */
        if (d.mon == 0)
            d.mon = 1;

        /* normalize for time zone offset */
        u.sec -= (d.tzo * 60);
        d.tzo = 0;

        /* normalization */
        if (d.day == 0)
            d.day = 1;

        /* month */
        carry = d.mon + u.mon;
        r.mon = d2bits(moduloRange(carry, 1, 13), 4);
        carry = (long) fquotientRange(carry, 1, 13);

        /* year (may be modified later) */
        r.year = d.year + carry;
        if (r.year == 0)
        {
            if (d.year > 0)
                r.year--;
            else
                r.year++;
        }

        /* time zone */
        r.tzo = d.tzo;
        r.tz_flag = d.tz_flag;

        /* seconds */
        r.sec = d.sec + u.sec;
        carry = (long) fquotient((long) r.sec, 60);
        if (r.sec != 0.0)
        {
            r.sec = modulo(r.sec, 60.0);
        }

        /* minute */
        carry += d.min;
        r.min = d2bits(modulo(carry, 60), 6);
        carry = (long) fquotient(carry, 60);

        /* hours */
        carry += d.hour;
        r.hour = d2bits(modulo(carry, 24), 5);
        carry = (long) fquotient(carry, 24);

        /*
         * days
         * Note we use tempdays because the temporary values may need more
         * than 5 bits
         */
        if ((validYear(r.year)) && (validMonth(r.mon)) &&
            (d.day > maxDayInMonth(r.year, r.mon)))
            tempdays = maxDayInMonth(r.year, r.mon);
        else if (d.day < 1)
            tempdays = 1;
        else
            tempdays = d.day;

        tempdays += u.day + carry;

        while (true)
        {
            if (tempdays < 1)
            {
                long tmon = (long) moduloRange(r.mon - 1, 1, 13);
                long tyr = r.year + (long) fquotientRange(r.mon - 1, 1, 13);
                if (tyr == 0)
                    tyr--;
                /*
                 * Coverity detected an overrun in daysInMonth
                 * of size 12 at position 12 with index variable "((r)->mon - 1)"
                 */
                if (tmon < 1)
                    tmon = 1;
                if (tmon > 12)
                    tmon = 12;
                tempdays += maxDayInMonth(tyr, tmon);
                carry = -1;
            }
            else if (validYear(r.year) && validMonth(r.mon) &&
                     tempdays > maxDayInMonth(r.year, r.mon))
            {
                tempdays = tempdays - maxDayInMonth(r.year, r.mon);
                carry = 1;
            }
            else
                break;

            temp = r.mon + carry;
            r.mon = d2bits(moduloRange(temp, 1, 13), 4);
            r.year = r.year + (long) fquotientRange(temp, 1, 13);
            if (r.year == 0)
            {
                if (temp < 1)
                    r.year--;
                else
                    r.year++;
            }
        }

        r.day = (int) (tempdays & 0x1f);

        /*
         * adjust the date/time type to the date values
         */
        if (ret.type != XML_XSD10_SCHEMAS_DATETIME)
        {
            if ((r.hour != 0) || (r.min != 0) || (r.sec != 0))
                ret.type = XML_XSD10_SCHEMAS_DATETIME;
            else if (ret.type != XML_XSD10_SCHEMAS_DATE)
            {
                if ((r.mon != 1) && (r.day != 1))
                    ret.type = XML_XSD10_SCHEMAS_DATE;
                else if ((ret.type != XML_XSD10_SCHEMAS_GYEARMONTH) && (r.mon != 1))
                    ret.type = XML_XSD10_SCHEMAS_GYEARMONTH;
            }
        }

        return ret;
    }

    // xsd10_types_date_normalize
    /* Port of xmlSchemaDateNormalize: normalize dt to GMT time. The offset parameter is subtracted
       from the return value is a time-zone offset is present on dt.
       Returns a normalized copy of dt or null if error. */
    private static Xsd10Val typesDateNormalize(Xsd10Val dt, double offset)
    {
        Xsd10Val dur, ret;

        if (dt == null)
            return null;

        if (((dt.type != XML_XSD10_SCHEMAS_TIME) &&
             (dt.type != XML_XSD10_SCHEMAS_DATETIME) &&
             (dt.type != XML_XSD10_SCHEMAS_DATE)) || (dt.date.tzo == 0))
            return typesDupVal(dt);

        dur = typesNewValue(XML_XSD10_SCHEMAS_DURATION);

        /* C writes dur->value.date.sec, which is the same union memory as value.dur.sec */
        dur.dur.sec -= offset;

        ret = typesDateAdd(dt, dur);

        if (ret == null)
            return null;

        /* ret->value.date.tzo = 0; */
        return ret;
    }

    // xsd10_types_date_cast_ym_to_days
    /* Port of _xmlSchemaDateCastYMToDays: convert mon and year of dt to total number of days. */
    private static long typesDateCastYmToDays(Xsd10Val dt)
    {
        long ret;
        int mon;

        mon = dt.date.mon;
        if (mon <= 0) mon = 1; /* normalization */

        if (dt.date.year <= 0)
            ret = (dt.date.year * 365) +
                  (((dt.date.year + 1) / 4) - ((dt.date.year + 1) / 100) +
                   ((dt.date.year + 1) / 400)) +
                  dayInYear(0, mon, dt.date.year);
        else
            ret = ((dt.date.year - 1) * 365) +
                  (((dt.date.year - 1) / 4) - ((dt.date.year - 1) / 100) +
                   ((dt.date.year - 1) / 400)) +
                  dayInYear(0, mon, dt.date.year);

        return ret;
    }

    /* Port of TIME_TO_NUMBER: calculates the number of seconds in the time portion of dt. */
    private static double timeToNumber(Xsd10Val dt)
    {
        return ((double) ((dt.date.hour * XSD10_TYPES_SECS_PER_HOUR) +
                          (dt.date.min * XSD10_TYPES_SECS_PER_MIN) +
                          (dt.date.tzo * XSD10_TYPES_SECS_PER_MIN))) +
               dt.date.sec;
    }

    // xsd10_types_compare_dates
    /* Port of xmlSchemaCompareDates: -1 if x < y, 0 if x == y, 1 if x > y, 2 if x <> y, and -2 in
       case of error. */
    private static int typesCompareDates(Xsd10Val x, Xsd10Val y)
    {
        int xmask, ymask, xor_mask, and_mask;
        Xsd10Val p1, p2, q1, q2;
        long p1d, p2d, q1d, q2d;

        if ((x == null) || (y == null))
            return -2;

        if ((x.date.year > Long.MAX_VALUE / 366) ||
            (x.date.year < Long.MIN_VALUE / 366) ||
            (y.date.year > Long.MAX_VALUE / 366) ||
            (y.date.year < Long.MIN_VALUE / 366))
        {
            /* Possible overflow when converting to days. */
            return -2;
        }

        if (x.date.tz_flag != 0)
        {
            if (y.date.tz_flag == 0)
            {
                p1 = typesDateNormalize(x, 0);
                if (p1 == null)
                    return -2;
                p1d = typesDateCastYmToDays(p1) + p1.date.day;
                /* normalize y + 14:00 */
                q1 = typesDateNormalize(y, (14 * XSD10_TYPES_SECS_PER_HOUR));
                if (q1 == null)
                {
                    return -2;
                }

                q1d = typesDateCastYmToDays(q1) + q1.date.day;
                if (p1d < q1d)
                {
                    return -1;
                }
                else if (p1d == q1d)
                {
                    double sec;

                    sec = timeToNumber(p1) - timeToNumber(q1);
                    if (sec < 0.0)
                    {
                        return -1;
                    }
                    else
                    {
                        int ret = 0;
                        /* normalize y - 14:00 */
                        q2 = typesDateNormalize(y, -(14 * XSD10_TYPES_SECS_PER_HOUR));
                        if (q2 == null)
                        {
                            return -2;
                        }
                        q2d = typesDateCastYmToDays(q2) + q2.date.day;
                        if (p1d > q2d)
                            ret = 1;
                        else if (p1d == q2d)
                        {
                            sec = timeToNumber(p1) - timeToNumber(q2);
                            if (sec > 0.0)
                                ret = 1;
                            else
                                ret = 2; /* indeterminate */
                        }
                        if (ret != 0)
                            return ret;
                    }
                }
            }
        }
        else if (y.date.tz_flag != 0)
        {
            q1 = typesDateNormalize(y, 0);
            if (q1 == null)
                return -2;
            q1d = typesDateCastYmToDays(q1) + q1.date.day;

            /* normalize x - 14:00 */
            p1 = typesDateNormalize(x, -(14 * XSD10_TYPES_SECS_PER_HOUR));
            if (p1 == null)
            {
                return -2;
            }
            p1d = typesDateCastYmToDays(p1) + p1.date.day;

            if (p1d < q1d)
            {
                return -1;
            }
            else if (p1d == q1d)
            {
                double sec;

                sec = timeToNumber(p1) - timeToNumber(q1);
                if (sec < 0.0)
                {
                    return -1;
                }
                else
                {
                    int ret = 0;
                    /* normalize x + 14:00 */
                    p2 = typesDateNormalize(x, (14 * XSD10_TYPES_SECS_PER_HOUR));
                    if (p2 == null)
                    {
                        return -2;
                    }
                    p2d = typesDateCastYmToDays(p2) + p2.date.day;

                    if (p2d > q1d)
                    {
                        ret = 1;
                    }
                    else if (p2d == q1d)
                    {
                        sec = timeToNumber(p2) - timeToNumber(q1);
                        if (sec > 0.0)
                            ret = 1;
                        else
                            ret = 2; /* indeterminate */
                    }
                    if (ret != 0)
                        return ret;
                }
            }
        }

        /*
         * if the same type then calculate the difference
         */
        if (x.type == y.type)
        {
            int ret = 0;
            q1 = typesDateNormalize(y, 0);
            if (q1 == null)
                return -2;
            q1d = typesDateCastYmToDays(q1) + q1.date.day;

            p1 = typesDateNormalize(x, 0);
            if (p1 == null)
            {
                return -2;
            }
            p1d = typesDateCastYmToDays(p1) + p1.date.day;

            if (p1d < q1d)
            {
                ret = -1;
            }
            else if (p1d > q1d)
            {
                ret = 1;
            }
            else
            {
                double sec;

                sec = timeToNumber(p1) - timeToNumber(q1);
                if (sec < 0.0)
                    ret = -1;
                else if (sec > 0.0)
                    ret = 1;
            }
            return ret;
        }

        xmask = dateMask(x.type);
        ymask = dateMask(y.type);

        xor_mask = xmask ^ ymask;           /* mark type differences */
        and_mask = xmask & ymask;           /* mark field specification */

        /* year */
        if ((xor_mask & 1) != 0)
            return 2; /* indeterminate */
        else if ((and_mask & 1) != 0)
        {
            if (x.date.year < y.date.year)
                return -1;
            else if (x.date.year > y.date.year)
                return 1;
        }

        /* month */
        if ((xor_mask & 2) != 0)
            return 2; /* indeterminate */
        else if ((and_mask & 2) != 0)
        {
            if (x.date.mon < y.date.mon)
                return -1;
            else if (x.date.mon > y.date.mon)
                return 1;
        }

        /* day */
        if ((xor_mask & 4) != 0)
            return 2; /* indeterminate */
        else if ((and_mask & 4) != 0)
        {
            if (x.date.day < y.date.day)
                return -1;
            else if (x.date.day > y.date.day)
                return 1;
        }

        /* time */
        if ((xor_mask & 8) != 0)
            return 2; /* indeterminate */
        else if ((and_mask & 8) != 0)
        {
            if (x.date.hour < y.date.hour)
                return -1;
            else if (x.date.hour > y.date.hour)
                return 1;
            else if (x.date.min < y.date.min)
                return -1;
            else if (x.date.min > y.date.min)
                return 1;
            else if (x.date.sec < y.date.sec)
                return -1;
            else if (x.date.sec > y.date.sec)
                return 1;
        }

        return 0;
    }

    /** The field masks of xsd10_types_compare_dates. */
    private static int dateMask(int type)
    {
        switch (type)
        {
            case XML_XSD10_SCHEMAS_DATETIME:
                return 0xf;
            case XML_XSD10_SCHEMAS_DATE:
                return 0x7;
            case XML_XSD10_SCHEMAS_GYEAR:
                return 0x1;
            case XML_XSD10_SCHEMAS_GMONTH:
                return 0x2;
            case XML_XSD10_SCHEMAS_GDAY:
                return 0x3;
            case XML_XSD10_SCHEMAS_GYEARMONTH:
                return 0x3;
            case XML_XSD10_SCHEMAS_GMONTHDAY:
                return 0x6;
            case XML_XSD10_SCHEMAS_TIME:
                return 0x8;
            default:
                return 0;
        }
    }

    // xsd10_types_compare_preserve_replace_strings
    /* Port of xmlSchemaComparePreserveReplaceStrings: x is a string with whitespace of "preserve",
       y is a string with a whitespace of "replace". invert inverts the result.
       Returns -1 if x < y, 0 if x == y, 1 if x > y, and -2 in case of error. */
    private static int typesComparePreserveReplaceStrings(byte[] xs, byte[] ys, int invert)
    {
        int tmp;
        int x = 0, y = 0;

        while ((at(xs, x) != 0) && (at(ys, y) != 0))
        {
            if (isWspReplaceCh(at(ys, y)))
            {
                if (!isWspSpaceCh(at(xs, x)))
                {
                    if ((at(xs, x) - 0x20) < 0)
                    {
                        return invert != 0 ? 1 : -1;
                    }
                    else
                    {
                        return invert != 0 ? -1 : 1;
                    }
                }
            }
            else
            {
                tmp = at(xs, x) - at(ys, y);
                if (tmp < 0)
                {
                    return invert != 0 ? 1 : -1;
                }
                if (tmp > 0)
                {
                    return invert != 0 ? -1 : 1;
                }
            }
            x++;
            y++;
        }
        if (at(xs, x) != 0)
        {
            return invert != 0 ? -1 : 1;
        }
        if (at(ys, y) != 0)
        {
            return invert != 0 ? 1 : -1;
        }
        return 0;
    }

    // xsd10_types_compare_preserve_collapse_strings
    /* Port of xmlSchemaComparePreserveCollapseStrings: x is a string with whitespace of "preserve",
       y is a string with a whitespace of "collapse". */
    private static int typesComparePreserveCollapseStrings(byte[] xs, byte[] ys, int invert)
    {
        int tmp;
        int x = 0, y = 0;

        /*
         * Skip leading blank chars of the collapsed string.
         */
        while (isBlankCh(at(ys, y)))
            y++;

        while ((at(xs, x) != 0) && (at(ys, y) != 0))
        {
            if (isBlankCh(at(ys, y)))
            {
                if (!isWspSpaceCh(at(xs, x)))
                {
                    /*
                     * The yv character would have been replaced to 0x20.
                     */
                    if ((at(xs, x) - 0x20) < 0)
                    {
                        return invert != 0 ? 1 : -1;
                    }
                    else
                    {
                        return invert != 0 ? -1 : 1;
                    }
                }
                x++;
                y++;
                /*
                 * Skip contiguous blank chars of the collapsed string.
                 */
                while (isBlankCh(at(ys, y)))
                    y++;
            }
            else
            {
                tmp = at(xs, x++) - at(ys, y++);
                if (tmp < 0)
                {
                    return invert != 0 ? 1 : -1;
                }
                if (tmp > 0)
                {
                    return invert != 0 ? -1 : 1;
                }
            }
        }
        if (at(xs, x) != 0)
        {
            return invert != 0 ? -1 : 1;
        }
        if (at(ys, y) != 0)
        {
            /*
             * Skip trailing blank chars of the collapsed string.
             */
            while (isBlankCh(at(ys, y)))
                y++;
            if (at(ys, y) != 0)
            {
                return invert != 0 ? 1 : -1;
            }
        }
        return 0;
    }

    // xsd10_types_compare_replace_collapse_strings
    /* Port of xmlSchemaCompareReplaceCollapseStrings: x is a string with whitespace of "replace",
       y is a string with a whitespace of "collapse". */
    private static int typesCompareReplaceCollapseStrings(byte[] xs, byte[] ys, int invert)
    {
        int tmp;
        int x = 0, y = 0;

        /*
         * Skip leading blank chars of the collapsed string.
         */
        while (isBlankCh(at(ys, y)))
            y++;

        while ((at(xs, x) != 0) && (at(ys, y) != 0))
        {
            if (isBlankCh(at(ys, y)))
            {
                if (!isBlankCh(at(xs, x)))
                {
                    /*
                     * The yv character would have been replaced to 0x20.
                     */
                    if ((at(xs, x) - 0x20) < 0)
                    {
                        return invert != 0 ? 1 : -1;
                    }
                    else
                    {
                        return invert != 0 ? -1 : 1;
                    }
                }
                x++;
                y++;
                /*
                 * Skip contiguous blank chars of the collapsed string.
                 */
                while (isBlankCh(at(ys, y)))
                    y++;
            }
            else
            {
                if (isBlankCh(at(xs, x)))
                {
                    /*
                     * The xv character would have been replaced to 0x20.
                     */
                    if ((0x20 - at(ys, y)) < 0)
                    {
                        return invert != 0 ? 1 : -1;
                    }
                    else
                    {
                        return invert != 0 ? -1 : 1;
                    }
                }
                tmp = at(xs, x++) - at(ys, y++);
                if (tmp < 0)
                    return -1;
                if (tmp > 0)
                    return 1;
            }
        }
        if (at(xs, x) != 0)
        {
            return invert != 0 ? -1 : 1;
        }
        if (at(ys, y) != 0)
        {
            /*
             * Skip trailing blank chars of the collapsed string.
             */
            while (isBlankCh(at(ys, y)))
                y++;
            if (at(ys, y) != 0)
            {
                return invert != 0 ? 1 : -1;
            }
        }
        return 0;
    }

    // xsd10_types_compare_replaced_strings
    /* Port of xmlSchemaCompareReplacedStrings: compare 2 string for their normalized values. */
    private static int typesCompareReplacedStrings(byte[] xs, byte[] ys)
    {
        int tmp;
        int x = 0, y = 0;

        while ((at(xs, x) != 0) && (at(ys, y) != 0))
        {
            if (isBlankCh(at(ys, y)))
            {
                if (!isBlankCh(at(xs, x)))
                {
                    if ((at(xs, x) - 0x20) < 0)
                        return -1;
                    else
                        return 1;
                }
            }
            else
            {
                if (isBlankCh(at(xs, x)))
                {
                    if ((0x20 - at(ys, y)) < 0)
                        return -1;
                    else
                        return 1;
                }
                tmp = at(xs, x) - at(ys, y);
                if (tmp < 0)
                    return -1;
                if (tmp > 0)
                    return 1;
            }
            x++;
            y++;
        }
        if (at(xs, x) != 0)
            return 1;
        if (at(ys, y) != 0)
            return -1;
        return 0;
    }

    // xsd10_types_compare_norm_strings
    /* Port of xmlSchemaCompareNormStrings: compare 2 string for their normalized values.
       Note that like libxml2 the raw byte difference can be returned. */
    private static int typesCompareNormStrings(byte[] xs, byte[] ys)
    {
        int tmp;
        int x = 0, y = 0;

        while (isBlankCh(at(xs, x))) x++;
        while (isBlankCh(at(ys, y))) y++;
        while ((at(xs, x) != 0) && (at(ys, y) != 0))
        {
            if (isBlankCh(at(xs, x)))
            {
                if (!isBlankCh(at(ys, y)))
                {
                    tmp = at(xs, x) - at(ys, y);
                    return tmp;
                }
                while (isBlankCh(at(xs, x))) x++;
                while (isBlankCh(at(ys, y))) y++;
            }
            else
            {
                tmp = at(xs, x++) - at(ys, y++);
                if (tmp < 0)
                    return -1;
                if (tmp > 0)
                    return 1;
            }
        }
        if (at(xs, x) != 0)
        {
            while (isBlankCh(at(xs, x))) x++;
            if (at(xs, x) != 0)
                return 1;
        }
        if (at(ys, y) != 0)
        {
            while (isBlankCh(at(ys, y))) y++;
            if (at(ys, y) != 0)
                return -1;
        }
        return 0;
    }

    // xsd10_types_compare_floats
    /* Port of xmlSchemaCompareFloats: -1 if x < y, 0 if x == y, 1 if x > y, 2 if x <> y, and -2 in
       case of error. */
    private static int typesCompareFloats(Xsd10Val x, Xsd10Val y)
    {
        double d1, d2;

        if ((x == null) || (y == null))
            return -2;

        /*
         * Cast everything to doubles.
         */
        if (x.type == XML_XSD10_SCHEMAS_DOUBLE)
            d1 = x.d;
        else if (x.type == XML_XSD10_SCHEMAS_FLOAT)
            d1 = x.f;
        else
            return -2;

        if (y.type == XML_XSD10_SCHEMAS_DOUBLE)
            d2 = y.d;
        else if (y.type == XML_XSD10_SCHEMAS_FLOAT)
            d2 = y.f;
        else
            return -2;

        /*
         * Check for special cases.
         */
        if (Double.isNaN(d1))
        {
            if (Double.isNaN(d2))
                return 0;
            return 1;
        }
        if (Double.isNaN(d2))
            return -1;

        if (d1 < d2)
            return -1;
        if (d1 > d2)
            return 1;
        if (d1 == d2)
            return 0;
        return 2;
    }

    /** The str member of a string value (C value.str; for NOTATION/QNAME that is the same union
        memory as value.qname.name). */
    private static String unionStr(Xsd10Val v)
    {
        if (v.type == XML_XSD10_SCHEMAS_QNAME || v.type == XML_XSD10_SCHEMAS_NOTATION)
            return v.qname.name;
        return v.str;
    }

    // xsd10_types_compare_values_internal
    /* Port of xmlSchemaCompareValuesInternal: -1 if x < y, 0 if x == y, 1 if x > y, 2 if x <> y,
       3 if not comparable and -2 in case of error. */
    private static int typesCompareValuesInternal(int xtype, Xsd10Val x, byte[] xvalue, int xws,
                                                  int ytype, Xsd10Val y, byte[] yvalue, int yws)
    {
        switch (xtype)
        {
            case XML_XSD10_SCHEMAS_UNKNOWN:
            case XML_XSD10_SCHEMAS_ANYTYPE:
                return -2;
            case XML_XSD10_SCHEMAS_INTEGER:
            case XML_XSD10_SCHEMAS_NPINTEGER:
            case XML_XSD10_SCHEMAS_NINTEGER:
            case XML_XSD10_SCHEMAS_NNINTEGER:
            case XML_XSD10_SCHEMAS_PINTEGER:
            case XML_XSD10_SCHEMAS_INT:
            case XML_XSD10_SCHEMAS_UINT:
            case XML_XSD10_SCHEMAS_LONG:
            case XML_XSD10_SCHEMAS_ULONG:
            case XML_XSD10_SCHEMAS_SHORT:
            case XML_XSD10_SCHEMAS_USHORT:
            case XML_XSD10_SCHEMAS_BYTE:
            case XML_XSD10_SCHEMAS_UBYTE:
            case XML_XSD10_SCHEMAS_DECIMAL:
                if ((x == null) || (y == null))
                    return -2;
                if (ytype == xtype)
                    return typesCompareDecimals(x, y);
                if ((ytype == XML_XSD10_SCHEMAS_DECIMAL) ||
                    (ytype == XML_XSD10_SCHEMAS_INTEGER) ||
                    (ytype == XML_XSD10_SCHEMAS_NPINTEGER) ||
                    (ytype == XML_XSD10_SCHEMAS_NINTEGER) ||
                    (ytype == XML_XSD10_SCHEMAS_NNINTEGER) ||
                    (ytype == XML_XSD10_SCHEMAS_PINTEGER) ||
                    (ytype == XML_XSD10_SCHEMAS_INT) ||
                    (ytype == XML_XSD10_SCHEMAS_UINT) ||
                    (ytype == XML_XSD10_SCHEMAS_LONG) ||
                    (ytype == XML_XSD10_SCHEMAS_ULONG) ||
                    (ytype == XML_XSD10_SCHEMAS_SHORT) ||
                    (ytype == XML_XSD10_SCHEMAS_USHORT) ||
                    (ytype == XML_XSD10_SCHEMAS_BYTE) ||
                    (ytype == XML_XSD10_SCHEMAS_UBYTE))
                    return typesCompareDecimals(x, y);
                return -2;
            case XML_XSD10_SCHEMAS_DURATION:
                if ((x == null) || (y == null))
                    return -2;
                if (ytype == XML_XSD10_SCHEMAS_DURATION)
                    return typesCompareDurations(x, y);
                return -2;
            case XML_XSD10_SCHEMAS_TIME:
            case XML_XSD10_SCHEMAS_GDAY:
            case XML_XSD10_SCHEMAS_GMONTH:
            case XML_XSD10_SCHEMAS_GMONTHDAY:
            case XML_XSD10_SCHEMAS_GYEAR:
            case XML_XSD10_SCHEMAS_GYEARMONTH:
            case XML_XSD10_SCHEMAS_DATE:
            case XML_XSD10_SCHEMAS_DATETIME:
                if ((x == null) || (y == null))
                    return -2;
                if ((ytype == XML_XSD10_SCHEMAS_DATETIME) ||
                    (ytype == XML_XSD10_SCHEMAS_TIME) ||
                    (ytype == XML_XSD10_SCHEMAS_GDAY) ||
                    (ytype == XML_XSD10_SCHEMAS_GMONTH) ||
                    (ytype == XML_XSD10_SCHEMAS_GMONTHDAY) ||
                    (ytype == XML_XSD10_SCHEMAS_GYEAR) ||
                    (ytype == XML_XSD10_SCHEMAS_DATE) ||
                    (ytype == XML_XSD10_SCHEMAS_GYEARMONTH))
                    return typesCompareDates(x, y);
                return -2;
            /*
             * Note that we will support comparison of string types against
             * anySimpleType as well.
             */
            case XML_XSD10_SCHEMAS_ANYSIMPLETYPE:
            case XML_XSD10_SCHEMAS_STRING:
            case XML_XSD10_SCHEMAS_NORMSTRING:
            case XML_XSD10_SCHEMAS_TOKEN:
            case XML_XSD10_SCHEMAS_LANGUAGE:
            case XML_XSD10_SCHEMAS_NMTOKEN:
            case XML_XSD10_SCHEMAS_NAME:
            case XML_XSD10_SCHEMAS_NCNAME:
            case XML_XSD10_SCHEMAS_ID:
            case XML_XSD10_SCHEMAS_IDREF:
            case XML_XSD10_SCHEMAS_ENTITY:
            case XML_XSD10_SCHEMAS_ANYURI:
            {
                byte[] xv, yv;

                if (x == null)
                    xv = xvalue;
                else
                    xv = b(unionStr(x));
                if (y == null)
                    yv = yvalue;
                else
                    yv = b(unionStr(y));
                /*
                 * TODO: Compare those against QName.
                 */
                if (ytype == XML_XSD10_SCHEMAS_QNAME)
                {
                    /* TODO */
                    return -2;
                }
                if ((ytype == XML_XSD10_SCHEMAS_ANYSIMPLETYPE) ||
                    (ytype == XML_XSD10_SCHEMAS_STRING) ||
                    (ytype == XML_XSD10_SCHEMAS_NORMSTRING) ||
                    (ytype == XML_XSD10_SCHEMAS_TOKEN) ||
                    (ytype == XML_XSD10_SCHEMAS_LANGUAGE) ||
                    (ytype == XML_XSD10_SCHEMAS_NMTOKEN) ||
                    (ytype == XML_XSD10_SCHEMAS_NAME) ||
                    (ytype == XML_XSD10_SCHEMAS_NCNAME) ||
                    (ytype == XML_XSD10_SCHEMAS_ID) ||
                    (ytype == XML_XSD10_SCHEMAS_IDREF) ||
                    (ytype == XML_XSD10_SCHEMAS_ENTITY) ||
                    (ytype == XML_XSD10_SCHEMAS_ANYURI))
                {
                    if (xws == XML_XSD10_SCHEMA_WHITESPACE_PRESERVE)
                    {
                        if (yws == XML_XSD10_SCHEMA_WHITESPACE_PRESERVE)
                        {
                            /* TODO: What about x < y or x > y. */
                            if (typesStrEqual(xv, yv) != 0)
                                return 0;
                            else
                                return 2;
                        }
                        else if (yws == XML_XSD10_SCHEMA_WHITESPACE_REPLACE)
                            return typesComparePreserveReplaceStrings(xv, yv, 0);
                        else if (yws == XML_XSD10_SCHEMA_WHITESPACE_COLLAPSE)
                            return typesComparePreserveCollapseStrings(xv, yv, 0);
                    }
                    else if (xws == XML_XSD10_SCHEMA_WHITESPACE_REPLACE)
                    {
                        if (yws == XML_XSD10_SCHEMA_WHITESPACE_PRESERVE)
                            return typesComparePreserveReplaceStrings(yv, xv, 1);
                        if (yws == XML_XSD10_SCHEMA_WHITESPACE_REPLACE)
                            return typesCompareReplacedStrings(xv, yv);
                        if (yws == XML_XSD10_SCHEMA_WHITESPACE_COLLAPSE)
                            return typesCompareReplaceCollapseStrings(xv, yv, 0);
                    }
                    else if (xws == XML_XSD10_SCHEMA_WHITESPACE_COLLAPSE)
                    {
                        if (yws == XML_XSD10_SCHEMA_WHITESPACE_PRESERVE)
                            return typesComparePreserveCollapseStrings(yv, xv, 1);
                        if (yws == XML_XSD10_SCHEMA_WHITESPACE_REPLACE)
                            return typesCompareReplaceCollapseStrings(yv, xv, 1);
                        if (yws == XML_XSD10_SCHEMA_WHITESPACE_COLLAPSE)
                            return typesCompareNormStrings(xv, yv);
                    }
                    else
                        return -2;
                }
                return -2;
            }
            case XML_XSD10_SCHEMAS_QNAME:
            case XML_XSD10_SCHEMAS_NOTATION:
                if ((x == null) || (y == null))
                    return -2;
                if ((ytype == XML_XSD10_SCHEMAS_QNAME) ||
                    (ytype == XML_XSD10_SCHEMAS_NOTATION))
                {
                    if ((Objects.equals(x.qname.name, y.qname.name)) &&
                        (Objects.equals(x.qname.uri, y.qname.uri)))
                        return 0;
                    return 2;
                }
                return -2;
            case XML_XSD10_SCHEMAS_FLOAT:
            case XML_XSD10_SCHEMAS_DOUBLE:
                if ((x == null) || (y == null))
                    return -2;
                if ((ytype == XML_XSD10_SCHEMAS_FLOAT) ||
                    (ytype == XML_XSD10_SCHEMAS_DOUBLE))
                    return typesCompareFloats(x, y);
                return -2;
            case XML_XSD10_SCHEMAS_BOOLEAN:
                if ((x == null) || (y == null))
                    return -2;
                if (ytype == XML_XSD10_SCHEMAS_BOOLEAN)
                {
                    if (x.b == y.b)
                        return 0;
                    if (x.b == 0)
                        return -1;
                    return 1;
                }
                return -2;
            case XML_XSD10_SCHEMAS_HEXBINARY:
                if ((x == null) || (y == null))
                    return -2;
                if (ytype == XML_XSD10_SCHEMAS_HEXBINARY)
                {
                    if (x.hex.total == y.hex.total)
                    {
                        int ret = typesStrcmp(x.hex.str, y.hex.str);
                        if (ret > 0)
                            return 1;
                        else if (ret == 0)
                            return 0;
                    }
                    else if (Integer.compareUnsigned(x.hex.total, y.hex.total) > 0)
                        return 1;

                    return -1;
                }
                return -2;
            case XML_XSD10_SCHEMAS_BASE64BINARY:
                if ((x == null) || (y == null))
                    return -2;
                if (ytype == XML_XSD10_SCHEMAS_BASE64BINARY)
                {
                    if (x.base64.total == y.base64.total)
                    {
                        int ret = typesStrcmp(x.base64.str, y.base64.str);
                        if (ret > 0)
                            return 1;
                        else if (ret == 0)
                            return 0;
                        else
                            return -1;
                    }
                    else if (Integer.compareUnsigned(x.base64.total, y.base64.total) > 0)
                        return 1;
                    else
                        return -1;
                }
                return -2;
            case XML_XSD10_SCHEMAS_IDREFS:
            case XML_XSD10_SCHEMAS_ENTITIES:
            case XML_XSD10_SCHEMAS_NMTOKENS:
                /* TODO */
                break;
            default:
                break;
        }
        return -2;
    }

    // xml_xsd10_compare_values
    /** Port of xmlSchemaCompareValues.
        @param x first value @param y second value
        @return -1 if x &lt; y, 0 if x == y, 1 if x &gt; y, 2 if x &lt;&gt; y, and -2 in case of error */
    public static int compareValues(Xsd10Val x, Xsd10Val y)
    {
        int xws, yws;

        if ((x == null) || (y == null))
            return -2;
        if (x.type == XML_XSD10_SCHEMAS_STRING)
            xws = XML_XSD10_SCHEMA_WHITESPACE_PRESERVE;
        else if (x.type == XML_XSD10_SCHEMAS_NORMSTRING)
            xws = XML_XSD10_SCHEMA_WHITESPACE_REPLACE;
        else
            xws = XML_XSD10_SCHEMA_WHITESPACE_COLLAPSE;

        if (y.type == XML_XSD10_SCHEMAS_STRING)
            yws = XML_XSD10_SCHEMA_WHITESPACE_PRESERVE;
        else if (y.type == XML_XSD10_SCHEMAS_NORMSTRING)
            yws = XML_XSD10_SCHEMA_WHITESPACE_REPLACE;
        else
            yws = XML_XSD10_SCHEMA_WHITESPACE_COLLAPSE;

        return typesCompareValuesInternal(x.type, x, null, xws, y.type, y, null, yws);
    }

    // xml_xsd10_compare_values_whtsp
    /** Port of xmlSchemaCompareValuesWhtsp.
        @param x first value @param xws its whitespace @param y second value @param yws its whitespace
        @return the comparison result as for compareValues */
    public static int compareValuesWhtsp(Xsd10Val x, int xws, Xsd10Val y, int yws)
    {
        if ((x == null) || (y == null))
            return -2;
        return typesCompareValuesInternal(x.type, x, null, xws, y.type, y, null, yws);
    }

    // xsd10_types_compare_values_whtsp_ext
    /* Port of xmlSchemaCompareValuesWhtspExt. */
    private static int typesCompareValuesWhtspExt(int xtype, Xsd10Val x, byte[] xvalue, int xws,
                                                  int ytype, Xsd10Val y, byte[] yvalue, int yws)
    {
        return typesCompareValuesInternal(xtype, x, xvalue, xws, ytype, y, yvalue, yws);
    }

    // xsd10_types_norm_len
    /* Port of xmlSchemaNormLen: computes the UTF8 length of the normalized value of the string.
       Returns the length or -1 in case of error. */
    private static int typesNormLen(byte[] value)
    {
        int utf;
        int ret = 0;

        if (value == null)
            return -1;
        utf = 0;
        while (isBlankCh(at(value, utf))) utf++;
        while (at(value, utf) != 0)
        {
            if ((at(value, utf) & 0x80) != 0)
            {
                if ((at(value, utf + 1) & 0xc0) != 0x80)
                    return -1;
                if ((at(value, utf) & 0xe0) == 0xe0)
                {
                    if ((at(value, utf + 2) & 0xc0) != 0x80)
                        return -1;
                    if ((at(value, utf) & 0xf0) == 0xf0)
                    {
                        if ((at(value, utf) & 0xf8) != 0xf0 || (at(value, utf + 3) & 0xc0) != 0x80)
                            return -1;
                        utf += 4;
                    }
                    else
                    {
                        utf += 3;
                    }
                }
                else
                {
                    utf += 2;
                }
            }
            else if (isBlankCh(at(value, utf)))
            {
                while (isBlankCh(at(value, utf))) utf++;
                if (at(value, utf) == 0)
                    break;
            }
            else
            {
                utf++;
            }
            ret++;
        }
        return ret;
    }

    // xml_xsd10_get_facet_value_as_ulong
    /** Port of xmlSchemaGetFacetValueAsULong: extract the value of a facet (strtoul of the
        decimal string after its sign; the result is an unsigned long).
        @param facet the facet @return the value */
    public static long getFacetValueAsUlong(Xsd10Facet facet)
    {
        /*
         * TODO: Check if this is a decimal.
         */
        if (facet == null || facet.val == null)
            return 0;
        return cStrtoul(facet.val.decimal.str, 1);
    }

    /** C strtoul(s + from, &end, 10): saturates at ULONG_MAX, negates for a '-' sign. */
    private static long cStrtoul(String s, int from)
    {
        int p = from;
        while (dc(s, p) == ' ' || (dc(s, p) >= 0x9 && dc(s, p) <= 0xd)) p++;
        boolean neg = false;
        if (dc(s, p) == '+' || dc(s, p) == '-')
        {
            neg = dc(s, p) == '-';
            p++;
        }
        long r = 0;
        boolean overflow = false;
        while (isDigit(dc(s, p)))
        {
            int d = dc(s, p) - '0';
            if (!overflow)
            {
                if (Long.compareUnsigned(r, Long.divideUnsigned(-1L, 10)) > 0)
                    overflow = true;
                else
                {
                    long n = r * 10 + d;
                    if (Long.compareUnsigned(n, r * 10) < 0)
                        overflow = true;
                    else
                        r = n;
                }
            }
            p++;
        }
        if (overflow) return -1L;
        return neg ? -r : r;
    }

    // xml_xsd10_validate_list_simple_type_facet
    /** Port of xmlSchemaValidateListSimpleTypeFacet: checks the value of a list simple type against
        a facet.
        @param facet the facet @param value the lexical value @param actualLen the number of items
        @param expectedLen out: the expected length (may be null)
        @return 0 if valid, a positive error code otherwise and -1 in case of an internal error */
    public static int validateListSimpleTypeFacet(Xsd10Facet facet, String value, long actualLen, long[] expectedLen)
    {
        if (facet == null)
            return -1;
        /*
         * TODO: Check if this will work with large numbers.
         */
        if (facet.type == XML_XSD10_SCHEMA_FACET_LENGTH)
        {
            if (actualLen != getFacetValueAsUlong(facet))
            {
                if (expectedLen != null)
                    expectedLen[0] = getFacetValueAsUlong(facet);
                return XML_XSD10_SCHEMAV_CVC_LENGTH_VALID;
            }
        }
        else if (facet.type == XML_XSD10_SCHEMA_FACET_MINLENGTH)
        {
            if (Long.compareUnsigned(actualLen, getFacetValueAsUlong(facet)) < 0)
            {
                if (expectedLen != null)
                    expectedLen[0] = getFacetValueAsUlong(facet);
                return XML_XSD10_SCHEMAV_CVC_MINLENGTH_VALID;
            }
        }
        else if (facet.type == XML_XSD10_SCHEMA_FACET_MAXLENGTH)
        {
            if (Long.compareUnsigned(actualLen, getFacetValueAsUlong(facet)) > 0)
            {
                if (expectedLen != null)
                    expectedLen[0] = getFacetValueAsUlong(facet);
                return XML_XSD10_SCHEMAV_CVC_MAXLENGTH_VALID;
            }
        }
        else
            /*
             * NOTE: That we can pass NULL as xmlSchemaVal to
             * xmlSchemaValidateFacet, since the remaining facet types
             * are: XML_SCHEMA_FACET_PATTERN, XML_SCHEMA_FACET_ENUMERATION.
             */
            return validateFacet(null, facet, value, null);
        return 0;
    }

    /** The length of a string value for the length facets (shared code of
        xsd10_types_validate_length_facet_internal and xsd10_types_validate_facet_internal).
        Returns the unsigned int len, or null for QName/NOTATION (return 0) in the length facet. */
    private static int lengthOfValue(int valType, byte[] value, Xsd10Val val, int ws)
    {
        int len = 0;
        if ((val != null) && (val.type == XML_XSD10_SCHEMAS_HEXBINARY))
            len = val.hex.total;
        else if ((val != null) && (val.type == XML_XSD10_SCHEMAS_BASE64BINARY))
            len = val.base64.total;
        else
        {
            switch (valType)
            {
                case XML_XSD10_SCHEMAS_STRING:
                case XML_XSD10_SCHEMAS_NORMSTRING:
                    if (ws == XML_XSD10_SCHEMA_WHITESPACE_UNKNOWN)
                    {
                        /*
                         * This is to ensure API compatibility with the old
                         * xmlSchemaValidateLengthFacet(). Anyway, this was and
                         * is not the correct handling.
                         * TODO: Get rid of this case somehow.
                         */
                        if (valType == XML_XSD10_SCHEMAS_STRING)
                            len = typesUtf8Strlen(value);
                        else
                            len = typesNormLen(value);
                    }
                    else if (value != null)
                    {
                        if (ws == XML_XSD10_SCHEMA_WHITESPACE_COLLAPSE)
                            len = typesNormLen(value);
                        else
                            /*
                             * Should be OK for "preserve" as well.
                             */
                            len = typesUtf8Strlen(value);
                    }
                    break;
                case XML_XSD10_SCHEMAS_IDREF:
                case XML_XSD10_SCHEMAS_TOKEN:
                case XML_XSD10_SCHEMAS_LANGUAGE:
                case XML_XSD10_SCHEMAS_NMTOKEN:
                case XML_XSD10_SCHEMAS_NAME:
                case XML_XSD10_SCHEMAS_NCNAME:
                case XML_XSD10_SCHEMAS_ID:
                    /*
                     * FIXME: What exactly to do with anyURI?
                     */
                case XML_XSD10_SCHEMAS_ANYURI:
                    if (value != null)
                        len = typesNormLen(value);
                    break;
                default:
                    /* TODO */
                    break;
            }
        }
        return len;
    }

    // xsd10_types_validate_length_facet_internal
    /* Port of xmlSchemaValidateLengthFacetInternal: checks a value against a "length", "minLength"
       and "maxLength" facet; sets length to the computed length of value.
       Returns 0 if the value is valid, a positive error code otherwise and -1 in case of an internal
       or API error. */
    private static int typesValidateLengthFacetInternal(Xsd10Facet facet, int valType, byte[] value,
                                                        Xsd10Val val, long[] length, int ws)
    {
        int len = 0;

        if ((length == null) || (facet == null))
            return -1;
        length[0] = 0;
        if ((facet.type != XML_XSD10_SCHEMA_FACET_LENGTH) &&
            (facet.type != XML_XSD10_SCHEMA_FACET_MAXLENGTH) &&
            (facet.type != XML_XSD10_SCHEMA_FACET_MINLENGTH))
            return -1;

        /*
         * TODO: length, maxLength and minLength must be of type
         * nonNegativeInteger only. Check if decimal is used somehow.
         */
        if ((facet.val == null) ||
            ((facet.val.type != XML_XSD10_SCHEMAS_DECIMAL) &&
             (facet.val.type != XML_XSD10_SCHEMAS_NNINTEGER)) ||
            !(typesDecimalIsInteger(facet.val.decimal)))
        {
            return -1;
        }
        if (!((val != null) && ((val.type == XML_XSD10_SCHEMAS_HEXBINARY) || (val.type == XML_XSD10_SCHEMAS_BASE64BINARY))) &&
            ((valType == XML_XSD10_SCHEMAS_QNAME) || (valType == XML_XSD10_SCHEMAS_NOTATION)))
        {
            /*
             * For QName and NOTATION, those facets are
             * deprecated and should be ignored.
             */
            return 0;
        }
        len = lengthOfValue(valType, value, val, ws);
        long ulen = Integer.toUnsignedLong(len);
        length[0] = ulen;
        /*
         * TODO: Return the whole expected value. (This may be possible now with xmlSchemaValDecimalCompareWithInteger)
         */
        if (facet.type == XML_XSD10_SCHEMA_FACET_LENGTH)
        {
            if (ulen != getFacetValueAsUlong(facet))
                return XML_XSD10_SCHEMAV_CVC_LENGTH_VALID;
        }
        else if (facet.type == XML_XSD10_SCHEMA_FACET_MINLENGTH)
        {
            if (Long.compareUnsigned(ulen, getFacetValueAsUlong(facet)) < 0)
                return XML_XSD10_SCHEMAV_CVC_MINLENGTH_VALID;
        }
        else
        {
            if (Long.compareUnsigned(ulen, getFacetValueAsUlong(facet)) > 0)
                return XML_XSD10_SCHEMAV_CVC_MAXLENGTH_VALID;
        }

        return 0;
    }

    // xml_xsd10_validate_length_facet
    /** Port of xmlSchemaValidateLengthFacet.
        @param type the built in type @param facet the facet @param value the value
        @param val the computed value @param length out: the computed length
        @return 0 if valid, a positive error code otherwise, -1 on error */
    public static int validateLengthFacet(Xsd10Type type, Xsd10Facet facet, String value, Xsd10Val val, long[] length)
    {
        if (type == null)
            return -1;
        return typesValidateLengthFacetInternal(facet, type.builtInType, b(value), val, length,
            XML_XSD10_SCHEMA_WHITESPACE_UNKNOWN);
    }

    // xml_xsd10_validate_length_facet_whtsp
    /** Port of xmlSchemaValidateLengthFacetWhtsp.
        @param facet the facet @param valType the value type @param value the value
        @param val the computed value @param length out: the computed length @param ws the whitespace
        @return 0 if valid, a positive error code otherwise, -1 on error */
    public static int validateLengthFacetWhtsp(Xsd10Facet facet, int valType, String value, Xsd10Val val, long[] length, int ws)
    {
        return typesValidateLengthFacetInternal(facet, valType, b(value), val, length, ws);
    }

    // xsd10_types_validate_facet_internal
    /* Port of xmlSchemaValidateFacetInternal: check a value against a facet condition.
       Returns 0 if the element is schemas valid, a positive error code
       number otherwise and -1 in case of internal or API error. */
    private static int typesValidateFacetInternal(Xsd10Facet facet, int fws, int valType, String value,
                                                  Xsd10Val val, int ws)
    {
        int ret;

        if (facet == null)
            return -1;

        switch (facet.type)
        {
            case XML_XSD10_SCHEMA_FACET_PATTERN:
                /*
                 * NOTE that for patterns, the @value needs to be the normalized
                 * value, *not* the lexical initial value or the canonical value.
                 */
                if (value == null)
                    return -1;
                /*
                 * If string-derived type, regexp must be tested on the value space of
                 * the datatype.
                 * See https://www.w3.org/TR/xmlschema-2/#rf-pattern
                 */
                if (val != null &&
                    val.str != null &&
                    ((val.type >= XML_XSD10_SCHEMAS_STRING &&
                      val.type <= XML_XSD10_SCHEMAS_NORMSTRING) ||
                     (val.type >= XML_XSD10_SCHEMAS_TOKEN &&
                      val.type <= XML_XSD10_SCHEMAS_ENTITIES &&
                      val.type != XML_XSD10_SCHEMAS_QNAME)))
                {
                    value = val.str;
                }
                ret = typesRegexpExec(facet.regexp, value);
                if (ret == 1)
                    return 0;
                if (ret == 0)
                    return XML_XSD10_SCHEMAV_CVC_PATTERN_VALID;
                return ret;
            case XML_XSD10_SCHEMA_FACET_MAXEXCLUSIVE:
                ret = compareValues(val, facet.val);
                if (ret == -2)
                    return -1;
                if (ret == -1)
                    return 0;
                return XML_XSD10_SCHEMAV_CVC_MAXEXCLUSIVE_VALID;
            case XML_XSD10_SCHEMA_FACET_MAXINCLUSIVE:
                ret = compareValues(val, facet.val);
                if (ret == -2)
                    return -1;
                if ((ret == -1) || (ret == 0))
                    return 0;
                return XML_XSD10_SCHEMAV_CVC_MAXINCLUSIVE_VALID;
            case XML_XSD10_SCHEMA_FACET_MINEXCLUSIVE:
                ret = compareValues(val, facet.val);
                if (ret == -2)
                    return -1;
                if (ret == 1)
                    return 0;
                return XML_XSD10_SCHEMAV_CVC_MINEXCLUSIVE_VALID;
            case XML_XSD10_SCHEMA_FACET_MININCLUSIVE:
                ret = compareValues(val, facet.val);
                if (ret == -2)
                    return -1;
                if ((ret == 1) || (ret == 0))
                    return 0;
                return XML_XSD10_SCHEMAV_CVC_MININCLUSIVE_VALID;
            case XML_XSD10_SCHEMA_FACET_WHITESPACE:
                /* TODO whitespaces */
                /*
                 * NOTE: Whitespace should be handled to normalize
                 * the value to be validated against a the facets;
                 * not to normalize the value in-between.
                 */
                return 0;
            case XML_XSD10_SCHEMA_FACET_ENUMERATION:
                if (ws == XML_XSD10_SCHEMA_WHITESPACE_UNKNOWN)
                {
                    /*
                     * This is to ensure API compatibility with the old
                     * xmlSchemaValidateFacet().
                     * TODO: Get rid of this case.
                     */
                    if ((facet.value != null) &&
                        (typesStrEqual(b(facet.value), b(value)) != 0))
                        return 0;
                }
                else
                {
                    if (facet.val == null)
                        return -1; /* C would dereference NULL here */
                    ret = typesCompareValuesWhtspExt(facet.val.type,
                        facet.val, b(facet.value), fws, valType, val,
                        b(value), ws);
                    if (ret == -2)
                        return -1;
                    if (ret == 0)
                        return 0;
                }
                return XML_XSD10_SCHEMAV_CVC_ENUMERATION_VALID;
            case XML_XSD10_SCHEMA_FACET_LENGTH:
            case XML_XSD10_SCHEMA_FACET_MAXLENGTH:
            case XML_XSD10_SCHEMA_FACET_MINLENGTH:
            {
                /*
                 * SPEC (1.3) "if {primitive type definition} is QName or NOTATION,
                 * then any {value} is facet-valid."
                 */
                int len = 0;

                if ((valType == XML_XSD10_SCHEMAS_QNAME) ||
                    (valType == XML_XSD10_SCHEMAS_NOTATION))
                    return 0;
                /*
                 * TODO: length, maxLength and minLength must be of type
                 * nonNegativeInteger only. Check if decimal is used somehow.
                 */
                if ((facet.val == null) ||
                    ((facet.val.type != XML_XSD10_SCHEMAS_DECIMAL) &&
                     (facet.val.type != XML_XSD10_SCHEMAS_NNINTEGER)) ||
                    !typesDecimalIsInteger(facet.val.decimal))
                {
                    return -1;
                }
                len = lengthOfValue(valType, b(value), val, ws);
                long ulen = Integer.toUnsignedLong(len);
                if (facet.type == XML_XSD10_SCHEMA_FACET_LENGTH)
                {
                    if (ulen != getFacetValueAsUlong(facet))
                        return XML_XSD10_SCHEMAV_CVC_LENGTH_VALID;
                }
                else if (facet.type == XML_XSD10_SCHEMA_FACET_MINLENGTH)
                {
                    if (Long.compareUnsigned(ulen, getFacetValueAsUlong(facet)) < 0)
                        return XML_XSD10_SCHEMAV_CVC_MINLENGTH_VALID;
                }
                else if (Long.compareUnsigned(ulen, getFacetValueAsUlong(facet)) > 0)
                {
                    return XML_XSD10_SCHEMAV_CVC_MAXLENGTH_VALID;
                }
                break;
            }
            case XML_XSD10_SCHEMA_FACET_TOTALDIGITS:
            case XML_XSD10_SCHEMA_FACET_FRACTIONDIGITS:

                if ((facet.val == null) ||
                    ((facet.val.type != XML_XSD10_SCHEMAS_PINTEGER) &&
                     (facet.val.type != XML_XSD10_SCHEMAS_NNINTEGER)) ||
                    !typesDecimalIsInteger(facet.val.decimal))
                {
                    return -1;
                }
                if ((val == null) ||
                    ((val.type != XML_XSD10_SCHEMAS_DECIMAL) &&
                     (val.type != XML_XSD10_SCHEMAS_INTEGER) &&
                     (val.type != XML_XSD10_SCHEMAS_NPINTEGER) &&
                     (val.type != XML_XSD10_SCHEMAS_NINTEGER) &&
                     (val.type != XML_XSD10_SCHEMAS_NNINTEGER) &&
                     (val.type != XML_XSD10_SCHEMAS_PINTEGER) &&
                     (val.type != XML_XSD10_SCHEMAS_INT) &&
                     (val.type != XML_XSD10_SCHEMAS_UINT) &&
                     (val.type != XML_XSD10_SCHEMAS_LONG) &&
                     (val.type != XML_XSD10_SCHEMAS_ULONG) &&
                     (val.type != XML_XSD10_SCHEMAS_SHORT) &&
                     (val.type != XML_XSD10_SCHEMAS_USHORT) &&
                     (val.type != XML_XSD10_SCHEMAS_BYTE) &&
                     (val.type != XML_XSD10_SCHEMAS_UBYTE)))
                {
                    return -1;
                }
                if (facet.type == XML_XSD10_SCHEMA_FACET_TOTALDIGITS)
                {
                    if (Long.compareUnsigned(typesDecimalGetSignificantDigitCount(val.decimal), getFacetValueAsUlong(facet)) > 0)
                        return XML_XSD10_SCHEMAV_CVC_TOTALDIGITS_VALID;
                }
                else if (facet.type == XML_XSD10_SCHEMA_FACET_FRACTIONDIGITS)
                {
                    long fd = typesDecimalIsInteger(val.decimal) ? 0 : Integer.toUnsignedLong(val.decimal.fractionalPlaces);
                    if (Long.compareUnsigned(fd, getFacetValueAsUlong(facet)) > 0)
                        return XML_XSD10_SCHEMAV_CVC_FRACTIONDIGITS_VALID;
                }
                break;
            default:
                /* TODO */
                break;
        }
        return 0;
    }

    // xml_xsd10_validate_facet
    /** Port of xmlSchemaValidateFacet: check a value against a facet condition.
        @param base the base type @param facet the facet @param value the lexical value
        @param val the computed value
        @return 0 if valid, a positive error code otherwise and -1 in case of internal or API error */
    public static int validateFacet(Xsd10Type base, Xsd10Facet facet, String value, Xsd10Val val)
    {
        /*
         * This tries to ensure API compatibility regarding the old
         * xmlSchemaValidateFacet() and the new xmlSchemaValidateFacetInternal() and
         * xmlSchemaValidateFacetWhtsp().
         */
        if (val != null)
            return typesValidateFacetInternal(facet,
                XML_XSD10_SCHEMA_WHITESPACE_UNKNOWN, val.type, value, val,
                XML_XSD10_SCHEMA_WHITESPACE_UNKNOWN);
        else if (base != null)
            return typesValidateFacetInternal(facet,
                XML_XSD10_SCHEMA_WHITESPACE_UNKNOWN, base.builtInType, value, val,
                XML_XSD10_SCHEMA_WHITESPACE_UNKNOWN);
        return -1;
    }

    // xml_xsd10_validate_facet_whtsp
    /** Port of xmlSchemaValidateFacetWhtsp: check a value against a facet condition, taking the
        whitespace types into account. value needs to be the normalized value for patterns.
        @param facet the facet @param fws the facet whitespace @param valType the value type
        @param value the lexical value @param val the computed value @param ws the value whitespace
        @return 0 if valid, a positive error code otherwise and -1 in case of internal or API error */
    public static int validateFacetWhtsp(Xsd10Facet facet, int fws, int valType, String value, Xsd10Val val, int ws)
    {
        return typesValidateFacetInternal(facet, fws, valType, value, val, ws);
    }

    // printf helpers (glibc formatting of the canonical values) //////////////////

    /** snprintf(buf, size, ...) truncation of an ASCII result. */
    private static String trunc(String s, int size)
    {
        return s.length() > size - 1 ? s.substring(0, size - 1) : s;
    }

    /** printf zero padding to width (after a sign). */
    private static String zpad(String s, int width)
    {
        if (s.length() >= width) return s;
        int signLen = (s.startsWith("-") || s.startsWith("+")) ? 1 : 0;
        StringBuilder sb = new StringBuilder();
        sb.append(s, 0, signLen);
        for (int i = s.length(); i < width; i++) sb.append('0');
        sb.append(s, signLen, s.length());
        return sb.toString();
    }

    /** %lu of an unsigned long. */
    private static String ul(long v)
    {
        return Long.toUnsignedString(v);
    }

    /** C (unsigned long) of a non negative double. */
    private static long d2ul(double d)
    {
        if (!(d >= 0)) return (long) d;
        if (d < 9.223372036854775807E18) return (long) d;
        if (d >= 1.8446744073709551615E19) return -1L;
        return (long) (d - 9.223372036854775808E18) + Long.MIN_VALUE;
    }

    /** The decimal digits (prec+1 significant, exactly rounded half even like glibc) and the
        decimal exponent of a finite non zero |d|. */
    private static Object[] eDigits(double a, int prec)
    {
        BigDecimal r = new BigDecimal(a).round(new MathContext(prec + 1, RoundingMode.HALF_EVEN));
        String digits = r.unscaledValue().toString();
        int exp = digits.length() - 1 - r.scale();
        StringBuilder sb = new StringBuilder(digits);
        while (sb.length() < prec + 1) sb.append('0');
        if (sb.length() > prec + 1) sb.setLength(prec + 1);
        return new Object[] { sb.toString(), exp };
    }

    private static String special(double d)
    {
        boolean neg = Double.doubleToRawLongBits(d) < 0;
        if (Double.isNaN(d)) return neg ? "-nan" : "nan";
        return neg ? "-inf" : "inf";
    }

    private static String expStr(int exp)
    {
        int ae = Math.abs(exp);
        return "e" + (exp < 0 ? "-" : "+") + (ae < 10 ? "0" : "") + ae;
    }

    /** C printf %.{prec}e. */
    private static String fmtE(double d, int prec)
    {
        if (Double.isNaN(d) || Double.isInfinite(d)) return special(d);
        boolean neg = Double.doubleToRawLongBits(d) < 0;
        double a = Math.abs(d);
        String digits;
        int exp;
        if (a == 0)
        {
            StringBuilder sb = new StringBuilder();
            for (int i = 0; i <= prec; i++) sb.append('0');
            digits = sb.toString();
            exp = 0;
        }
        else
        {
            Object[] e = eDigits(a, prec);
            digits = (String) e[0];
            exp = (Integer) e[1];
        }
        StringBuilder out = new StringBuilder();
        if (neg) out.append('-');
        out.append(digits.charAt(0));
        if (prec > 0) out.append('.').append(digits, 1, digits.length());
        out.append(expStr(exp));
        return out.toString();
    }

    private static String stripTrailingZeros(String s)
    {
        if (s.indexOf('.') < 0) return s;
        int e = s.length();
        while (s.charAt(e - 1) == '0') e--;
        if (s.charAt(e - 1) == '.') e--;
        return s.substring(0, e);
    }

    /** C printf %.{prec}g. */
    private static String fmtG(double d, int prec)
    {
        if (Double.isNaN(d) || Double.isInfinite(d)) return special(d);
        if (prec == 0) prec = 1;
        boolean neg = Double.doubleToRawLongBits(d) < 0;
        double a = Math.abs(d);
        int x = 0;
        if (a != 0)
            x = (Integer) eDigits(a, prec - 1)[1];
        String body;
        if (prec > x && x >= -4)
        {
            body = new BigDecimal(a).setScale(prec - 1 - x, RoundingMode.HALF_EVEN).toPlainString();
            body = stripTrailingZeros(body);
        }
        else
        {
            String e = fmtE(a, prec - 1);
            int ei = e.indexOf('e');
            body = stripTrailingZeros(e.substring(0, ei)) + e.substring(ei);
        }
        return (neg ? "-" : "") + body;
    }

    /** %02u */
    private static String u2(long v)
    {
        return zpad(Long.toString(v), 2);
    }

    /** %04ld */
    private static String ld4(long v)
    {
        return zpad(Long.toString(v), 4);
    }

    // xsd10_types_get_canon_value
    /* Port of xmlSchemaGetCanonValue: get the canonical lexical representation of the value.
       WARNING: Some value types are not supported yet, resulting in a retValue of "???".
       Returns 0 if the value could be built, 1 if the value type is
       not supported yet and -1 in case of API errors. */
    private static int typesGetCanonValue(Xsd10Val val, String[] retValue)
    {
        if ((retValue == null) || (val == null))
            return -1;
        retValue[0] = null;
        switch (val.type)
        {
            case XML_XSD10_SCHEMAS_STRING:
                if (val.str == null)
                    retValue[0] = "";
                else
                    retValue[0] = val.str;
                break;
            case XML_XSD10_SCHEMAS_NORMSTRING:
                if (val.str == null)
                    retValue[0] = "";
                else
                {
                    retValue[0] = whiteSpaceReplace(val.str);
                    if ((retValue[0]) == null)
                        retValue[0] = val.str;
                }
                break;
            case XML_XSD10_SCHEMAS_TOKEN:
            case XML_XSD10_SCHEMAS_LANGUAGE:
            case XML_XSD10_SCHEMAS_NMTOKEN:
            case XML_XSD10_SCHEMAS_NAME:
            case XML_XSD10_SCHEMAS_NCNAME:
            case XML_XSD10_SCHEMAS_ID:
            case XML_XSD10_SCHEMAS_IDREF:
            case XML_XSD10_SCHEMAS_ENTITY:
            case XML_XSD10_SCHEMAS_NOTATION: /* Unclear */
            case XML_XSD10_SCHEMAS_ANYURI:   /* Unclear */
            {
                String s = unionStr(val);
                if (s == null)
                    return -1;
                retValue[0] = collapseString(s);
                if (retValue[0] == null)
                    retValue[0] = s;
                break;
            }
            case XML_XSD10_SCHEMAS_QNAME:
                /* TODO: Unclear in XML Schema 1.0. */
                if (val.qname.uri == null)
                {
                    retValue[0] = val.qname.name;
                    return 0;
                }
                else
                {
                    /* Like libxml2, the uri is appended twice ("{uri}uri"). */
                    retValue[0] = "{" + val.qname.uri + "}" + val.qname.uri;
                }
                break;
            case XML_XSD10_SCHEMAS_DECIMAL:
            {
                String start = val.decimal.str;
                if (start == null)
                    return -1;
                if (dc(start, 0) == '+')
                {
                    start = start.substring(1);
                }
                retValue[0] = start;
                break;
            }
            case XML_XSD10_SCHEMAS_INTEGER:
            case XML_XSD10_SCHEMAS_PINTEGER:
            case XML_XSD10_SCHEMAS_NPINTEGER:
            case XML_XSD10_SCHEMAS_NINTEGER:
            case XML_XSD10_SCHEMAS_NNINTEGER:
            case XML_XSD10_SCHEMAS_LONG:
            case XML_XSD10_SCHEMAS_BYTE:
            case XML_XSD10_SCHEMAS_SHORT:
            case XML_XSD10_SCHEMAS_INT:
            case XML_XSD10_SCHEMAS_UINT:
            case XML_XSD10_SCHEMAS_ULONG:
            case XML_XSD10_SCHEMAS_USHORT:
            case XML_XSD10_SCHEMAS_UBYTE:
            {
                String start = val.decimal.str;
                if (start == null)
                    return -1;
                /* 2 = sign+NULL */
                long bufSize = Integer.toUnsignedLong(val.decimal.integralPlaces) + 2;
                if (dc(start, 0) == '+')
                {
                    start = start.substring(1);
                    bufSize -= 1;
                }
                retValue[0] = start.substring(0, (int) Math.min(start.length(), bufSize - 1));
                break;
            }
            case XML_XSD10_SCHEMAS_BOOLEAN:
                if (val.b != 0)
                    retValue[0] = "true";
                else
                    retValue[0] = "false";
                break;
            case XML_XSD10_SCHEMAS_DURATION:
            {
                long year;
                long mon, day, hour = 0, min = 0;
                double sec = 0, left;

                /* TODO: Unclear in XML Schema 1.0 */
                year = d2ul(fquotient(Math.abs(val.dur.mon), 12));
                mon = Math.abs(val.dur.mon) - 12 * year;

                day = d2ul(fquotient(Math.abs(val.dur.sec), 86400));
                left = Math.abs(val.dur.sec) - udouble(day) * 86400;
                if (left > 0)
                {
                    hour = d2ul(fquotient(left, 3600));
                    left = left - udouble(hour * 3600);
                    if (left > 0)
                    {
                        min = d2ul(fquotient(left, 60));
                        sec = left - udouble(min * 60);
                    }
                }
                /* Like libxml2: the sign is inverted and the days of the duration are not used. */
                String buf;
                if ((val.dur.mon < 0) || (val.dur.sec < 0))
                    buf = "P" + ul(year) + "Y" + ul(mon) + "M" + ul(day) + "DT" + ul(hour) + "H" + ul(min) + "M" + fmtG(sec, 14) + "S";
                else
                    buf = "-P" + ul(year) + "Y" + ul(mon) + "M" + ul(day) + "DT" + ul(hour) + "H" + ul(min) + "M" + fmtG(sec, 14) + "S";
                retValue[0] = trunc(buf, 100);
                break;
            }
            case XML_XSD10_SCHEMAS_GYEAR:
                /* TODO: Unclear in XML Schema 1.0 */
                /* TODO: What to do with the timezone? */
                retValue[0] = trunc(ld4(val.date.year), 30);
                break;
            case XML_XSD10_SCHEMAS_GMONTH:
                /* TODO: Unclear in XML Schema 1.0 */
                /* TODO: What to do with the timezone? */
                retValue[0] = trunc("--" + u2(val.date.mon), 6);
                break;
            case XML_XSD10_SCHEMAS_GDAY:
                /* TODO: Unclear in XML Schema 1.0 */
                /* TODO: What to do with the timezone? */
                retValue[0] = trunc("---" + u2(val.date.day), 6);
                break;
            case XML_XSD10_SCHEMAS_GMONTHDAY:
                /* TODO: Unclear in XML Schema 1.0 */
                /* TODO: What to do with the timezone? */
                retValue[0] = trunc("--" + u2(val.date.mon) + "-" + u2(val.date.day), 8);
                break;
            case XML_XSD10_SCHEMAS_GYEARMONTH:
            {
                /* TODO: Unclear in XML Schema 1.0 */
                /* TODO: What to do with the timezone? */
                String buf;
                if (val.date.year < 0)
                    buf = "-" + ld4(Math.abs(val.date.year)) + "-" + u2(val.date.mon);
                else
                    buf = ld4(val.date.year) + "-" + u2(val.date.mon);
                retValue[0] = trunc(buf, 35);
                break;
            }
            case XML_XSD10_SCHEMAS_TIME:
            {
                String buf;

                if (val.date.tz_flag != 0)
                {
                    Xsd10Val norm;

                    norm = typesDateNormalize(val, 0);
                    if (norm == null)
                        return -1;
                    /*
                     * TODO: Check if "%.14g" is portable.
                     */
                    buf = u2(norm.date.hour) + ":" + u2(norm.date.min) + ":" + zpad(fmtG(norm.date.sec, 14), 2) + "Z";
                }
                else
                {
                    buf = u2(val.date.hour) + ":" + u2(val.date.min) + ":" + zpad(fmtG(val.date.sec, 14), 2);
                }
                retValue[0] = trunc(buf, 30);
                break;
            }
            case XML_XSD10_SCHEMAS_DATE:
            {
                String buf;

                if (val.date.tz_flag != 0)
                {
                    Xsd10Val norm;

                    norm = typesDateNormalize(val, 0);
                    if (norm == null)
                        return -1;
                    /*
                     * TODO: Append the canonical value of the
                     * recoverable timezone and not "Z".
                     */
                    buf = ld4(norm.date.year) + "-" + u2(norm.date.mon) + "-" + u2(norm.date.day) + "Z";
                }
                else
                {
                    buf = ld4(val.date.year) + "-" + u2(val.date.mon) + "-" + u2(val.date.day);
                }
                retValue[0] = trunc(buf, 30);
                break;
            }
            case XML_XSD10_SCHEMAS_DATETIME:
            {
                String buf;

                if (val.date.tz_flag != 0)
                {
                    Xsd10Val norm;

                    norm = typesDateNormalize(val, 0);
                    if (norm == null)
                        return -1;
                    /*
                     * TODO: Check if "%.14g" is portable.
                     */
                    buf = ld4(norm.date.year) + "-" + u2(norm.date.mon) + "-" + u2(norm.date.day) + "T" +
                        u2(norm.date.hour) + ":" + u2(norm.date.min) + ":" + zpad(fmtG(norm.date.sec, 14), 2) + "Z";
                }
                else
                {
                    buf = ld4(val.date.year) + "-" + u2(val.date.mon) + "-" + u2(val.date.day) + "T" +
                        u2(val.date.hour) + ":" + u2(val.date.min) + ":" + zpad(fmtG(val.date.sec, 14), 2);
                }
                retValue[0] = trunc(buf, 50);
                break;
            }
            case XML_XSD10_SCHEMAS_HEXBINARY:
                retValue[0] = val.hex.str;
                break;
            case XML_XSD10_SCHEMAS_BASE64BINARY:
                /*
                 * TODO: Is the following spec piece implemented?:
                 * SPEC: "Note: For some values the canonical form defined
                 * above does not conform to [RFC 2045], which requires breaking
                 * with linefeeds at appropriate intervals."
                 */
                retValue[0] = val.base64.str;
                break;
            case XML_XSD10_SCHEMAS_FLOAT:
                /*
                 * |m| < 16777216, -149 <= e <= 104.
                 * TODO: Handle, NaN, INF, -INF. The format is not
                 * yet conformant. The c type float does not cover
                 * the whole range.
                 */
                retValue[0] = trunc(fmtE(val.f, 14), 30);
                break;
            case XML_XSD10_SCHEMAS_DOUBLE:
                /* |m| < 9007199254740992, -1075 <= e <= 970 */
                retValue[0] = trunc(fmtE(val.d, 14), 40);
                break;
            default:
                retValue[0] = "???";
                return 1;
        }
        if (retValue[0] == null)
            return -1;
        return 0;
    }

    /** C conversion of an unsigned long to double. */
    private static double udouble(long v)
    {
        if (v >= 0) return (double) v;
        return ((double) (v >>> 1)) * 2.0 + (v & 1);
    }

    // xml_xsd10_get_canon_value
    /** Port of xmlSchemaGetCanonValue.
        @param val the value @param retValue out: the canonical value
        @return 0 if built, 1 if the type is not supported yet and -1 in case of API errors */
    public static int getCanonValue(Xsd10Val val, String[] retValue)
    {
        String[] r = { null };
        int ret;

        if (retValue == null)
            return -1;
        ret = typesGetCanonValue(val, r);
        if (val != null)
            retValue[0] = r[0];
        return ret;
    }

    // xml_xsd10_get_canon_value_whtsp
    /** Port of xmlSchemaGetCanonValueWhtsp: get the canonical representation of the value.
        @param val the value @param retValue out: the canonical value @param ws the whitespace
        @return 0 if built, 1 if the type is not supported yet and -1 in case of API errors */
    public static int getCanonValueWhtsp(Xsd10Val val, String[] retValue, int ws)
    {
        String r = null;

        if ((retValue == null) || (val == null))
            return -1;
        if ((ws == XML_XSD10_SCHEMA_WHITESPACE_UNKNOWN) ||
            (ws > XML_XSD10_SCHEMA_WHITESPACE_COLLAPSE))
            return -1;

        retValue[0] = null;
        switch (val.type)
        {
            case XML_XSD10_SCHEMAS_STRING:
                if (val.str == null)
                    r = "";
                else if (ws == XML_XSD10_SCHEMA_WHITESPACE_COLLAPSE)
                    r = collapseString(val.str);
                else if (ws == XML_XSD10_SCHEMA_WHITESPACE_REPLACE)
                    r = whiteSpaceReplace(val.str);
                if (r == null)
                    r = val.str;
                break;
            case XML_XSD10_SCHEMAS_NORMSTRING:
                if (val.str == null)
                    r = "";
                else
                {
                    if (ws == XML_XSD10_SCHEMA_WHITESPACE_COLLAPSE)
                        r = collapseString(val.str);
                    else
                        r = whiteSpaceReplace(val.str);
                    if (r == null)
                        r = val.str;
                }
                break;
            default:
                return getCanonValue(val, retValue);
        }
        retValue[0] = r;
        return 0;
    }

    // xml_xsd10_get_val_type
    /** Port of xmlSchemaGetValType: accessor for the type of a value.
        @param val the value @return the type, XML_XSD10_SCHEMAS_UNKNOWN for null */
    public static int getValType(Xsd10Val val)
    {
        if (val == null)
            return XML_XSD10_SCHEMAS_UNKNOWN;
        return val.type;
    }

    // Facets (libxml2 xmlschemas.c) ////////////////////////////////////////////////

    // xml_xsd10_new_facet
    /** Port of xmlSchemaNewFacet: allocate a new Facet structure.
        @return the facet */
    public static Xsd10Facet newFacet()
    {
        return new Xsd10Facet();
    }

    // xsd10_types_free_annot
    /* Port of xmlSchemaFreeAnnot (nothing to free in Java). */
    private static void typesFreeAnnot(Xsd10Annot annot)
    {
    }

    // xml_xsd10_free_facet
    /** Port of xmlSchemaFreeFacet: deallocate a Schema Facet structure (frees the regexp
        through the hooks).
        @param facet the facet */
    public static void freeFacet(Xsd10Facet facet)
    {
        if (facet == null)
            return;
        if (facet.val != null)
            freeValue(facet.val);
        if (facet.regexp != null)
            typesRegexpFree(facet.regexp);
        if (facet.annot != null)
            typesFreeAnnot(facet.annot);
    }

    // xsd10_types_builtin_white_space
    /* Port of xmlSchemaGetWhiteSpaceFacetValue for built in types. */
    private static int typesBuiltinWhiteSpace(Xsd10Type type)
    {
        /*
         * Note that we assume a whitespace of preserve for anySimpleType.
         */
        if ((type.builtInType == XML_XSD10_SCHEMAS_STRING) ||
            (type.builtInType == XML_XSD10_SCHEMAS_ANYSIMPLETYPE))
            return XML_XSD10_SCHEMA_WHITESPACE_PRESERVE;
        else if (type.builtInType == XML_XSD10_SCHEMAS_NORMSTRING)
            return XML_XSD10_SCHEMA_WHITESPACE_REPLACE;
        /*
         * For all `atomic` datatypes other than string (and types `derived`
         * by `restriction` from it) the value of whiteSpace is fixed to
         * collapse
         * Note that this includes built-in list datatypes.
         */
        return XML_XSD10_SCHEMA_WHITESPACE_COLLAPSE;
    }

    // xsd10_types_validate_notation
    /* Port of xmlSchemaValidateNotation (called with a parser context): the notations of the schema
       are not available here, so a valid QName is never a declared notation. */
    private static int typesValidateNotation(byte[] value)
    {
        int ret;

        ret = typesValidateQname(value, 1);
        if (ret != 0)
            return ret;
        return 1;
    }

    // xsd10_types_check_cvc_simple_type
    /* Port of xmlSchemaVCheckCVCSimpleType (cvc-simple-type) for built in types, as called by
       xmlSchemaCheckFacet with a parser context and without error reporting. */
    private static int typesCheckCvcSimpleType(XmlNode node, Xsd10Type type, byte[] value,
                                               Xsd10Val[] retVal, int normalize, int isNormalized)
    {
        int ret = 0, valNeeded = (retVal != null) ? 1 : 0;
        Xsd10Val val = null;
        Xsd10Val[] valOut = { null };

        if ((retVal != null) && (retVal[0] != null))
        {
            freeValue(retVal[0]);
            retVal[0] = null;
        }
        if (type == null)
            return -1; /* C would crash */
        /* User derived types are checked by the schema part. */
        if (type.type != XML_XSD10_SCHEMA_TYPE_BASIC)
            return -1;
        if ((valNeeded == 0) && ((type.flags & XML_XSD10_SCHEMAS_TYPE_FACETSNEEDVALUE) != 0))
            valNeeded = 1;
        if (value == null)
            value = new byte[1];
        if ((type.builtInType == XML_XSD10_SCHEMAS_ANYSIMPLETYPE) ||
            ((type.flags & XML_XSD10_SCHEMAS_TYPE_VARIETY_ATOMIC) != 0))
        {
            Xsd10Type biType; /* The built-in type. */
            /*
             * SPEC (1.2.1) "if {variety} is `atomic` then the string must `match`
             * a literal in the `lexical space` of {base type definition}"
             */
            /*
             * Whitespace-normalize.
             */
            if ((isNormalized == 0) &&
                (normalize != 0 || ((type.flags & XML_XSD10_SCHEMAS_TYPE_NORMVALUENEEDED) != 0)))
            {
                byte[] normValue = null;
                switch (typesBuiltinWhiteSpace(type))
                {
                    case XML_XSD10_SCHEMA_WHITESPACE_COLLAPSE:
                        normValue = typesCollapseString(value); break;
                    case XML_XSD10_SCHEMA_WHITESPACE_REPLACE:
                        normValue = typesWhiteSpaceReplace(value); break;
                    default: break;
                }
                if (normValue != null)
                    value = normValue;
                isNormalized = 1;
            }
            biType = type;
            switch (biType.builtInType)
            {
                case XML_XSD10_SCHEMAS_NOTATION:
                    ret = typesValidateNotation(value);
                    break;
                default:
                    if (valNeeded != 0)
                    {
                        ret = typesValAtomicType(biType, value, valOut, node, 1,
                            XML_XSD10_SCHEMA_WHITESPACE_UNKNOWN, 1, 0, 1, null);
                        val = valOut[0];
                    }
                    else
                        ret = typesValAtomicType(biType, value, null, node, 1,
                            XML_XSD10_SCHEMA_WHITESPACE_UNKNOWN, 1, 0, 1, null);
                    break;
            }
            if (ret != 0)
            {
                if (ret < 0)
                    return -1;
                if ((type.flags & XML_XSD10_SCHEMAS_TYPE_VARIETY_LIST) != 0)
                    ret = XML_XSD10_SCHEMAV_CVC_DATATYPE_VALID_1_2_2;
                else
                    ret = XML_XSD10_SCHEMAV_CVC_DATATYPE_VALID_1_2_1;
            }
            /* The facets of built in types are not checked (xmlSchemaValidateFacets). */
        }
        else if ((type.flags & XML_XSD10_SCHEMAS_TYPE_VARIETY_LIST) != 0)
        {
            Xsd10Type itemType;
            int cur, end;
            byte[] tmpValue;
            Xsd10Val prevVal = null;
            Xsd10Val[] curVal = { null };
            /* 1.2.2 if {variety} is `list` then the string must be a sequence
             * of white space separated tokens, each of which `match`es a literal
             * in the `lexical space` of {item type definition}
             */
            if ((isNormalized == 0) &&
                (normalize != 0 || ((type.flags & XML_XSD10_SCHEMAS_TYPE_NORMVALUENEEDED) != 0)))
            {
                byte[] normValue = null;
                switch (typesBuiltinWhiteSpace(type))
                {
                    case XML_XSD10_SCHEMA_WHITESPACE_COLLAPSE:
                        normValue = typesCollapseString(value); break;
                    case XML_XSD10_SCHEMA_WHITESPACE_REPLACE:
                        normValue = typesWhiteSpaceReplace(value); break;
                    default: break;
                }
                if (normValue != null)
                    value = normValue;
                isNormalized = 1;
            }
            itemType = (type.subtypes instanceof Xsd10Type) ? (Xsd10Type) type.subtypes : null;
            cur = 0;
            do
            {
                while (isBlankCh(at(value, cur)))
                    cur++;
                end = cur;
                while ((at(value, end) != 0) && (!(isBlankCh(at(value, end)))))
                    end++;
                if (end == cur)
                    break;
                tmpValue = cbytes(value, cur, end - cur);

                if (valNeeded != 0)
                    ret = typesCheckCvcSimpleType(node, itemType, tmpValue, curVal, 0, 1);
                else
                    ret = typesCheckCvcSimpleType(node, itemType, tmpValue, null, 0, 1);
                if (curVal[0] != null)
                {
                    /*
                     * Add to list of computed values.
                     */
                    if (val == null)
                        val = curVal[0];
                    else
                        valueAppend(prevVal, curVal[0]);
                    prevVal = curVal[0];
                    curVal[0] = null;
                }
                if (ret != 0)
                {
                    if (ret < 0)
                        return -1;
                    ret = XML_XSD10_SCHEMAV_CVC_DATATYPE_VALID_1_2_2;
                    break;
                }
                cur = end;
            } while (at(value, cur) != 0);
            /* The facets of built in types are not checked (xmlSchemaValidateFacets). */
        }

        if (ret == 0)
        {
            if (retVal != null)
                retVal[0] = val;
        }
        return ret;
    }

    // xml_xsd10_check_facet
    /** Port of xmlSchemaCheckFacet (without a parser context, as used by RelaxNG): checks and
        computes the values of facets.
        @param facet the facet @param typeDecl the type holding the facet @param name unused
        @return 0 if valid, a positive error code if not valid and -1 in case of an internal or API error */
    public static int checkFacet(Xsd10Facet facet, Xsd10Type typeDecl, String name)
    {
        int ret = 0;

        if ((facet == null) || (typeDecl == null))
            return -1;

        switch (facet.type)
        {
            case XML_XSD10_SCHEMA_FACET_MININCLUSIVE:
            case XML_XSD10_SCHEMA_FACET_MINEXCLUSIVE:
            case XML_XSD10_SCHEMA_FACET_MAXINCLUSIVE:
            case XML_XSD10_SCHEMA_FACET_MAXEXCLUSIVE:
            case XML_XSD10_SCHEMA_FACET_ENUMERATION:
            {
                /*
                 * Okay we need to validate the value
                 * at that point.
                 */
                Xsd10Type base;

                /* 4.3.5.5 Constraints on enumeration Schema Components
                 * Schema Component Constraint: enumeration valid restriction
                 * It is an `error` if any member of {value} is not in the
                 * `value space` of {base type definition}.
                 *
                 * minInclusive, maxInclusive, minExclusive, maxExclusive:
                 * The value `must` be in the
                 * `value space` of the `base type`.
                 */
                /*
                 * This function is intended to deliver a compiled value
                 * on the facet. In this implementation of XML Schemata the
                 * type holding a facet, won't be a built-in type.
                 * Thus to ensure that other API
                 * calls (relaxng) do work, if the given type is a built-in
                 * type, we will assume that the given built-in type *is
                 * already* the base type.
                 */
                if (typeDecl.type != XML_XSD10_SCHEMA_TYPE_BASIC)
                {
                    base = typeDecl.baseType;
                    if (base == null)
                    {
                        return -1;
                    }
                }
                else
                    base = typeDecl;

                /*
                 * NOTE: This call does not check the content nodes,
                 * since they are not available:
                 * facet->node is just the node holding the facet
                 * definition, *not* the attribute holding the *value*
                 * of the facet.
                 */
                Xsd10Val[] fv = { facet.val };
                ret = typesCheckCvcSimpleType(facet.node, base, b(facet.value), fv, 1, 0);
                facet.val = fv[0];
                if (ret != 0)
                {
                    if (ret < 0)
                    {
                        return -1;
                    }
                    ret = XML_XSD10_SCHEMAP_INVALID_FACET_VALUE;
                    return ret;
                }
                else if (facet.val == null)
                {
                    /* TODO */
                }
                break;
            }
            case XML_XSD10_SCHEMA_FACET_PATTERN:
                facet.regexp = (xsd10_types_regexp_hooks_.compile != null && facet.value != null) ?
                    xsd10_types_regexp_hooks_.compile.compile(facet.value) : null;
                if (facet.regexp == null)
                {
                    ret = XML_XSD10_SCHEMAP_REGEXP_INVALID;
                }
                break;
            case XML_XSD10_SCHEMA_FACET_TOTALDIGITS:
            case XML_XSD10_SCHEMA_FACET_FRACTIONDIGITS:
            case XML_XSD10_SCHEMA_FACET_LENGTH:
            case XML_XSD10_SCHEMA_FACET_MAXLENGTH:
            case XML_XSD10_SCHEMA_FACET_MINLENGTH:
            {
                Xsd10Val[] fv = { facet.val };
                if (facet.type == XML_XSD10_SCHEMA_FACET_TOTALDIGITS)
                {
                    ret = validatePredefinedType(
                        getBuiltInType(XML_XSD10_SCHEMAS_PINTEGER),
                        facet.value, fv);
                }
                else
                {
                    ret = validatePredefinedType(
                        getBuiltInType(XML_XSD10_SCHEMAS_NNINTEGER),
                        facet.value, fv);
                }
                facet.val = fv[0];
                if (ret != 0)
                {
                    if (ret < 0)
                    {
                        return -1;
                    }
                    ret = XML_XSD10_SCHEMAP_INVALID_FACET_VALUE;
                }
                break;
            }
            case XML_XSD10_SCHEMA_FACET_WHITESPACE:
            {
                if ("preserve".equals(facet.value))
                {
                    facet.whitespace = XML_XSD10_SCHEMAS_FACET_PRESERVE;
                }
                else if ("replace".equals(facet.value))
                {
                    facet.whitespace = XML_XSD10_SCHEMAS_FACET_REPLACE;
                }
                else if ("collapse".equals(facet.value))
                {
                    facet.whitespace = XML_XSD10_SCHEMAS_FACET_COLLAPSE;
                }
                else
                {
                    ret = XML_XSD10_SCHEMAP_INVALID_FACET_VALUE;
                }
                break;
            }
            default:
                break;
        }
        return ret;
    }
}
