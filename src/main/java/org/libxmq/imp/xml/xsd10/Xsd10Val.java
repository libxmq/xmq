/* This file is ported from libxml2 (xmlschemastypes.h and
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

/** A computed value (libxml2 xmlSchemaVal, C {@code struct XmlXsd10Val} of xml_xsd10_types.c).
    The C union {@code value} is flattened: {@code val->value.decimal.str} is {@code val.decimal.str},
    {@code val->value.str} is {@code val.str}, {@code val->value.f} is {@code val.f} etc. The union member
    structs are allocated eagerly so they can be written without a null check; a C struct copy of the
    union (memcpy, {@code *dst = *src}) must copy the members (see the copy helpers of the members). */
public final class Xsd10Val
{
    /** Construct with default field values. */
    public Xsd10Val()
    {
    }

    /** C XmlXsd10ValDecimal. */
    static final class Decimal
    {
        /** XmlXsd10Char *str */
        String str;
        /** unsigned integral_places */
        int integral_places;
        /** unsigned fractional_places */
        int fractional_places;
        Decimal copy() { Decimal d = new Decimal(); d.str = str; d.integral_places = integral_places; d.fractional_places = fractional_places; return d; }
    }
    /** C XmlXsd10ValDate (the C bit fields are plain ints). */
    static final class Date
    {
        /** long year */
        long year;
        /** 1 &lt;= mon &lt;= 12 (4 bits) */
        int mon;
        /** 1 &lt;= day &lt;= 31 (5 bits) */
        int day;
        /** 0 &lt;= hour &lt;= 24 (5 bits) */
        int hour;
        /** 0 &lt;= min &lt;= 59 (6 bits) */
        int min;
        /** double sec */
        double sec;
        /** is tzo explicitly set? (1 bit) */
        int tz_flag;
        /** -1440 &lt;= tzo &lt;= 1440 (signed 12 bits) */
        int tzo;
        Date copy() { Date d = new Date(); d.year = year; d.mon = mon; d.day = day; d.hour = hour; d.min = min; d.sec = sec; d.tz_flag = tz_flag; d.tzo = tzo; return d; }
    }
    /** C XmlXsd10ValDuration. */
    static final class Duration
    {
        /** mon stores years also */
        long mon;
        /** long day */
        long day;
        /** sec stores min and hour also */
        double sec;
        Duration copy() { Duration d = new Duration(); d.mon = mon; d.day = day; d.sec = sec; return d; }
    }
    /** C XmlXsd10ValQName. */
    static final class QName
    {
        /** name */
        String name;
        /** uri */
        String uri;
        QName copy() { QName q = new QName(); q.name = name; q.uri = uri; return q; }
    }
    /** C XmlXsd10ValHex and XmlXsd10ValBase64. */
    static final class Bin
    {
        /** str */
        String str;
        /** unsigned int total */
        int total;
        Bin copy() { Bin b = new Bin(); b.str = str; b.total = total; return b; }
    }

    /** XmlXsd10ValType */
    int type;
    /** struct XmlXsd10Val *next */
    Xsd10Val next;
    /** value.decimal */
    Decimal decimal = new Decimal();
    /** value.date */
    Date date = new Date();
    /** value.dur */
    Duration dur = new Duration();
    /** value.qname */
    QName qname = new QName();
    /** value.hex */
    Bin hex = new Bin();
    /** value.base64 */
    Bin base64 = new Bin();
    /** value.f */
    float f;
    /** value.d */
    double d;
    /** value.b */
    int b;
    /** value.str */
    String str;

    /** Copy the whole C union (the {@code value} member) from src into this value (type and next are not copied).
        @param src the source */
    void copyValueFrom(Xsd10Val src)
    {
        decimal = src.decimal.copy();
        date = src.date.copy();
        dur = src.dur.copy();
        qname = src.qname.copy();
        hex = src.hex.copy();
        base64 = src.base64.copy();
        f = src.f;
        d = src.d;
        b = src.b;
        str = src.str;
    }
}
