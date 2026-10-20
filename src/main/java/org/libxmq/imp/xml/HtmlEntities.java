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

package org.libxmq.imp.xml;

import java.util.HashMap;
import java.util.Map;

/** Html entity values, ported from
 *  Alternatives/SML/libxml2/HTMLparser.c (htmlDefaultEncodingNames).
 */
public final class HtmlEntities
{
    private static final Map<String, Integer> m = new HashMap<String, Integer>();

    static {
        put("quot", 0x22);
        put("amp", 0x26);
        put("apos", 0x27);
        put("lt", 0x3c);
        put("gt", 0x3e);
        put("nbsp", 0xa0);
        put("iexcl", 0xa1);
        put("cent", 0xa2);
        put("pound", 0xa3);
        put("curren", 0xa4);
        put("yen", 0xa5);
        put("brvbar", 0xa6);
        put("sect", 0xa7);
        put("uml", 0xa8);
        put("copy", 0xa9);
        put("ordf", 0xaa);
        put("laquo", 0xab);
        put("not", 0xac);
        put("shy", 0xad);
        put("reg", 0xae);
        put("macr", 0xaf);
        put("deg", 0xb0);
        put("plusmn", 0xb1);
        put("sup2", 0xb2);
        put("sup3", 0xb3);
        put("acute", 0xb4);
        put("micro", 0xb5);
        put("para", 0xb6);
        put("middot", 0xb7);
        put("cedil", 0xb8);
        put("sup1", 0xb9);
        put("ordm", 0xba);
        put("raquo", 0xbb);
        put("frac14", 0xbc);
        put("frac12", 0xbd);
        put("frac34", 0xbe);
        put("iquest", 0xbf);
        put("Agrave", 0xc0);
        put("Aacute", 0xc1);
        put("Acirc", 0xc2);
        put("Atilde", 0xc3);
        put("Auml", 0xc4);
        put("Aring", 0xc5);
        put("AElig", 0xc6);
        put("Ccedil", 0xc7);
        put("Egrave", 0xc8);
        put("Eacute", 0xc9);
        put("Ecirc", 0xca);
        put("Euml", 0xcb);
        put("Igrave", 0xcc);
        put("Iacute", 0xcd);
        put("Icirc", 0xce);
        put("Iuml", 0xcf);
        put("ETH", 0xd0);
        put("Ntilde", 0xd1);
        put("Ograve", 0xd2);
        put("Oacute", 0xd3);
        put("Ocirc", 0xd4);
        put("Otilde", 0xd5);
        put("Ouml", 0xd6);
        put("times", 0xd7);
        put("Oslash", 0xd8);
        put("Ugrave", 0xd9);
        put("Uacute", 0xda);
        put("Ucirc", 0xdb);
        put("Uuml", 0xdc);
        put("Yacute", 0xdd);
        put("THORN", 0xde);
        put("szlig", 0xdf);
        put("agrave", 0xe0);
        put("aacute", 0xe1);
        put("acirc", 0xe2);
        put("atilde", 0xe3);
        put("auml", 0xe4);
        put("aring", 0xe5);
        put("aelig", 0xe6);
        put("ccedil", 0xe7);
        put("egrave", 0xe8);
        put("eacute", 0xe9);
        put("ecirc", 0xea);
        put("euml", 0xeb);
        put("igrave", 0xec);
        put("iacute", 0xed);
        put("icirc", 0xee);
        put("iuml", 0xef);
        put("eth", 0xf0);
        put("ntilde", 0xf1);
        put("ograve", 0xf2);
        put("oacute", 0xf3);
        put("ocirc", 0xf4);
        put("otilde", 0xf5);
        put("ouml", 0xf6);
        put("divide", 0xf7);
        put("oslash", 0xf8);
        put("ugrave", 0xf9);
        put("uacute", 0xfa);
        put("ucirc", 0xfb);
        put("uuml", 0xfc);
        put("yacute", 0xfd);
        put("thorn", 0xfe);
        put("yuml", 0xff);
        put("OElig", 0x152);
        put("oelig", 0x153);
        put("Scaron", 0x160);
        put("scaron", 0x161);
        put("Yuml", 0x178);
        put("fnof", 0x192);
        put("circ", 0x2c6);
        put("tilde", 0x2dc);
        put("Alpha", 0x391);
        put("Beta", 0x392);
        put("Gamma", 0x393);
        put("Delta", 0x394);
        put("Epsilon", 0x395);
        put("Zeta", 0x396);
        put("Eta", 0x397);
        put("Theta", 0x398);
        put("Iota", 0x399);
        put("Kappa", 0x39a);
        put("Lambda", 0x39b);
        put("Mu", 0x39c);
        put("Nu", 0x39d);
        put("Xi", 0x39e);
        put("Omicron", 0x39f);
        put("Pi", 0x3a0);
        put("Rho", 0x3a1);
        put("Sigma", 0x3a3);
        put("Tau", 0x3a4);
        put("Upsilon", 0x3a5);
        put("Phi", 0x3a6);
        put("Chi", 0x3a7);
        put("Psi", 0x3a8);
        put("Omega", 0x3a9);
        put("alpha", 0x3b1);
        put("beta", 0x3b2);
        put("gamma", 0x3b3);
        put("delta", 0x3b4);
        put("epsilon", 0x3b5);
        put("zeta", 0x3b6);
        put("eta", 0x3b7);
        put("theta", 0x3b8);
        put("iota", 0x3b9);
        put("kappa", 0x3ba);
        put("lambda", 0x3bb);
        put("mu", 0x3bc);
        put("nu", 0x3bd);
        put("xi", 0x3be);
        put("omicron", 0x3bf);
        put("pi", 0x3c0);
        put("rho", 0x3c1);
        put("sigmaf", 0x3c2);
        put("sigma", 0x3c3);
        put("tau", 0x3c4);
        put("upsilon", 0x3c5);
        put("phi", 0x3c6);
        put("chi", 0x3c7);
        put("psi", 0x3c8);
        put("omega", 0x3c9);
        put("thetasym", 0x3d1);
        put("upsih", 0x3d2);
        put("piv", 0x3d6);
        put("ensp", 0x2002);
        put("emsp", 0x2003);
        put("thinsp", 0x2009);
        put("zwnj", 0x200c);
        put("zwj", 0x200d);
        put("lrm", 0x200e);
        put("rlm", 0x200f);
        put("ndash", 0x2013);
        put("mdash", 0x2014);
        put("lsquo", 0x2018);
        put("rsquo", 0x2019);
        put("sbquo", 0x201a);
        put("ldquo", 0x201c);
        put("rdquo", 0x201d);
        put("bdquo", 0x201e);
        put("dagger", 0x2020);
        put("Dagger", 0x2021);
        put("bull", 0x2022);
        put("hellip", 0x2026);
        put("permil", 0x2030);
        put("prime", 0x2032);
        put("Prime", 0x2033);
        put("lsaquo", 0x2039);
        put("rsaquo", 0x203a);
        put("oline", 0x203e);
        put("frasl", 0x2044);
        put("euro", 0x20ac);
        put("image", 0x2111);
        put("weierp", 0x2118);
        put("real", 0x211c);
        put("trade", 0x2122);
        put("alefsym", 0x2135);
        put("larr", 0x2190);
        put("uarr", 0x2191);
        put("rarr", 0x2192);
        put("darr", 0x2193);
        put("harr", 0x2194);
        put("crarr", 0x21b5);
        put("lArr", 0x21d0);
        put("uArr", 0x21d1);
        put("rArr", 0x21d2);
        put("dArr", 0x21d3);
        put("hArr", 0x21d4);
        put("forall", 0x2200);
        put("part", 0x2202);
        put("exist", 0x2203);
        put("empty", 0x2205);
        put("nabla", 0x2207);
        put("isin", 0x2208);
        put("notin", 0x2209);
        put("ni", 0x220b);
        put("prod", 0x220f);
        put("sum", 0x2211);
        put("minus", 0x2212);
        put("lowast", 0x2217);
        put("radic", 0x221a);
        put("prop", 0x221d);
        put("infin", 0x221e);
        put("ang", 0x2220);
        put("and", 0x2227);
        put("or", 0x2228);
        put("cap", 0x2229);
        put("cup", 0x222a);
        put("int", 0x222b);
        put("there4", 0x2234);
        put("sim", 0x223c);
        put("cong", 0x2245);
        put("asymp", 0x2248);
        put("ne", 0x2260);
        put("equiv", 0x2261);
        put("le", 0x2264);
        put("ge", 0x2265);
        put("sub", 0x2282);
        put("sup", 0x2283);
        put("nsub", 0x2284);
        put("sube", 0x2286);
        put("supe", 0x2287);
        put("oplus", 0x2295);
        put("otimes", 0x2297);
        put("perp", 0x22a5);
        put("sdot", 0x22c5);
        put("lceil", 0x2308);
        put("rceil", 0x2309);
        put("lfloor", 0x230a);
        put("rfloor", 0x230b);
        put("lang", 0x2329);
        put("rang", 0x232a);
        put("loz", 0x25ca);
        put("spades", 0x2660);
        put("clubs", 0x2663);
        put("hearts", 0x2665);
        put("diams", 0x2666);
    }

    private HtmlEntities()
    {
    }

    /** Look up a named html entity.
     *  @param name the entity name
     *  @return the code point, or null if unknown */
    public static Integer get(String name)
    {
        return m.get(name);
    }

    /** Store an entity name to code-point mapping in this table.
     *  @param name the entity name
     *  @param i the code point */
    public static void put(String name, int i)
    {
        m.put(name, i);
    }

}
