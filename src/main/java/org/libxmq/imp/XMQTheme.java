/* libxmq - Copyright (C) 2025 Fredrik Öhrström (spdx: MIT)

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

/**
   XMQTheme

   The theme struct is used to prefix/postfix ANSI/HTML/TEX strings for
   XMQ tokens to colorize the printed xmq output.
*/
public class XMQTheme
{
    /**
     * Creates an empty theme.
     */
    public XMQTheme()
    {
    }

    String name;
    String indentation_space;
    String explicit_space;
    String explicit_nl;
    String explicit_tab;
    String explicit_cr;

    XMQThemeStrings document; // <html></html>  \documentclass{...}... etc
    XMQThemeStrings header; // <head>..</head>
    XMQThemeStrings style;  // Stylesheet content inside header (html) or color(tex) definitions.
    XMQThemeStrings body; // <body></body> \begin{document}\end{document}
    XMQThemeStrings content; // Wrapper around rendered code. Like <pre></pre>, \textt{...}

    XMQThemeStrings whitespace; // The normal whitespaces: space=32. Normally not colored.
    XMQThemeStrings unicode_whitespace; // The remaining unicode whitespaces, like: nbsp=160 color as red underline.
    XMQThemeStrings indentation_whitespace; // The xmq generated indentation spaces. Normally not colored.
    XMQThemeStrings equals; // The key = value equal sign.
    XMQThemeStrings brace_left; // Left brace starting a list of childs.
    XMQThemeStrings brace_right; // Right brace ending a list of childs.
    XMQThemeStrings apar_left; // Left parentheses surrounding attributes. foo(x=1)
    XMQThemeStrings apar_right; // Right parentheses surrounding attributes.
    XMQThemeStrings cpar_left; // Left parentheses surrounding a compound value. foo = (&#10;' x '&#10;)
    XMQThemeStrings cpar_right; // Right parentheses surrounding a compound value.
    XMQThemeStrings quote; // A quote 'x y z' can be single or multiline.
    XMQThemeStrings entity; // A entity &#10;
    XMQThemeStrings comment; // A comment // foo or /* foo */
    XMQThemeStrings comment_continuation; // A comment containing newlines /* Hello */* there */
    XMQThemeStrings ns_colon; // The color of the colon separating a namespace from a name.
    XMQThemeStrings element_ns; // The namespace part of an element tag, i.e. the text before colon in foo:alfa.
    XMQThemeStrings element_name; // When an element tag has multiple children or attributes it is rendered using this color.
    XMQThemeStrings element_key; // When an element tag is suitable to be presented as a key value, this color is used.
    XMQThemeStrings element_value_text; // When an element is presented as a key and the value is presented as text, use this color.
    XMQThemeStrings element_value_quote; // When the value is a single quote, use this color.
    XMQThemeStrings element_value_entity; // When the value is a single entity, use this color.
    XMQThemeStrings element_value_compound_quote; // When the value is compounded and this is a quote in the compound.
    XMQThemeStrings element_value_compound_entity; // When the value is compounded and this is an entity in the compound.
    XMQThemeStrings attr_ns; // The namespace part of an attribute name, i.e. the text before colon in bar:speed.
    XMQThemeStrings attr_key; // The color of the attribute name, i.e. the key.
    XMQThemeStrings attr_value_text; // When the attribute value is text, use this color.
    XMQThemeStrings attr_value_quote; // When the attribute value is a quote, use this color.
    XMQThemeStrings attr_value_entity; // When the attribute value is an entity, use this color.
    XMQThemeStrings attr_value_compound_quote; // When the attribute value is a compound and this is a quote in the compound.
    XMQThemeStrings attr_value_compound_entity; // When the attribute value is a compound and this is an entity in the compound.
    XMQThemeStrings ns_declaration; // The xmlns part of an attribute namespace declaration.
    XMQThemeStrings ns_override_xsl; // Override key/name colors for elements with xsl namespace.

    // RGB Sources + bold + underline from which we can configure the strings.
    XMQColorDef[] colors_darkbg; // NUM_XMQ_COLOR_NAMES
    XMQColorDef[] colors_lightbg;


