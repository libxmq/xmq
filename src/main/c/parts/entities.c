/* libxmq - Copyright (C) 2023 Fredrik Öhrström (spdx: MIT)

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
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#ifndef BUILDING_DIST_XMQ

#include"always.h"
#include"entities.h"

#endif

#ifdef ENTITIES_MODULE

// This is not complete yet...

#define HTML_GREEK \
X(913,Alpha,"Α","Alpha") \
X(914,Beta,"Β","Beta") \
X(915,Gamma,"Γ","Gamma") \
X(916,Delta,"Δ","Delta") \
X(917,Epsilon,"Ε","Epsilon") \
X(918,Zeta,"Ζ","Zeta") \
X(919,Eta,"Η","Eta") \
X(920,Theta,"Θ","Theta") \
X(921,Iota,"Ι","Iota") \
X(922,Kappa,"Κ","Kappa") \
X(923,Lambda,"Λ","Lambda") \
X(924,Mu,"Μ","Mu") \
X(925,Nu,"Ν","Nu") \
X(926,Xi,"Ξ","Xi") \
X(927,Omicron,"Ο","Omicron") \
X(928,Pi,"Π","Pi") \
X(929,Rho,"Ρ","Rho") \
X(931,Sigma,"Σ","Sigma") \
X(932,Tau,"Τ","Tau") \
X(933,Upsilon,"Υ","Upsilon") \
X(934,Phi,"Φ","Phi") \
X(935,Chi,"Χ","Chi") \
X(936,Psi,"Ψ","Psi") \
X(937,Omega,"Ω","Omega") \
X(945,alpha,"α","alpha") \
X(946,beta,"β","beta") \
X(947,gamma,"γ","gamma") \
X(948,delta,"δ","delta") \
X(949,epsilon,"ε","epsilon") \
X(950,zeta,"ζ","zeta") \
X(951,eta,"η","eta") \
X(952,theta,"θ","theta") \
X(953,iota,"ι","iota") \
X(954,kappa,"κ","kappa") \
X(955,lambda,"λ","lambda") \
X(956,mu,"μ","mu") \
X(957,nu,"ν","nu") \
X(958,xi,"ξ","xi") \
X(959,omicron,"ο","omicron") \
X(960,pi,"π","pi") \
X(961,rho,"ρ","rho") \
X(962,sigmaf,"ς","sigmaf") \
X(963,sigma,"σ","sigma") \
X(964,tau,"τ","tau") \
X(965,upsilon,"υ","upsilon") \
X(966,phi,"φ","phi") \
X(967,chi,"χ","chi") \
X(968,psi,"ψ","psi") \
X(969,omega,"ω","omega") \
X(977,thetasym,"ϑ","Theta") \
X(978,upsih,"ϒ","Upsilon") \
X(982,piv,"ϖ","Pi") \

#define HTML_MATH        \
X(8704,forall,"∀","For") \
X(8706,part,"∂","Part") \
X(8707,exist,"∃","Exist") \
X(8709,empty,"∅","Empty") \
X(8711,nabla,"∇","Nabla") \
X(8712,isin,"∈","Is") \
X(8713,notin,"∉","Not") \
X(8715,ni,"∋","Ni") \
X(8719,prod,"∏","Product") \
X(8721,sum,"∑","Sum") \
X(8722,minus,"−","Minus") \
X(8727,lowast,"∗","Asterisk") \
X(8730,radic,"√","Square") \
X(8733,prop,"∝","Proportional") \
X(8734,infin,"∞","Infinity") \
X(8736,ang,"∠","Angle") \
X(8743,and,"∧","And") \
X(8744,or,"∨","Or") \
X(8745,cap,"∩","Cap") \
X(8746,cup,"∪","Cup") \
X(8747,int,"∫","Integral") \
X(8756,there4,"∴","Therefore") \
X(8764,sim,"∼","Similar") \
X(8773,cong,"≅","Congurent") \
X(8776,asymp,"≈","Almost") \
X(8800,ne,"≠","Not") \
X(8801,equiv,"≡","Equivalent") \
X(8804,le,"≤","Less") \
X(8805,ge,"≥","Greater") \
X(8834,sub,"⊂","Subset") \
X(8835,sup,"⊃","Superset") \
X(8836,nsub,"⊄","Not") \
X(8838,sube,"⊆","Subset") \
X(8839,supe,"⊇","Superset") \
X(8853,oplus,"⊕","Circled") \
X(8855,otimes,"⊗","Circled") \
X(8869,perp,"⊥","Perpendicular") \
X(8901,sdot,"⋅","Dot") \

#define HTML_MISC \
X(338,OElig,"Œ","Uppercase") \
X(339,oelig,"œ","Lowercase") \
X(352,Scaron,"Š","Uppercase") \
X(353,scaron,"š","Lowercase") \
X(376,Yuml,"Ÿ","Capital") \
X(402,fnof,"ƒ","Lowercase") \
X(710,circ,"ˆ","Circumflex") \
X(732,tilde,"˜","Tilde") \
X(8194,ensp," ","En") \
X(8195,emsp," ","Em") \
X(8201,thinsp," ","Thin") \
X(8204,zwnj,"‌","Zero") \
X(8205,zwj,"‍","Zero") \
X(8206,lrm,"‎","Left-to-right") \
X(8207,rlm,"‏","Right-to-left") \
X(8211,ndash,"–","En") \
X(8212,mdash,"—","Em") \
X(8216,lsquo,"‘","Left") \
X(8217,rsquo,"’","Right") \
X(8218,sbquo,"‚","Single") \
X(8220,ldquo,"“","Left") \
X(8221,rdquo,"”","Right") \
X(8222,bdquo,"„","Double") \
X(8224,dagger,"†","Dagger") \
X(8225,Dagger,"‡","Double") \
X(8226,bull,"•","Bullet") \
X(8230,hellip,"…","Horizontal") \
X(8240,permil,"‰","Per") \
X(8242,prime,"′","Minutes") \
X(8243,Prime,"″","Seconds") \
X(8249,lsaquo,"‹","Single") \
X(8250,rsaquo,"›","Single") \
X(8254,oline,"‾","Overline") \
X(8364,euro,"€","Euro") \
X(8482,trade,"™","Trademark") \
X(8592,larr,"←","Left") \
X(8593,uarr,"↑","Up") \
X(8594,rarr,"→","Right") \
X(8595,darr,"↓","Down") \
X(8596,harr,"↔","Left") \
X(8629,crarr,"↵","Carriage") \
X(8968,lceil,"⌈","Left") \
X(8969,rceil,"⌉","Right") \
X(8970,lfloor,"⌊","Left") \
X(8971,rfloor,"⌋","Right") \
X(9674,loz,"◊","Lozenge") \
X(9824,spades,"♠","Spade") \
X(9827,clubs,"♣","Club") \
X(9829,hearts,"♥","Heart") \
X(9830,diams,"♦","Diamond") \

const char *toHtmlEntity(int uc)
{
    switch(uc)
    {
#define X(uc,name,is,about) case uc: return #name;
HTML_GREEK
HTML_MATH
HTML_MISC
#undef X
    }
    return NULL;
}

#endif // ENTITIES_MODULE
