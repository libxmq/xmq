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

// Ported from libxml2 2.9.14 HTMLparser.c,
// Copyright (C) 1998-2012 Daniel Veillard, MIT license.

package org.libxmq.imp.xml;

import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;

/**
 * The HTML 4 element descriptions and the auto close table used by the
 * html parser and serializer. Ported from html40ElementTable and htmlStartClose in
 * libxml2 HTMLparser.c (Copyright (C) 1998-2012 Daniel Veillard, MIT license).
 */
public final class HtmlElementTable
{
    /**
     * Description of an html element, like libxml2 htmlElemDesc.
     */
    public static final class Desc
    {
        /** The element name. */
        public final String name;
        /** 1 if the start tag can be implied. */
        public final int startTag;
        /** 1 if the end tag can be implied, 2 if it is forbidden, 3 if it is stylistic. */
        public final int endTag;
        /** 1 if the end tag should be saved when there is no content. */
        public final int saveEndTag;
        /** 1 if the element is empty. */
        public final int empty;
        /** 1 if the element is deprecated. */
        public final int depr;
        /** 1 = loose dtd only, 2 = frameset dtd only. */
        public final int dtd;
        /** 1 if the element is inline, 2 if it is both inline and block. */
        public final int isinline;

        Desc(String name, int startTag, int endTag, int saveEndTag, int empty, int depr, int dtd, int isinline)
        {
            this.name = name;
            this.startTag = startTag;
            this.endTag = endTag;
            this.saveEndTag = saveEndTag;
            this.empty = empty;
            this.depr = depr;
            this.dtd = dtd;
            this.isinline = isinline;
        }
    }

    private HtmlElementTable()
    {
    }