    void installDefault()
    {
        name = "default";
        indentation_space = " ";
        explicit_space = " ";
        explicit_nl = "\n";
        explicit_tab = "\t";
        explicit_cr = "\r";
    }

    /**
     * Returns the pre/post color strings for the given color token, or null
     * if no string is installed for that token. Mirrors the C
     * {@code getThemeStrings}.
     *
     * @param color The token color.
     * @return The pre/post strings, or null if not set.
     */
    public XMQThemeStrings getStringsFor(XMQColor color)
    {
        switch (color)
        {
        case COLOR_whitespace: return whitespace;
        case COLOR_unicode_whitespace: return unicode_whitespace;
        case COLOR_indentation_whitespace: return indentation_whitespace;
        case COLOR_equals: return equals;
        case COLOR_brace_left: return brace_left;
        case COLOR_brace_right: return brace_right;
        case COLOR_apar_left: return apar_left;
        case COLOR_apar_right: return apar_right;
        case COLOR_cpar_left: return cpar_left;
        case COLOR_cpar_right: return cpar_right;
        case COLOR_quote: return quote;
        case COLOR_entity: return entity;
        case COLOR_comment: return comment;
        case COLOR_comment_continuation: return comment_continuation;
        case COLOR_ns_colon: return ns_colon;
        case COLOR_element_ns: return element_ns;
        case COLOR_element_name: return element_name;
        case COLOR_element_key: return element_key;
        case COLOR_element_value_text: return element_value_text;
        case COLOR_element_value_quote: return element_value_quote;
        case COLOR_element_value_entity: return element_value_entity;
        case COLOR_element_value_compound_quote: return element_value_compound_quote;
        case COLOR_element_value_compound_entity: return element_value_compound_entity;
        case COLOR_attr_ns: return attr_ns;
        case COLOR_attr_key: return attr_key;
        case COLOR_attr_value_text: return attr_value_text;
        case COLOR_attr_value_quote: return attr_value_quote;
        case COLOR_attr_value_entity: return attr_value_entity;
        case COLOR_attr_value_compound_quote: return attr_value_compound_quote;
        case COLOR_attr_value_compound_entity: return attr_value_compound_entity;
        case COLOR_ns_declaration: return ns_declaration;
        case COLOR_ns_override_xsl: return ns_override_xsl;
        default: return null;
        }
    }

