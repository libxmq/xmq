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

package org.libxmq.imp;

import java.util.HashMap;
import java.util.Map;

/**
 * The html entity names used when escaping non 7-bit characters in htmq output.
 * This mirrors the C function toHtmlEntity (entities.c), which only knows the
 * greek, math and misc html entities, not the latin1 entities.
 */
public final class XMQHtmlEntities
{
    private XMQHtmlEntities()
    {
    }

    private static final Map<Integer, String> NAMES = new HashMap<>();

    static
    {
        NAMES.put(338, "OElig");
        NAMES.put(339, "oelig");
        NAMES.put(352, "Scaron");
        NAMES.put(353, "scaron");
        NAMES.put(376, "Yuml");
        NAMES.put(402, "fnof");
        NAMES.put(710, "circ");
        NAMES.put(732, "tilde");
        NAMES.put(913, "Alpha");
        NAMES.put(914, "Beta");
        NAMES.put(915, "Gamma");
        NAMES.put(916, "Delta");
        NAMES.put(917, "Epsilon");
        NAMES.put(918, "Zeta");
        NAMES.put(919, "Eta");
        NAMES.put(920, "Theta");
        NAMES.put(921, "Iota");
        NAMES.put(922, "Kappa");
        NAMES.put(923, "Lambda");
        NAMES.put(924, "Mu");
        NAMES.put(925, "Nu");
        NAMES.put(926, "Xi");
        NAMES.put(927, "Omicron");
        NAMES.put(928, "Pi");
        NAMES.put(929, "Rho");
        NAMES.put(931, "Sigma");
        NAMES.put(932, "Tau");
        NAMES.put(933, "Upsilon");
        NAMES.put(934, "Phi");
        NAMES.put(935, "Chi");
        NAMES.put(936, "Psi");
        NAMES.put(937, "Omega");
        NAMES.put(945, "alpha");
        NAMES.put(946, "beta");
        NAMES.put(947, "gamma");
        NAMES.put(948, "delta");
        NAMES.put(949, "epsilon");
        NAMES.put(950, "zeta");
        NAMES.put(951, "eta");
        NAMES.put(952, "theta");
        NAMES.put(953, "iota");
        NAMES.put(954, "kappa");
        NAMES.put(955, "lambda");
        NAMES.put(956, "mu");
        NAMES.put(957, "nu");
        NAMES.put(958, "xi");
        NAMES.put(959, "omicron");
        NAMES.put(960, "pi");
        NAMES.put(961, "rho");
        NAMES.put(962, "sigmaf");
        NAMES.put(963, "sigma");
        NAMES.put(964, "tau");
        NAMES.put(965, "upsilon");
        NAMES.put(966, "phi");
        NAMES.put(967, "chi");
        NAMES.put(968, "psi");
        NAMES.put(969, "omega");
        NAMES.put(977, "thetasym");
        NAMES.put(978, "upsih");
        NAMES.put(982, "piv");
        NAMES.put(8194, "ensp");
        NAMES.put(8195, "emsp");
        NAMES.put(8201, "thinsp");
        NAMES.put(8204, "zwnj");
        NAMES.put(8205, "zwj");
        NAMES.put(8206, "lrm");
        NAMES.put(8207, "rlm");
        NAMES.put(8211, "ndash");
        NAMES.put(8212, "mdash");
        NAMES.put(8216, "lsquo");
        NAMES.put(8217, "rsquo");
        NAMES.put(8218, "sbquo");
        NAMES.put(8220, "ldquo");
        NAMES.put(8221, "rdquo");
        NAMES.put(8222, "bdquo");
        NAMES.put(8224, "dagger");
        NAMES.put(8225, "Dagger");
        NAMES.put(8226, "bull");
        NAMES.put(8230, "hellip");
        NAMES.put(8240, "permil");
        NAMES.put(8242, "prime");
        NAMES.put(8243, "Prime");
        NAMES.put(8249, "lsaquo");
        NAMES.put(8250, "rsaquo");
        NAMES.put(8254, "oline");
        NAMES.put(8364, "euro");
        NAMES.put(8482, "trade");
        NAMES.put(8592, "larr");
        NAMES.put(8593, "uarr");
        NAMES.put(8594, "rarr");
        NAMES.put(8595, "darr");
        NAMES.put(8596, "harr");
        NAMES.put(8629, "crarr");
        NAMES.put(8704, "forall");
        NAMES.put(8706, "part");
        NAMES.put(8707, "exist");
        NAMES.put(8709, "empty");
        NAMES.put(8711, "nabla");
        NAMES.put(8712, "isin");
        NAMES.put(8713, "notin");
        NAMES.put(8715, "ni");
        NAMES.put(8719, "prod");
        NAMES.put(8721, "sum");
        NAMES.put(8722, "minus");
        NAMES.put(8727, "lowast");
        NAMES.put(8730, "radic");
        NAMES.put(8733, "prop");
        NAMES.put(8734, "infin");
        NAMES.put(8736, "ang");
        NAMES.put(8743, "and");
        NAMES.put(8744, "or");
        NAMES.put(8745, "cap");
        NAMES.put(8746, "cup");
        NAMES.put(8747, "int");
        NAMES.put(8756, "there4");
        NAMES.put(8764, "sim");
        NAMES.put(8773, "cong");
        NAMES.put(8776, "asymp");
        NAMES.put(8800, "ne");
        NAMES.put(8801, "equiv");
        NAMES.put(8804, "le");
        NAMES.put(8805, "ge");
        NAMES.put(8834, "sub");
        NAMES.put(8835, "sup");
        NAMES.put(8836, "nsub");
        NAMES.put(8838, "sube");
        NAMES.put(8839, "supe");
        NAMES.put(8853, "oplus");
        NAMES.put(8855, "otimes");
        NAMES.put(8869, "perp");
        NAMES.put(8901, "sdot");
        NAMES.put(8968, "lceil");
        NAMES.put(8969, "rceil");
        NAMES.put(8970, "lfloor");
        NAMES.put(8971, "rfloor");
        NAMES.put(9674, "loz");
        NAMES.put(9824, "spades");
        NAMES.put(9827, "clubs");
        NAMES.put(9829, "hearts");
        NAMES.put(9830, "diams");
    }

    /**
     * Returns the html entity name for a code point, like the C toHtmlEntity.
     * @param uc The unicode code point.
     * @return The entity name or null.
     */
    public static String toHtmlEntity(int uc)
    {
        return NAMES.get(uc);
    }
}