    private static final Desc[] TABLE = {
        new Desc("a", 0, 0, 0, 0, 0, 0, 1),
        new Desc("abbr", 0, 0, 0, 0, 0, 0, 1),
        new Desc("acronym", 0, 0, 0, 0, 0, 0, 1),
        new Desc("address", 0, 0, 0, 0, 0, 0, 0),
        new Desc("applet", 0, 0, 0, 0, 1, 1, 2),
        new Desc("area", 0, 2, 2, 1, 0, 0, 0),
        new Desc("b", 0, 3, 0, 0, 0, 0, 1),
        new Desc("base", 0, 2, 2, 1, 0, 0, 0),
        new Desc("basefont", 0, 2, 2, 1, 1, 1, 1),
        new Desc("bdo", 0, 0, 0, 0, 0, 0, 1),
        new Desc("big", 0, 3, 0, 0, 0, 0, 1),
        new Desc("blockquote", 0, 0, 0, 0, 0, 0, 0),
        new Desc("body", 1, 1, 0, 0, 0, 0, 0),
        new Desc("br", 0, 2, 2, 1, 0, 0, 1),
        new Desc("button", 0, 0, 0, 0, 0, 0, 2),
        new Desc("caption", 0, 0, 0, 0, 0, 0, 0),
        new Desc("center", 0, 3, 0, 0, 1, 1, 0),
        new Desc("cite", 0, 0, 0, 0, 0, 0, 1),
        new Desc("code", 0, 0, 0, 0, 0, 0, 1),
        new Desc("col", 0, 2, 2, 1, 0, 0, 0),
        new Desc("colgroup", 0, 1, 0, 0, 0, 0, 0),
        new Desc("dd", 0, 1, 0, 0, 0, 0, 0),
        new Desc("del", 0, 0, 0, 0, 0, 0, 2),
        new Desc("dfn", 0, 0, 0, 0, 0, 0, 1),
        new Desc("dir", 0, 0, 0, 0, 1, 1, 0),
        new Desc("div", 0, 0, 0, 0, 0, 0, 0),
        new Desc("dl", 0, 0, 0, 0, 0, 0, 0),
        new Desc("dt", 0, 1, 0, 0, 0, 0, 0),
        new Desc("em", 0, 3, 0, 0, 0, 0, 1),
        new Desc("embed", 0, 1, 0, 0, 1, 1, 1),
        new Desc("fieldset", 0, 0, 0, 0, 0, 0, 0),
        new Desc("font", 0, 3, 0, 0, 1, 1, 1),
        new Desc("form", 0, 0, 0, 0, 0, 0, 0),
        new Desc("frame", 0, 2, 2, 1, 0, 2, 0),
        new Desc("frameset", 0, 0, 0, 0, 0, 2, 0),
        new Desc("h1", 0, 0, 0, 0, 0, 0, 0),
        new Desc("h2", 0, 0, 0, 0, 0, 0, 0),
        new Desc("h3", 0, 0, 0, 0, 0, 0, 0),
        new Desc("h4", 0, 0, 0, 0, 0, 0, 0),
        new Desc("h5", 0, 0, 0, 0, 0, 0, 0),
        new Desc("h6", 0, 0, 0, 0, 0, 0, 0),
        new Desc("head", 1, 1, 0, 0, 0, 0, 0),
        new Desc("hr", 0, 2, 2, 1, 0, 0, 0),
        new Desc("html", 1, 1, 0, 0, 0, 0, 0),
        new Desc("i", 0, 3, 0, 0, 0, 0, 1),
        new Desc("iframe", 0, 0, 0, 0, 0, 1, 2),
        new Desc("img", 0, 2, 2, 1, 0, 0, 1),
        new Desc("input", 0, 2, 2, 1, 0, 0, 1),
        new Desc("ins", 0, 0, 0, 0, 0, 0, 2),
        new Desc("isindex", 0, 2, 2, 1, 1, 1, 0),
        new Desc("kbd", 0, 0, 0, 0, 0, 0, 1),
        new Desc("label", 0, 0, 0, 0, 0, 0, 1),
        new Desc("legend", 0, 0, 0, 0, 0, 0, 0),
        new Desc("li", 0, 1, 1, 0, 0, 0, 0),
        new Desc("link", 0, 2, 2, 1, 0, 0, 0),
        new Desc("map", 0, 0, 0, 0, 0, 0, 2),
        new Desc("menu", 0, 0, 0, 0, 1, 1, 0),
        new Desc("meta", 0, 2, 2, 1, 0, 0, 0),
        new Desc("noframes", 0, 0, 0, 0, 0, 2, 0),
        new Desc("noscript", 0, 0, 0, 0, 0, 0, 0),
        new Desc("object", 0, 0, 0, 0, 0, 0, 2),
        new Desc("ol", 0, 0, 0, 0, 0, 0, 0),
        new Desc("optgroup", 0, 0, 0, 0, 0, 0, 0),
        new Desc("option", 0, 1, 0, 0, 0, 0, 0),
        new Desc("p", 0, 1, 0, 0, 0, 0, 0),
        new Desc("param", 0, 2, 2, 1, 0, 0, 0),
        new Desc("pre", 0, 0, 0, 0, 0, 0, 0),
        new Desc("q", 0, 0, 0, 0, 0, 0, 1),
        new Desc("s", 0, 3, 0, 0, 1, 1, 1),
        new Desc("samp", 0, 0, 0, 0, 0, 0, 1),
        new Desc("script", 0, 0, 0, 0, 0, 0, 2),
        new Desc("select", 0, 0, 0, 0, 0, 0, 1),
        new Desc("small", 0, 3, 0, 0, 0, 0, 1),
        new Desc("span", 0, 0, 0, 0, 0, 0, 1),
        new Desc("strike", 0, 3, 0, 0, 1, 1, 1),
        new Desc("strong", 0, 3, 0, 0, 0, 0, 1),
        new Desc("style", 0, 0, 0, 0, 0, 0, 0),
        new Desc("sub", 0, 3, 0, 0, 0, 0, 1),
        new Desc("sup", 0, 3, 0, 0, 0, 0, 1),
        new Desc("table", 0, 0, 0, 0, 0, 0, 0),
        new Desc("tbody", 1, 0, 0, 0, 0, 0, 0),
        new Desc("td", 0, 0, 0, 0, 0, 0, 0),
        new Desc("textarea", 0, 0, 0, 0, 0, 0, 1),
        new Desc("tfoot", 0, 1, 0, 0, 0, 0, 0),
        new Desc("th", 0, 1, 0, 0, 0, 0, 0),
        new Desc("thead", 0, 1, 0, 0, 0, 0, 0),
        new Desc("title", 0, 0, 0, 0, 0, 0, 0),
        new Desc("tr", 0, 0, 0, 0, 0, 0, 0),
        new Desc("tt", 0, 3, 0, 0, 0, 0, 1),
        new Desc("u", 0, 3, 0, 0, 1, 1, 1),
        new Desc("ul", 0, 0, 0, 0, 0, 0, 0),
        new Desc("var", 0, 0, 0, 0, 0, 0, 1),
    };