    /**
     * Configures this theme for {@code render-html}. Mirrors the C
     * {@code setup_html_coloring}: the token color strings are set for both
     * the styled and the raw ({@code --nostyle}) render, and the
     * document/stylesheet scaffolding is only added when not raw.
     *
     * @param render_raw If true (the {@code --nostyle} option), only wrap the
     *        content in a single {@code <pre>} element without a full document.
     * @param dark_mode If true, use the dark background color scheme in the css.
     */
    public void setupHtmlColoring(boolean render_raw, boolean dark_mode)
    {
        // Token color strings: identical for the styled and the raw render.
        whitespace = null;
        indentation_whitespace = null;
        equals = null;
        brace_left = null;
        brace_right = null;
        apar_left = null;
        apar_right = null;
        ns_colon = null;
        unicode_whitespace      = new XMQThemeStrings("<xmqUW>", "</xmqUW>");
        cpar_left = new XMQThemeStrings("<xmqCP>", "</xmqCP>");
        cpar_right = new XMQThemeStrings("<xmqCP>", "</xmqCP>");
        quote = new XMQThemeStrings("<xmqQ>", "</xmqQ>");
        entity = new XMQThemeStrings("<xmqE>", "</xmqE>");
        comment = new XMQThemeStrings("<xmqC>", "</xmqC>");
        comment_continuation = new XMQThemeStrings("<xmqC>", "</xmqC>");
        element_ns = new XMQThemeStrings("<xmqNS>", "</xmqNS>");
        element_name = new XMQThemeStrings("<xmqEN>", "</xmqEN>");
        element_key = new XMQThemeStrings("<xmqEK>", "</xmqEK>");
        element_value_text = new XMQThemeStrings("<xmqEKV>", "</xmqEKV>");
        element_value_quote = new XMQThemeStrings("<xmqEKV>", "</xmqEKV>");
        element_value_entity = new XMQThemeStrings("<xmqE>", "</xmqE>");
        element_value_compound_quote = new XMQThemeStrings("<xmqEKV>", "</xmqEKV>");
        element_value_compound_entity = new XMQThemeStrings("<xmqE>", "</xmqE>");
        attr_ns = new XMQThemeStrings("<xmqNS>", "</xmqNS>");
        attr_key = new XMQThemeStrings("<xmqAK>", "</xmqAK>");
        attr_value_text = new XMQThemeStrings("<xmqAKV>", "</xmqAKV>");
        attr_value_quote = new XMQThemeStrings("<xmqAKV>", "</xmqAKV>");
        attr_value_entity = new XMQThemeStrings("<xmqE>", "</xmqE>");
        attr_value_compound_quote = new XMQThemeStrings("<xmqAKV>", "</xmqAKV>");
        attr_value_compound_entity = new XMQThemeStrings("<xmqE>", "</xmqE>");
        ns_declaration = new XMQThemeStrings("<xmqNSD>", "</xmqNSD>");
        ns_override_xsl = new XMQThemeStrings("<xmqXSL>", "</xmqXSL>");

        if (render_raw)
        {
            // The --nostyle render just wraps the content in a single <pre> element.
            content = new XMQThemeStrings("<pre class=\"xmq xmq_auto\">", "</pre>");
            return;
        }

        // Build the full html document with a stylesheet.
        XMQColorDef[] light = colors_lightbg;
        XMQColorDef[] dark = colors_darkbg;

        StringBuilder css = new StringBuilder();
        css.append("    .xmq_auto, .xmq_light { font-family:courier,monospace,'dejavu sans mono','DejaVu Sans Mono';}\n");
        if (dark_mode)
        {
            css.append("    .xmq_auto, .xmq_light { background-color:#141414;color:white; }\n");
        }
        else
        {
            css.append("    .xmq_auto, .xmq_light, .xmq_dark { background-color:white;color:black; }\n");
        }
        appendTokenCss(css, "xmqC",  light[0],  dark[0]);
        appendTokenCss(css, "xmqQ",  light[1],  dark[1]);
        appendTokenCss(css, "xmqE",  light[2],  dark[2]);
        appendTokenCss(css, "xmqNS", light[3],  dark[3]);
        appendTokenCss(css, "xmqEN", light[4],  dark[4]);
        appendTokenCss(css, "xmqEK", light[5],  dark[5]);
        appendTokenCss(css, "xmqEKV",light[6],  dark[6]);
        appendTokenCss(css, "xmqAK", light[7],  dark[7]);
        appendTokenCss(css, "xmqAKV",light[8],  dark[8]);
        appendTokenCss(css, "xmqNSD",light[10], dark[10]);
        appendTokenCss(css, "xmqCP", light[9],  dark[9]);
        appendTokenCss(css, "xmqUW", light[11], dark[11]);
        appendTokenCss(css, "xmqXSL",light[12], dark[12]);

        String docPre = "<!DOCTYPE html>\n<html>\n  <head>\n    <title>xmq render</title>\n"
            + "    <style>\n" + css.toString()
            + "</style>\n  </head>\n  <body>\n";
        document = new XMQThemeStrings(docPre, "</body>\n</html>\n");
        content = new XMQThemeStrings("<pre class=\"xmq xmq_auto\">", "</pre>");
    }

    private static String colorDecl(XMQColorDef c)
    {
        StringBuilder s = new StringBuilder("color:rgb(");
        s.append(c.r).append(",").append(c.g).append(",").append(c.b).append(");");
        if (c.bold)
        {
            s.append("font-weight:bold;");
        }
        if (c.underline)
        {
            s.append("text-decoration:underline;");
        }
        return s.toString();
    }

    private static void appendTokenCss(StringBuilder sb, String cls, XMQColorDef light, XMQColorDef dark)
    {
        if (light != null)
        {
            sb.append("    .").append(cls).append(".xmq_auto, .").append(cls)
              .append(".xmq_light { ").append(colorDecl(light)).append(" }\n");
        }
        if (dark != null)
        {
            sb.append("    .").append(cls).append(".xmq_dark { background-color:#141414;")
              .append(colorDecl(dark)).append(" }\n");
        }
    }
}
