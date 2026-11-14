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

    // Number of color names (and the FG/BG indices), mirroring colors.h.
    static final int NUM_XMQ_COLOR_NAMES = 15;
    static final int XMQ_COLOR_FG_INDEX = 13;
    static final int XMQ_COLOR_BG_INDEX = 14;
    static final String[] XMQ_COLOR_NAMES = {
        "xmqC", "xmqQ", "xmqE", "xmqNS", "xmqEN", "xmqEK", "xmqEKV",
        "xmqAK", "xmqAKV", "xmqCP", "xmqNSD", "xmqUW", "xmqXSL",
        "xmqFG", "xmqBG"
    };

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
        installDefaultColors();
    }

    /**
     * Parses a color specification like the C {@code string_to_color_def}:
     * an empty string means "no color" (rgb = -1), otherwise a
     * {@code #rrggbb} hex color with optional {@code _B} (bold) and
     * {@code _U} (underline) suffixes.
     */
    private static boolean stringToColorDef(String s, XMQColorDef def)
    {
        def.r = -1;
        def.g = -1;
        def.b = -1;
        def.bold = false;
        def.underline = false;

        if (s.isEmpty()) return true;

        if (s.length() < 7 || s.charAt(0) != '#') return false;
        int r = hex2(s.charAt(1), s.charAt(2));
        int g = hex2(s.charAt(3), s.charAt(4));
        int b = hex2(s.charAt(5), s.charAt(6));
        if (r < 0 || g < 0 || b < 0) return false;

        def.r = r;
        def.g = g;
        def.b = b;

        int i = 7;
        for (int suffix = 0; suffix < 2 && i + 2 <= s.length(); ++suffix)
        {
            if (s.charAt(i) != '_') break;
            if (s.charAt(i+1) == 'B') def.bold = true;
            if (s.charAt(i+1) == 'U') def.underline = true;
            i += 2;
        }
        return i == s.length();
    }

    private static int hex2(char hi, char lo)
    {
        int h = hexDigit(hi);
        if (h < 0) return -1;
        int l = hexDigit(lo);
        if (l < 0) return -1;
        return h * 16 + l;
    }

    private static int hexDigit(char c)
    {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
        if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
        return -1;
    }

    /**
     * Installs the default dark/light color tables, mirroring the C
     * {@code install_default_colors} in default_themes.c.
     */
    void installDefaultColors()
    {
        String[] darkbg = {
            "#2aa1b3", "#26a269_B", "#c061cb", "#a9a9a9", "#ff8c00", "#88b4f7",
            "#26a269_B", "#88b4f7", "#6196ec", "#c061cb", "#2aa1b3",
            "#880000_U", "#c061cb", "", ""
        };
        String[] lightbg = {
            "#2aa1b3", "#26a269_B", "#c061cb", "#696969", "#a86c00", "#0060fd",
            "#26a269_B", "#0060fd", "#12488c", "#c061cb", "#1a91a3",
            "#880000_U", "#c061cb", "", ""
        };
        colors_darkbg = new XMQColorDef[NUM_XMQ_COLOR_NAMES];
        for (int i = 0; i < NUM_XMQ_COLOR_NAMES; ++i)
        {
            colors_darkbg[i] = new XMQColorDef();
            stringToColorDef(darkbg[i], colors_darkbg[i]);
        }
        colors_lightbg = new XMQColorDef[NUM_XMQ_COLOR_NAMES];
        for (int i = 0; i < NUM_XMQ_COLOR_NAMES; ++i)
        {
            colors_lightbg[i] = new XMQColorDef();
            stringToColorDef(lightbg[i], colors_lightbg[i]);
        }
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
     * @param bg 0 = follow the browser preference (class xmq_auto),
     *        1 = forced dark (class xmq_dark), 2 = forced light (class xmq_light).
     * @param use_id The id of the pre element (--id=), or null.
     * @param use_class An extra class of the pre element (--class=), or null.
     */
    public void setupHtmlColoring(boolean render_raw, int bg, String use_id, String use_class)
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

        // Like C, the pre element gets the id and the extra class, if given.
        String pre = "<pre " + (use_id != null ? "id=\"" + use_id + "\" " : "")
            + "class=\"xmq " + bgModeClass(bg) + (use_class != null ? " " + use_class : "") + "\">";

        if (render_raw)
        {
            // The --nostyle render just wraps the content in a single <pre> element.
            content = new XMQThemeStrings(pre, "</pre>");
            return;
        }

        // Build the full html document with a stylesheet. Mirrors the C
        // setup_html_coloring byte-for-byte.
        document = new XMQThemeStrings("<!DOCTYPE html>\n<html>\n", "</html>");
        header = new XMQThemeStrings(
            "<head><meta http-equiv=\"content-type\" content=\"text/html; charset=UTF-8\">"
            + "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1, maximum-scale=5\"><style>",
            "</style></head>");

        StringBuilder stylePre = new StringBuilder();
        stylePre.append("@media screen and (orientation: portrait) { pre { font-size: 2vw; } }")
                .append("@media screen and (orientation: landscape) { pre { max-width: 98%; } }");

        body = new XMQThemeStrings("<body class=\"" + bgModeClass(bg) + "\">", "</body>");

        // Default body background (light mode / no preference).
        XMQColorDef def = colors_lightbg[XMQ_COLOR_BG_INDEX];
        stylePre.append("body{background-color:#");
        if (def.r != -1) stylePre.append(hex(def.r, def.g, def.b));
        else stylePre.append("ffffff");
        stylePre.append(";}\n");

        // CSS for dark mode (applied when the browser prefers dark mode).
        stylePre.append("@media (prefers-color-scheme: dark) {\n");
        stylePre.append("body.xmq_auto{background-color:black;}\n");
        appendPreHtmlColoring(stylePre, true, "xmq_auto");
        stylePre.append("}\n");

        // Forced dark mode (--bg=dark).
        stylePre.append("body.xmq_dark{background-color:black;}\n");
        appendPreHtmlColoring(stylePre, true, "xmq_dark");

        // CSS for light mode (applied when the browser prefers light mode).
        stylePre.append("@media (prefers-color-scheme: light) {\n");
        appendPreHtmlColoring(stylePre, false, "xmq_auto");
        stylePre.append("}\n");

        // Forced light mode (--bg=light).
        appendPreHtmlColoring(stylePre, false, "xmq_light");

        style = new XMQThemeStrings(stylePre.toString(), null);

        content = new XMQThemeStrings(pre, "</pre>");
    }

    /**
     * Append the pre-element CSS for the color class {@code clazz} using
     * either the dark or the light background colors. Mirrors the C
     * {@code append_pre_html_coloring}.
     */
    private void appendPreHtmlColoring(StringBuilder sb, boolean dark, String clazz)
    {
        XMQColorDef[] colors = dark ? colors_darkbg : colors_lightbg;

        sb.append("pre.").append(clazz)
          .append("{white-space:pre-wrap;word-break:break-all;border-radius:2px;background-color:#");

        // The bg color (or default when not overridden).
        XMQColorDef def = colors[XMQ_COLOR_BG_INDEX];
        if (def.r == -1) sb.append(dark ? "263338" : "ffffcc");
        else sb.append(hex(def.r, def.g, def.b));

        sb.append(";border:solid 1px ").append(dark ? "#555555" : "#888888")
          .append(";display:inline-block;padding:1em;color:#");

        // The fg color (or default when not overridden).
        def = colors[XMQ_COLOR_FG_INDEX];
        if (def.r == -1) sb.append(dark ? "ffffff" : "000000");
        else sb.append(hex(def.r, def.g, def.b));

        sb.append(";}\n");

        for (int i = 0; i < NUM_XMQ_COLOR_NAMES; ++i)
        {
            def = colors[i];
            sb.append("pre.").append(clazz).append(' ')
              .append(XMQ_COLOR_NAMES[i])
              .append("{color:#").append(hex(def.r, def.g, def.b)).append(";");
            if (def.bold) sb.append("font-weight:600;");
            if (def.underline) sb.append("text-decoration:underline;");
            sb.append('}');
        }
    }

    private static String hex(int r, int g, int b)
    {
        // Match the C snprintf("%02x%02x%02x") for (possibly) negative values.
        return String.format("%02x%02x%02x", r, g, b);
    }

    /** The body/pre class for a forced background: 0 auto, 1 dark, 2 light. */
    private static String bgModeClass(int bg)
    {
        if (bg == 1) return "xmq_dark";
        if (bg == 2) return "xmq_light";
        return "xmq_auto";
    }

    /** Mirrors C colorShortNameToIndex. */
    static int colorShortNameToIndex(String name)
    {
        String[] names = { "C", "Q", "E", "NS", "EN", "EK", "EKV", "AK", "AKV", "CP", "NSD", "UW", "XSL", "FG", "BG" };
        for (int i = 0; i < names.length; ++i) if (names[i].equals(name)) return i;
        return -1;
    }

    /** Mirrors C install_single_color. */
    private boolean installSingleColor(String name, String color, boolean dark, boolean light)
    {
        int i = colorShortNameToIndex(name);
        if (i < 0) return false;
        boolean ok = true;
        if (dark) ok &= stringToColorDef(color, colors_darkbg[i]);
        if (light) ok &= stringToColorDef(color, colors_lightbg[i]);
        return ok;
    }

    /**
     * Mirrors C install_theme: a theme like {@code EN=#ff0000:EK=#00ff00_B}
     * optionally prefixed with {@code dark+} or {@code light+}.
     */
    private boolean installSingleTheme(String spec)
    {
        boolean dark = true;
        boolean light = true;
        int i = 0;
        int stop = spec.length();
        int plus = spec.indexOf('+');
        if (plus >= 0)
        {
            if (spec.startsWith("dark+")) light = false;
            else if (spec.startsWith("light+")) dark = false;
            else return false;
            i = plus + 1;
        }
        while (i < stop)
        {
            int eq = spec.indexOf('=', i);
            if (eq < 0) return false;
            // All theme names are three character or less.
            if (eq - i > 3) return false;
            String name = spec.substring(i, eq);
            int col = spec.indexOf(':', i);
            if (col < 0) col = stop;
            i = eq + 1;
            if (col - i > 16) return false;
            // Like C, a color is read up to the colon (or the end).
            String color = col >= i ? spec.substring(i, col) : "";
            if (!installSingleColor(name, color, dark, light)) return false;
            i = col + 1;
        }
        return true;
    }

    /**
     * Mirrors C installTheme: installs the default colors, then the theme
     * spec, which is one or two (comma separated) themes.
     *
     * @param spec The theme spec, eg from --theme= or XMQ_THEME.
     * @return True if the spec was understood.
     */
    public boolean installTheme(String spec)
    {
        installDefaultColors();
        if (spec.length() > 2048) return false;
        int comma = spec.indexOf(',');
        String first = comma >= 0 ? spec.substring(0, comma) : spec;
        String second = comma >= 0 ? spec.substring(comma + 1) : "";
        boolean ok = installSingleTheme(first);
        ok &= installSingleTheme(second);
        return ok;
    }

    /** The ansi reset, C NOCOLOR. */
    static final String NOCOLOR = "\033[0m";

    /** Mirrors C generate_ansi_color: a 256 color or truecolor ansi escape. */
    static String ansiColor(XMQColorDef def, boolean truecolor)
    {
        StringBuilder b = new StringBuilder();
        b.append("\033[0;");
        if (def.bold) b.append("1;");
        if (def.underline) b.append("4;");
        if (!truecolor)
        {
            b.append("38;5;").append(rgbToAnsi256(def.r & 0xff, def.g & 0xff, def.b & 0xff)).append('m');
        }
        else
        {
            b.append("38;2;").append(def.r).append(';').append(def.g).append(';').append(def.b).append('m');
        }
        return b.toString();
    }

    private static final int[][] ANSI16 = {
        {0, 0, 0}, {128, 0, 0}, {0, 128, 0}, {128, 128, 0},
        {0, 0, 128}, {128, 0, 128}, {0, 128, 128}, {192, 192, 192},
        {128, 128, 128}, {255, 0, 0}, {0, 255, 0}, {255, 255, 0},
        {0, 0, 255}, {255, 0, 255}, {0, 255, 255}, {255, 255, 255}
    };

    /** Mirrors C ansi256_to_rgb. */
    private static int[] ansi256ToRgb(int idx)
    {
        if (idx < 16) return ANSI16[idx];
        if (idx <= 231)
        {
            int n = idx - 16;
            int[] levels = { 0, 95, 135, 175, 215, 255 };
            return new int[] { levels[n / 36], levels[(n / 6) % 6], levels[n % 6] };
        }
        int gray = 8 + (idx - 232) * 10;
        return new int[] { gray, gray, gray };
    }

    /** Mirrors C rgb_to_ansi256: the nearest ansi 256 color. */
    static int rgbToAnsi256(int r, int g, int b)
    {
        int best_idx = 0;
        int best_dist = Integer.MAX_VALUE;
        for (int i = 0; i < 256; ++i)
        {
            int[] c = ansi256ToRgb(i);
            int dr = r - c[0], dg = g - c[1], db = b - c[2];
            int dist = dr * dr + dg * dg + db * db;
            if (dist < best_dist)
            {
                best_dist = dist;
                best_idx = i;
            }
        }
        return best_idx;
    }

    /**
     * Mirrors C setup_terminal_coloring: ansi colors without post strings,
     * the active color is replayed after a nested color.
     *
     * @param dark_mode Use the dark background colors.
     * @param use_color If false, no colors are installed.
     * @param truecolor Generate 24 bit colors instead of the 256 color palette.
     */
    public void setupTerminalColoring(boolean dark_mode, boolean use_color, boolean truecolor)
    {
        if (!use_color) return;
        XMQColorDef[] colors = dark_mode ? colors_darkbg : colors_lightbg;

        XMQThemeStrings c = new XMQThemeStrings(ansiColor(colors[0], truecolor), null);
        comment = c;
        comment_continuation = c;
        quote = new XMQThemeStrings(ansiColor(colors[1], truecolor), null);
        c = new XMQThemeStrings(ansiColor(colors[2], truecolor), null);
        entity = c;
        element_value_entity = c;
        element_value_compound_entity = c;
        attr_value_entity = c;
        attr_value_compound_entity = c;
        c = new XMQThemeStrings(ansiColor(colors[3], truecolor), null);
        element_ns = c;
        attr_ns = c;
        element_name = new XMQThemeStrings(ansiColor(colors[4], truecolor), null);
        element_key = new XMQThemeStrings(ansiColor(colors[5], truecolor), null);
        c = new XMQThemeStrings(ansiColor(colors[6], truecolor), null);
        element_value_text = c;
        element_value_quote = c;
        element_value_compound_quote = c;
        attr_key = new XMQThemeStrings(ansiColor(colors[7], truecolor), null);
        c = new XMQThemeStrings(ansiColor(colors[8], truecolor), null);
        attr_value_text = c;
        attr_value_quote = c;
        attr_value_compound_quote = c;
        c = new XMQThemeStrings(ansiColor(colors[9], truecolor), null);
        cpar_left = c;
        cpar_right = c;
        ns_declaration = new XMQThemeStrings(ansiColor(colors[10], truecolor), null);
        unicode_whitespace = new XMQThemeStrings(ansiColor(colors[11], truecolor), null);
        ns_override_xsl = new XMQThemeStrings(ansiColor(colors[12], truecolor), null);

        c = new XMQThemeStrings(NOCOLOR, null);
        whitespace = c;
        equals = c;
        brace_left = c;
        brace_right = c;
        apar_left = c;
        apar_right = c;
        ns_colon = c;

        document = new XMQThemeStrings(null, NOCOLOR);
    }

    /**
     * Mirrors C setup_tex_coloring.
     *
     * @param dark_mode Use the bold/underline settings of the dark background colors.
     * @param render_raw If true (--nostyle), only print the content without a document.
     */
    public void setupTexColoring(boolean dark_mode, boolean render_raw)
    {
        XMQColorDef[] colors = dark_mode ? colors_darkbg : colors_lightbg;
        if (!render_raw)
        {
            document = new XMQThemeStrings(
                "\\documentclass[10pt,a4paper]{article}\n"
                + "\\usepackage{color}\n"
                + "\\usepackage{bold-extra}\n", null);
            StringBuilder b = new StringBuilder();
            for (int i = 0; i < NUM_XMQ_COLOR_NAMES; ++i)
            {
                XMQColorDef d = colors_lightbg[i];
                if (d.r < 0) b.append("\\definecolor{").append(XMQ_COLOR_NAMES[i]).append("}{RGB}{0,0,0}");
                else b.append("\\definecolor{").append(XMQ_COLOR_NAMES[i]).append("}{RGB}{")
                      .append(d.r).append(',').append(d.g).append(',').append(d.b).append('}');
                b.append('\n');
            }
            for (int i = 0; i < NUM_XMQ_COLOR_NAMES; ++i)
            {
                String n = XMQ_COLOR_NAMES[i];
                b.append("\\newcommand{\\").append(n).append("}[1]{{\\color{").append(n).append('}')
                 .append(colors[i].bold ? "\\textbf{" : "")
                 .append(colors[i].underline ? "\\underline{" : "")
                 .append("#1")
                 .append(colors[i].bold ? "}" : "")
                 .append(colors[i].underline ? "}" : "")
                 .append("}}\n");
            }
            b.append("\\newcommand{\\xmqI}[0]{{\\mbox{\\ }}}\n");
            style = new XMQThemeStrings(b.toString(), null);
            body = new XMQThemeStrings("\n\\begin{document}\n", "\n\\end{document}\n");
        }

        content = new XMQThemeStrings("\\texttt{\\flushleft\\noindent ", "\n}\n");
        whitespace = null;
        indentation_whitespace = null;
        equals = null;
        brace_left = null;
        brace_right = null;
        apar_left = null;
        apar_right = null;
        ns_colon = null;
        unicode_whitespace = tex("xmqUW");
        cpar_left = tex("xmqCP");
        cpar_right = tex("xmqCP");
        quote = tex("xmqQ");
        entity = tex("xmqE");
        comment = tex("xmqC");
        comment_continuation = tex("xmqC");
        element_ns = tex("xmqNS");
        element_name = tex("xmqEN");
        element_key = tex("xmqEK");
        element_value_text = tex("xmqEKV");
        element_value_quote = tex("xmqEKV");
        element_value_entity = tex("xmqE");
        element_value_compound_quote = tex("xmqEKV");
        element_value_compound_entity = tex("xmqE");
        attr_ns = tex("xmqNS");
        attr_key = tex("xmqAK");
        attr_value_text = tex("xmqAKV");
        attr_value_quote = tex("xmqAKV");
        attr_value_entity = tex("xmqE");
        attr_value_compound_quote = tex("xmqAKV");
        attr_value_compound_entity = tex("xmqE");
        ns_declaration = tex("xmqNSD");
        ns_override_xsl = tex("xmqXSL");
    }

    private static XMQThemeStrings tex(String name)
    {
        return new XMQThemeStrings("\\" + name + "{", "}");
    }
}