    private static final String[][] START_CLOSE = {
        { "a", "a" }, { "a", "fieldset" }, { "a", "table" }, { "a", "td" }, { "a", "th" },
        { "address", "dd" }, { "address", "dl" }, { "address", "dt" }, { "address", "form" }, { "address", "li" },
        { "address", "ul" }, { "b", "center" }, { "b", "p" }, { "b", "td" }, { "b", "th" },
        { "big", "p" }, { "caption", "col" }, { "caption", "colgroup" }, { "caption", "tbody" }, { "caption", "tfoot" },
        { "caption", "thead" }, { "caption", "tr" }, { "col", "col" }, { "col", "colgroup" }, { "col", "tbody" },
        { "col", "tfoot" }, { "col", "thead" }, { "col", "tr" }, { "colgroup", "colgroup" }, { "colgroup", "tbody" },
        { "colgroup", "tfoot" }, { "colgroup", "thead" }, { "colgroup", "tr" }, { "dd", "dt" }, { "dir", "dd" },
        { "dir", "dl" }, { "dir", "dt" }, { "dir", "form" }, { "dir", "ul" }, { "dl", "form" },
        { "dl", "li" }, { "dt", "dd" }, { "dt", "dl" }, { "font", "center" }, { "font", "td" },
        { "font", "th" }, { "form", "form" }, { "h1", "fieldset" }, { "h1", "form" }, { "h1", "li" },
        { "h1", "p" }, { "h1", "table" }, { "h2", "fieldset" }, { "h2", "form" }, { "h2", "li" },
        { "h2", "p" }, { "h2", "table" }, { "h3", "fieldset" }, { "h3", "form" }, { "h3", "li" },
        { "h3", "p" }, { "h3", "table" }, { "h4", "fieldset" }, { "h4", "form" }, { "h4", "li" },
        { "h4", "p" }, { "h4", "table" }, { "h5", "fieldset" }, { "h5", "form" }, { "h5", "li" },
        { "h5", "p" }, { "h5", "table" }, { "h6", "fieldset" }, { "h6", "form" }, { "h6", "li" },
        { "h6", "p" }, { "h6", "table" }, { "head", "a" }, { "head", "abbr" }, { "head", "acronym" },
        { "head", "address" }, { "head", "b" }, { "head", "bdo" }, { "head", "big" }, { "head", "blockquote" },
        { "head", "body" }, { "head", "br" }, { "head", "center" }, { "head", "cite" }, { "head", "code" },
        { "head", "dd" }, { "head", "dfn" }, { "head", "dir" }, { "head", "div" }, { "head", "dl" },
        { "head", "dt" }, { "head", "em" }, { "head", "fieldset" }, { "head", "font" }, { "head", "form" },
        { "head", "frameset" }, { "head", "h1" }, { "head", "h2" }, { "head", "h3" }, { "head", "h4" },
        { "head", "h5" }, { "head", "h6" }, { "head", "hr" }, { "head", "i" }, { "head", "iframe" },
        { "head", "img" }, { "head", "kbd" }, { "head", "li" }, { "head", "listing" }, { "head", "map" },
        { "head", "menu" }, { "head", "ol" }, { "head", "p" }, { "head", "pre" }, { "head", "q" },
        { "head", "s" }, { "head", "samp" }, { "head", "small" }, { "head", "span" }, { "head", "strike" },
        { "head", "strong" }, { "head", "sub" }, { "head", "sup" }, { "head", "table" }, { "head", "tt" },
        { "head", "u" }, { "head", "ul" }, { "head", "var" }, { "head", "xmp" }, { "hr", "form" },
        { "i", "center" }, { "i", "p" }, { "i", "td" }, { "i", "th" }, { "legend", "fieldset" },
        { "li", "li" }, { "link", "body" }, { "link", "frameset" }, { "listing", "dd" }, { "listing", "dl" },
        { "listing", "dt" }, { "listing", "fieldset" }, { "listing", "form" }, { "listing", "li" }, { "listing", "table" },
        { "listing", "ul" }, { "menu", "dd" }, { "menu", "dl" }, { "menu", "dt" }, { "menu", "form" },
        { "menu", "ul" }, { "ol", "form" }, { "ol", "ul" }, { "option", "optgroup" }, { "option", "option" },
        { "p", "address" }, { "p", "blockquote" }, { "p", "body" }, { "p", "caption" }, { "p", "center" },
        { "p", "col" }, { "p", "colgroup" }, { "p", "dd" }, { "p", "dir" }, { "p", "div" },
        { "p", "dl" }, { "p", "dt" }, { "p", "fieldset" }, { "p", "form" }, { "p", "frameset" },
        { "p", "h1" }, { "p", "h2" }, { "p", "h3" }, { "p", "h4" }, { "p", "h5" },
        { "p", "h6" }, { "p", "head" }, { "p", "hr" }, { "p", "li" }, { "p", "listing" },
        { "p", "menu" }, { "p", "ol" }, { "p", "p" }, { "p", "pre" }, { "p", "table" },
        { "p", "tbody" }, { "p", "td" }, { "p", "tfoot" }, { "p", "th" }, { "p", "title" },
        { "p", "tr" }, { "p", "ul" }, { "p", "xmp" }, { "pre", "dd" }, { "pre", "dl" },
        { "pre", "dt" }, { "pre", "fieldset" }, { "pre", "form" }, { "pre", "li" }, { "pre", "table" },
        { "pre", "ul" }, { "s", "p" }, { "script", "noscript" }, { "small", "p" }, { "span", "td" },
        { "span", "th" }, { "strike", "p" }, { "style", "body" }, { "style", "frameset" }, { "tbody", "tbody" },
        { "tbody", "tfoot" }, { "td", "tbody" }, { "td", "td" }, { "td", "tfoot" }, { "td", "th" },
        { "td", "tr" }, { "tfoot", "tbody" }, { "th", "tbody" }, { "th", "td" }, { "th", "tfoot" },
        { "th", "th" }, { "th", "tr" }, { "thead", "tbody" }, { "thead", "tfoot" }, { "title", "body" },
        { "title", "frameset" }, { "tr", "tbody" }, { "tr", "tfoot" }, { "tr", "tr" }, { "tt", "p" },
        { "u", "p" }, { "u", "td" }, { "u", "th" }, { "ul", "address" }, { "ul", "form" },
        { "ul", "menu" }, { "ul", "ol" }, { "ul", "pre" }, { "xmp", "dd" }, { "xmp", "dl" },
        { "xmp", "dt" }, { "xmp", "fieldset" }, { "xmp", "form" }, { "xmp", "li" }, { "xmp", "table" },
        { "xmp", "ul" },
    };

