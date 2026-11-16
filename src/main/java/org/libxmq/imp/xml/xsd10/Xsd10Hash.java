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

import java.util.ArrayList;
import java.util.HashMap;
import java.util.Objects;

/** A hash table with one to three string keys (libxml2 xmlHashTable, C {@code struct XmlXsd10Hash} of
    xml_xsd10_schemas.c), scanned in insertion order. Null keys are allowed for key2/key3 and differ
    from "". The data structure is implemented here (the instance methods mirror the static C helpers
    of xml_xsd10_schemas.c); the xmlHash api functions (hashCreate, hashAddEntry, hashLookup2, hashScan ...)
    are in Xsd10Schemas and call these. */
public final class Xsd10Hash
{
    /** C Xsd10SchemasHashEntry. */
    static final class Entry
    {
        /** char *key */
        String key;
        /** char *key2 */
        String key2;
        /** char *key3 */
        String key3;
        /** void *payload */
        Object payload;
        /** 0 when removed. */
        int used;
    }

    private static final class Key
    {
        final String k1, k2, k3;
        Key(String k1, String k2, String k3) { this.k1 = k1; this.k2 = k2; this.k3 = k3; }
        @Override public boolean equals(Object o)
        {
            if (!(o instanceof Key)) return false;
            Key k = (Key) o;
            return Objects.equals(k1, k.k1) && Objects.equals(k2, k.k2) && Objects.equals(k3, k.k3);
        }
        @Override public int hashCode() { return Objects.hash(k1, k2, k3); }
    }

    /** The entries in insertion order, including the removed ones (C entries, nb_entries). */
    ArrayList<Entry> entries = new ArrayList<>();
    /** key -&gt; index in entries (replaces the C buckets). */
    private final HashMap<Key, Integer> index = new HashMap<>();
    /** The used entries (C nb_elems). */
    int nb_elems;

    /** xsd10_schemas_hash_find: the entry for the keys, null if none.
        @param key key @param key2 key2 @param key3 key3 @return the entry or null */
    Entry find(String key, String key2, String key3)
    {
        Integer i = index.get(new Key(key, key2, key3));
        return i == null ? null : entries.get(i);
    }

    /** xsd10_schemas_hash_update_internal: add (update == 0) or add/replace. Returns 1 if added, 0 if the
        key exists (and was replaced if update, calling dealloc on the old non null payload), -1 on errors
        (key null).
        @param key key @param key2 key2 @param key3 key3 @param payload payload
        @param dealloc deallocator or null @param update 1 to replace @return 1, 0 or -1 */
    int updateInternal(String key, String key2, String key3, Object payload,
                       Xsd10Const.HashDeallocator dealloc, int update)
    {
        if (key == null) return -1;
        Entry e = find(key, key2, key3);
        if (e != null)
        {
            if (update != 0)
            {
                if ((dealloc != null) && (e.payload != null)) dealloc.dealloc(e.payload, e.key);
                e.payload = payload;
            }
            return 0;
        }
        e = new Entry();
        e.key = key;
        e.key2 = key2;
        e.key3 = key3;
        e.payload = payload;
        e.used = 1;
        index.put(new Key(key, key2, key3), entries.size());
        entries.add(e);
        nb_elems++;
        return 1;
    }

    /** The core of xml_xsd10_hash_remove_entry3: 0 if removed (dealloc called on a non null payload),
        -1 if not found.
        @param key key @param key2 key2 @param key3 key3 @param dealloc deallocator or null @return 0 or -1 */
    int remove(String key, String key2, String key3, Xsd10Const.HashDeallocator dealloc)
    {
        if (key == null) return -1;
        Integer i = index.get(new Key(key, key2, key3));
        if (i == null) return -1;
        Entry e = entries.get(i);
        if ((dealloc != null) && (e.payload != null)) dealloc.dealloc(e.payload, e.key);
        index.remove(new Key(key, key2, key3));
        e.key = e.key2 = e.key3 = null;
        e.payload = null;
        e.used = 0;
        nb_elems--;
        return 0;
    }

    /** The core of xml_xsd10_hash_scan_full: entries added by the callback are scanned too, removed ones
        are skipped, entries with a null payload are skipped.
        @param scan the scanner @param data the user data */
    void scanFull(Xsd10Const.HashScannerFull scan, Object data)
    {
        for (int i = 0; i < entries.size(); i++)
        {
            Entry e = entries.get(i);
            if (e.used != 0 && e.payload != null) scan.scan(e.payload, data, e.key, e.key2, e.key3);
        }
    }

    /** The core of xml_xsd10_hash_free: dealloc every used non null payload (in insertion order).
        @param dealloc the deallocator or null */
    void freeAll(Xsd10Const.HashDeallocator dealloc)
    {
        for (int i = 0; i < entries.size(); i++)
        {
            Entry e = entries.get(i);
            if (e.used == 0) continue;
            if ((dealloc != null) && (e.payload != null)) dealloc.dealloc(e.payload, e.key);
        }
        entries.clear();
        index.clear();
        nb_elems = 0;
    }
}