    private static final Map<String, Desc> BY_NAME = new HashMap<>();
    private static final Set<String> START_CLOSE_SET = new HashSet<>();

    static
    {
        for (Desc d : TABLE) BY_NAME.put(d.name, d);
        for (String[] p : START_CLOSE) START_CLOSE_SET.add(p[0] + " " + p[1]);
    }

    /**
     * Lookup an element description, like libxml2 htmlTagLookup.
     * The lookup is ascii case insensitive.
     * @param tag The tag name.
     * @return The description or null if the tag is not an html 4 element.
     */
    public static Desc lookup(String tag)
    {
        if (tag == null) return null;
        return BY_NAME.get(asciiLower(tag));
    }

    /**
     * Check if the start of newtag implicitly closes oldtag, like libxml2 htmlCheckAutoClose.
     * @param newtag The new tag name.
     * @param oldtag The currently open tag name.
     * @return True if oldtag should be closed.
     */
    public static boolean checkAutoClose(String newtag, String oldtag)
    {
        return START_CLOSE_SET.contains(oldtag + " " + newtag);
    }

    /**
     * Lowercase the ascii letters A-Z only.
     * @param s The string.
     * @return The string with A-Z replaced with a-z.
     */
    static String asciiLower(String s)
    {
        for (int i = 0; i < s.length(); i++)
        {
            char c = s.charAt(i);
            if (c >= 'A' && c <= 'Z')
            {
                char[] cs = s.toCharArray();
                for (int j = i; j < cs.length; j++)
                {
                    if (cs[j] >= 'A' && cs[j] <= 'Z') cs[j] = (char)(cs[j] + 0x20);
                }
                return new String(cs);
            }
        }
        return s;
    }
}
