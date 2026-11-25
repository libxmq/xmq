/* libxmq - Copyright (C) 2023-2026 Fredrik Öhrström (spdx: MIT) */

/**
 * Printing a W3C DOM as xmq. A port of the Java XMQPrinter and XMQPrintState (a port of
 * the printer of the C xmq.c), with a `// Java name` comment on every function. It is
 * not a complete port:
 * - Colors, themes and rendering (html, tex, terminal) are left out. The Java printer
 *   wraps every printed part in the pre and post strings of its color; without a theme
 *   these are empty, so print_utf8 here just prints the parts.
 * - The compact form is not printed yet, but the compact branches are kept so that it
 *   can be enabled later (the compact setting is always false).
 * - print_nodes does not recurse into the elements, it keeps its own stack, since the
 *   stack of node and the browsers is too small for 10000 nested elements.
 * A DOM that was not parsed from xmq, eg the DOM of a browser page, is printed trimmed,
 * like C trims the xml and html it reads (xmqTrimWhitespace, Java XmlTrim): the DOM is
 * not changed, the printer skips the whitespace only text and prints the text without
 * its incidental indentation (see trimmed_text). The elements of an html document are
 * printed with their localName, a browser has the nodeName in upper case.
 * The W3C DOM has no entity reference nodes: the text nodes that the xmq parser created
 * for entities (entityName) are printed as entities, and the attribute values with
 * entities or without a value use the parts the parser remembered (attrParts).
 */

import { potentially_add_leading_ending_space } from "./dom.js";
import { attrParts, doctypeText, entityName, isXmqDocument } from "./marks.js";
import { unQuote } from "./trim.js";

const XMLNS_NS = "http://www.w3.org/2000/xmlns/";
const XHTML_NS = "http://www.w3.org/1999/xhtml";

/** The options of print, like the to-xmq options of the xmq command line. */
export interface PrintOptions
{
    /** The number of spaces to indent the content of an element, default 4 (--indent=n). */
    indent?: number;
    /** Print newlines inside quotes as &#10; (--escape-newlines). */
    escapeNewlines?: boolean;
    /** Print the characters above 126 as character references (--escape-non-7bit). */
    escapeNon7bit?: boolean;
    /** Print tabs as &#9; (--escape-tabs). */
    escapeTabs?: boolean;
    /** Use double quotes when single and double quotes are equally good (--prefer-double-quotes). */
    preferDoubleQuotes?: boolean;
    /**
     * Trim the whitespace like xmq does when it reads xml or html (the default --trim):
     * skip the text nodes with only whitespace, remove the incidental indentation of the
     * text and the comments, and the leading and ending spaces of the comments. The DOM
     * is not changed. The default is to trim, unless the document was parsed from xmq by
     * this library (xmq is never trimmed by C either).
     */
    trim?: boolean;
}

/** Java XMQPrinter.Level, C Level. */
enum Level
{
    XMQ,
    ELEMENT_VALUE,
    ELEMENT_VALUE_COMPOUND,
    ATTR_VALUE,
    ATTR_VALUE_COMPOUND,
}

/** A part of a value: text, or an entity reference (Java: a text or entity node). */
type ValuePart = { text: string } | { entity: string };

/** Java XMQPrintState with the OutputSettings that the printer uses. */
export class XMQPrintState
{
    current_indent = 0;
    line_indent = 0;
    last_char = ""; // "" is the C 0, nothing printed yet on this line.
    restart_line: string | null = null;
    // The strings printed for indentation and explicit whitespace, the defaults of C.
    indentation_space = " ";
    explicit_space = " ";
    explicit_tab = "\t";
    explicit_nl = "\n";
    buffer = "";
    // The output settings.
    compact = false;
    indent_amount: number;
    escape_newlines: boolean;
    escape_non_7bit: boolean;
    escape_tabs: boolean;
    prefer_double_quotes: boolean;
    // Print as trimmed, the trimmed text of the text and comment nodes, null if skipped.
    trim: boolean;
    trimmed = new Map<Node, string | null>();

    constructor(o: PrintOptions, trim: boolean)
    {
        this.trim = trim;
        this.indent_amount = o.indent ?? 4;
        this.escape_newlines = o.escapeNewlines ?? false;
        this.escape_non_7bit = o.escapeNon7bit ?? false;
        this.escape_tabs = o.escapeTabs ?? false;
        this.prefer_double_quotes = o.preferDoubleQuotes ?? false;
    }
}

/** The utf-16 code unit at i as a number, like a Java char. */
function cu(s: string, i: number): number
{
    return s.charCodeAt(i);
}

/** The number of utf-16 code units of the code point at i, Java Character.charCount. */
function char_count(c: number): number
{
    return c > 0xffff ? 2 : 1;
}

/** Java XMQPrinter.write: a raw write that does not track the indentation. */
export function write(ps: XMQPrintState, s: string | null): void
{
    if (s !== null) ps.buffer += s;
}

/**
 * The text of a text, cdata or comment node as it is printed: with trimming the text as C
 * xmqTrimWhitespace leaves it (trim_text_node, Java XmlTrim.trim_text_node), or null if
 * C removes the node. The entity text nodes of the xmq parser are never trimmed.
 */
export function trimmed_text(ps: XMQPrintState, n: Node): string | null
{
    const data = (n as CharacterData).data;
    if (!ps.trim || entityName(n) !== null) return data;
    const cached = ps.trimmed.get(n);
    if (cached !== undefined) return cached;
    const t = trim_content(data, n.nodeType === 8);
    ps.trimmed.set(n, t);
    return t;
}

/** C trim_text_node: the trimmed text of a text or a comment, or null if it is removed. */
function trim_content(data: string, is_comment: boolean): string | null
{
    // We remove any all whitespace node.
    if (/^[ \t\n\r]*$/.test(data)) return null;
    let start = 0;
    let stop = data.length;
    if (is_comment)
    {
        // For comments we always remove leading and ending spaces.
        while (start < stop && data.charAt(start) === " ") start++;
        while (stop > start && data.charAt(stop - 1) === " ") stop--;
    }
    // Remove the incidental indentation, like xmq_un_quote(start, stop, false, false).
    const t = unQuote(data, start, stop, false, false);
    return t === "" ? null : t;
}

/**
 * True if the node is not printed: it is removed by the trimming, or it is whitespace
 * directly in a document. A W3C Document cannot have text (xmldom keeps the whitespace
 * between the top level nodes as text, a browser does not), C has none either.
 */
export function skipped(ps: XMQPrintState, n: Node): boolean
{
    const k = n.nodeType;
    if (k !== 3 && k !== 4 && k !== 8) return false;
    if (k !== 8 && n.parentNode !== null && n.parentNode.nodeType === 9 && /^[ \t\n\r]*$/.test((n as CharacterData).data)) return true;
    return ps.trim && trimmed_text(ps, n) === null;
}

/** The first child that is printed. */
export function first_child(ps: XMQPrintState, n: Node): Node | null
{
    let c = n.firstChild;
    while (c !== null && skipped(ps, c)) c = c.nextSibling;
    return c;
}

/** The next sibling that is printed. */
export function next_sibling(ps: XMQPrintState, n: Node): Node | null
{
    let c = n.nextSibling;
    while (c !== null && skipped(ps, c)) c = c.nextSibling;
    return c;
}

/** The previous sibling that is printed. */
export function prev_sibling(ps: XMQPrintState, n: Node): Node | null
{
    let c = n.previousSibling;
    while (c !== null && skipped(ps, c)) c = c.previousSibling;
    return c;
}

/** The last child that is printed. */
export function last_child(ps: XMQPrintState, n: Node): Node | null
{
    let c = n.lastChild;
    while (c !== null && skipped(ps, c)) c = c.previousSibling;
    return c;
}

/**
 * The name of an element as xmq prints it, prefix:name. An html element (in the xhtml
 * namespace without a prefix) has its localName, the nodeName is in upper case in the
 * html document of a browser.
 */
export function element_name(e: Element): string
{
    return e.namespaceURI === XHTML_NS && e.prefix === null && e.localName !== null ? e.localName : e.nodeName;
}

/** Java XMQPrinter.is_unicode_whitespace: nbsp and the en/em quads and spaces. */
function is_unicode_whitespace(c: number): boolean
{
    return c === 0xa0 || (c >= 0x2000 && c <= 0x2003);
}

/** Java XMQPrinter.print_utf8_char: print the character at i, returns the number of code units used. */
function print_utf8_char(ps: XMQPrintState, s: string, i: number): number
{
    const c = s.codePointAt(i)!;
    if (c === 0x20) write(ps, ps.explicit_space);
    else if (c === 0x09) write(ps, ps.explicit_tab);
    else ps.buffer += String.fromCodePoint(c);
    ps.last_char = s.charAt(i);
    ps.current_indent++;
    return char_count(c);
}

/** Java XMQPrinter.print_utf8_internal */
function print_utf8_internal(ps: XMQPrintState, s: string | null): void
{
    if (s === null || s.length === 0) return;
    let u_len = 0;
    for (let i = 0; i < s.length; )
    {
        const c = s.codePointAt(i)!;
        if (c === 0x20) write(ps, ps.explicit_space);
        else if (c === 0x09) write(ps, ps.explicit_tab);
        else ps.buffer += String.fromCodePoint(c);
        u_len++;
        i += char_count(c);
    }
    ps.last_char = s.charAt(s.length - 1);
    ps.current_indent += u_len;
}

/** Java XMQPrinter.print_utf8, without the color. */
function print_utf8(ps: XMQPrintState, ...parts: string[]): void
{
    for (const p of parts) print_utf8_internal(ps, p);
}

/** Java XMQPrinter.check_space_before_entity_node */
function check_space_before_entity_node(ps: XMQPrintState): void
{
    const c = ps.last_char;
    if (c === "(") return;
    if (!ps.compact && c !== "=")
    {
        print_nl_and_indent(ps, null, null);
    }
    else if (need_separation_before_entity(ps))
    {
        print_white_spaces(ps, 1);
    }
}

/** Java XMQPrinter.check_space_before_quote */
function check_space_before_quote(ps: XMQPrintState, level: Level): void
{
    const c = ps.last_char;
    if (c === "") return;
    if (!ps.compact && (c !== "=" || level === Level.XMQ) && c !== "(")
    {
        print_nl_and_indent(ps, null, null);
    }
    else if (need_separation_before_quote(c))
    {
        print_white_spaces(ps, 1);
    }
}

/** Java XMQPrinter.need_separation_before_quote: the previous char is a quote. */
function need_separation_before_quote(c: string): boolean
{
    return c === "'" || c === "\"";
}

/** Java XMQPrinter.print_white_spaces: indentation, printed with the indentation space string. */
export function print_white_spaces(ps: XMQPrintState, num: number): void
{
    if (num > 0) write(ps, ps.indentation_space.repeat(num));
    ps.current_indent += num;
}

/** Java XMQPrinter.print_nl */
function print_nl(ps: XMQPrintState, prefix: string | null, postfix: string | null): void
{
    write(ps, postfix);
    write(ps, ps.explicit_nl);
    ps.current_indent = 0;
    ps.last_char = "";
    write(ps, ps.restart_line);
    write(ps, prefix);
}

/** Java XMQPrinter.print_nl_and_indent */
export function print_nl_and_indent(ps: XMQPrintState, prefix: string | null, postfix: string | null): void
{
    write(ps, postfix);
    write(ps, ps.explicit_nl);
    ps.current_indent = 0;
    ps.last_char = "";
    print_white_spaces(ps, ps.line_indent);
    write(ps, ps.restart_line);
    write(ps, prefix);
}

/** Java XMQPrinter.check_space_before_key: a newline and indentation before an element key. */
function check_space_before_key(ps: XMQPrintState): void
{
    const c = ps.last_char;
    if (c === "") return;

    if (!ps.compact)
    {
        print_nl_and_indent(ps, null, null);
    }
    else if (need_separation_before_element_name(ps))
    {
        print_white_spaces(ps, 1);
    }
}

/** Java XMQPrinter.need_separation_before_element_name */
function need_separation_before_element_name(ps: XMQPrintState): boolean
{
    const c = ps.last_char;
    return c !== "" && c !== "'" && c !== "\"" && c !== "{" && c !== "}" && c !== ";" && c !== ")" && c !== "/";
}

/** Java XMQPrinter.need_separation_before_entity */
function need_separation_before_entity(ps: XMQPrintState): boolean
{
    const c = ps.last_char;
    return c !== "" && c !== "=" && c !== "'" && c !== "\"" && c !== "{" && c !== "}" && c !== ";" && c !== "(" && c !== ")";
}

/** Java XMQPrinter.check_space_before_attribute */
function check_space_before_attribute(ps: XMQPrintState): void
{
    const c = ps.last_char;
    if (c === "(") return;
    if (!ps.compact)
    {
        print_nl_and_indent(ps, null, null);
    }
    else if (need_separation_before_attribute_key(ps))
    {
        print_white_spaces(ps, 1);
    }
}

/**
 * Java XMQPrinter.need_separation_before_attribute_key: no space is needed after a quote,
 * an attribute start, a compound end or an entity, ie 'x y z'key=
 */
function need_separation_before_attribute_key(ps: XMQPrintState): boolean
{
    const c = ps.last_char;
    return c !== "" && c !== "'" && c !== "\"" && c !== "(" && c !== ")" && c !== ";";
}

/** Java XMQPrinter.check_space_before_opening_brace */
function check_space_before_opening_brace(ps: XMQPrintState): void
{
    const c = ps.last_char;

    if (!ps.compact)
    {
        if (c === ")")
        {
            print_nl_and_indent(ps, null, null);
        }
        else
        {
            print_white_spaces(ps, 1);
        }
    }
}

/** Java XMQPrinter.check_space_before_closing_brace */
function check_space_before_closing_brace(ps: XMQPrintState): void
{
    if (!ps.compact)
    {
        print_nl_and_indent(ps, null, null);
    }
}

/**
 * True if the attribute is a namespace declaration, xmlns or xmlns:prefix in the xmlns
 * namespace (Java keeps them apart from the attributes, as nsDefs). In an html document an
 * xmlns attribute has no namespace, it is an attribute like any other, also in C.
 */
function is_ns_declaration(a: Attr): boolean
{
    return a.namespaceURI === XMLNS_NS;
}

/** The attributes of an element that are not namespace declarations (Java XmlElement.attributes). */
export function attributes(e: Element): Attr[]
{
    const r: Attr[] = [];
    for (let i = 0; i < e.attributes.length; i++)
    {
        const a = e.attributes[i];
        if (!is_ns_declaration(a)) r.push(a);
    }
    return r;
}

/** The namespace declarations of an element (Java XmlElement.nsDefs). */
export function ns_defs(e: Element): Attr[]
{
    const r: Attr[] = [];
    for (let i = 0; i < e.attributes.length; i++)
    {
        const a = e.attributes[i];
        if (is_ns_declaration(a)) r.push(a);
    }
    return r;
}

/** The prefix of a namespace declaration, null for xmlns (Java XmlNs.prefix). */
export function ns_prefix(a: Attr): string | null
{
    return a.name.startsWith("xmlns:") ? a.name.substring(6) : null;
}

/** Java XMQPrinter.find_attr_key_max_u_width */
function find_attr_key_max_u_width(element: Element): number
{
    let max = 0;
    for (const a of attributes(element))
    {
        const len = u_len(a.name);
        if (len > max) max = len;
    }
    for (const ns of ns_defs(element))
    {
        const prefix = ns_prefix(ns);
        const len = 5 + (prefix !== null ? u_len(prefix) + 1 : 0);
        if (len > max) max = len;
    }
    return max;
}

/** Java XMQPrinter.has_printable_namespace_defs */
function has_printable_namespace_defs(element: Element): boolean
{
    return ns_defs(element).length > 0;
}

/** Java XMQPrinter.u_len: the number of unicode code points. */
function u_len(s: string): number
{
    let n = 0;
    for (let i = 0; i < s.length; i++)
    {
        const c = cu(s, i);
        if (c < 0xdc00 || c > 0xdfff || i === 0 || cu(s, i - 1) < 0xd800 || cu(s, i - 1) > 0xdbff) n++;
    }
    return n;
}

/** Java XMQPrinter.print_attributes */
function print_attributes(ps: XMQPrintState, element: Element): void
{
    let max = 0;
    if (!ps.compact) max = find_attr_key_max_u_width(element);

    const line_indent = ps.line_indent;
    ps.line_indent = ps.current_indent;
    for (const a of attributes(element))
    {
        print_attribute(ps, a, max);
    }
    for (const ns of ns_defs(element))
    {
        print_namespace_declaration(ps, ns, max);
    }
    ps.line_indent = line_indent;
}

/**
 * The value of an attribute as parts (Java: the children of the XmlAttr), the parts that
 * the parser remembered, or the value as one text. An empty value has no parts, like an
 * attribute without a value: both print as a key only, as in C.
 */
function attr_value_parts(a: Attr): ValuePart[]
{
    const parts = attrParts(a);
    if (parts === null) return [];
    if (parts !== undefined) return parts;
    return a.value === "" ? [] : [{ text: a.value }];
}

/** Java XMQPrinter.is_single_empty_text_node */
function is_single_empty_text_node(p: ValuePart): boolean
{
    return "text" in p && p.text === "";
}

/** Java XMQPrinter.print_attribute */
function print_attribute(ps: XMQPrintState, a: Attr, align: number): void
{
    check_space_before_attribute(ps);

    // The qualified name, prefix:key.
    const key = a.name;
    const total_u_len = u_len(key);

    print_utf8(ps, key);

    const parts = attr_value_parts(a);
    if (parts.length > 0 && !is_single_empty_text_node(parts[0]))
    {
        if (!ps.compact) print_white_spaces(ps, 1 + align - total_u_len);
        print_utf8(ps, "=");
        if (!ps.compact) print_white_spaces(ps, 1);
        print_value(ps, parts, Level.ATTR_VALUE);
    }
}

/** Java XMQPrinter.print_namespace_declaration */
function print_namespace_declaration(ps: XMQPrintState, ns: Attr, align: number): void
{
    check_space_before_attribute(ps);

    const prefix = ns_prefix(ns);
    const total_u_len = 5 + (prefix !== null ? u_len(prefix) + 1 : 0);

    print_utf8(ps, "xmlns");

    if (prefix !== null)
    {
        print_utf8(ps, ":");
        print_utf8(ps, prefix);
    }

    const v = ns.value;
    if (!ps.compact) print_white_spaces(ps, 1 + align - total_u_len);
    print_utf8(ps, "=");
    if (!ps.compact) print_white_spaces(ps, 1);
    print_value_string(ps, v, Level.ATTR_VALUE, false);
}

/** Java XMQPrinter.print_content_node */
function print_content_node(ps: XMQPrintState, node: CharacterData): void
{
    // Do NOT trim: leading and ending spaces can be part of the value,
    // ie. key = ' x y z '
    print_value_string(ps, trimmed_text(ps, node)!, Level.XMQ, false);
}

/** Java XMQPrinter.print_entity_node: prints &name; */
function print_entity_node(ps: XMQPrintState, name: string): void
{
    check_space_before_entity_node(ps);
    print_utf8(ps, "&");
    print_utf8(ps, name);
    print_utf8(ps, ";");
}

/** Java XMQPrinter.is_safe_value_char: true if the character does not need quoting. */
function is_safe_value_char(c: number): boolean
{
    return c !== 0x20 && c !== 0x0a && c !== 0x09 && c !== 0x0d
        && c !== 0xa0
        && c !== 0x2000 && c !== 0x2001 && c !== 0x2002 && c !== 0x2003
        && c !== 0x28 && c !== 0x29 && c !== 0x7b && c !== 0x7d
        && c !== 0x27 && c !== 0x22;
}

/** Java XMQPrinter.is_unsafe_value_start: true if the value cannot start with these characters. */
function is_unsafe_value_start(s: string): boolean
{
    if (s.length === 0)
    {
        return true;
    }
    const c = s.charAt(0);
    const cc = s.length > 1 ? s.charAt(1) : "";
    return c === "&" || c === "=" || (c === "/" && (cc === "/" || cc === "*"));
}

/** Java XMQPrinter.is_xmq_text_value: true if the value can be printed without quotes. */
function is_xmq_text_value(s: string): boolean
{
    if (is_unsafe_value_start(s))
    {
        return false;
    }
    for (let i = 0; i < s.length; i++)
    {
        if (!is_safe_value_char(cu(s, i)))
        {
            return false;
        }
    }
    return true;
}

/** The results of count_necessary_quotes. */
interface QuoteCount
{
    max: number;
    add_nls: boolean;
    add_compound: boolean;
    use_double_quotes: boolean;
}

/**
 * Java XMQPrinter.count_necessary_quotes: the number of quotes needed, whether to use
 * double quotes and the multi-line quote format (add_nls).
 */
function count_necessary_quotes(s: string, prefer_double_quotes: boolean): QuoteCount
{
    let all_safe = !is_unsafe_value_start(s);

    // Leading/ending whitespace mixed with newlines force a compound.
    const ls = has_leading_space_nl(s);
    const es = has_ending_nl_space(s, 0, s.length);
    const add_compound = (ls !== null && ls[1] === 0) || (es !== null && es[1] === 0);

    let max_single = 0;
    let curr_single = 0;
    let max_double = 0;
    let curr_double = 0;

    for (let i = 0; i < s.length; i++)
    {
        const c = cu(s, i);
        if (!is_safe_value_char(c))
        {
            all_safe = false;
        }
        if (c === 0x27)
        {
            curr_single++;
            if (curr_single > max_single) max_single = curr_single;
        }
        else
        {
            curr_single = 0;
            if (c === 0x22)
            {
                curr_double++;
                if (curr_double > max_double) max_double = curr_double;
            }
            else
            {
                curr_double = 0;
            }
        }
    }

    const leading_ending_sqs = s.charAt(0) === "'" || s.charAt(s.length - 1) === "'";
    const leading_ending_dqs = s.charAt(0) === "\"" || s.charAt(s.length - 1) === "\"";

    let use_dqs = prefer_double_quotes;
    if (leading_ending_sqs && !leading_ending_dqs)
    {
        // If there is a leading or ending single quote, then use double quotes.
        use_dqs = true;
    }
    else if (!leading_ending_sqs && leading_ending_dqs)
    {
        // If there are leading or ending double quotes, then use single quotes.
        use_dqs = false;
    }
    else if (max_double > max_single && max_double > 0)
    {
        // We have more doubles than singles, use single quotes.
        use_dqs = false;
    }
    else if (max_double < max_single)
    {
        // We have fewer doubles than singles, use double quotes.
        use_dqs = true;
    }
    else if (max_double > 0)
    {
        // Equal number of quotes, then always use single quotes.
        use_dqs = false;
    }

    let max = use_dqs ? max_double : max_single;
    // We found x quotes, thus we need x+1 quotes to quote them.
    if (max > 0) max++;
    // Content contains no quotes ', but has unsafe chars, a single quote is enough.
    if (max === 0 && !all_safe) max = 1;
    // Content contains two sequential '' quotes, must bump number of required quotes to 3.
    if (max === 2) max = 3;

    // If the value has a leading or ending quote of the same kind as
    // chosen, then we need the multi-line quote format.
    const add_nls = (use_dqs && leading_ending_dqs) || (!use_dqs && leading_ending_sqs);

    return { max, add_nls, add_compound, use_double_quotes: use_dqs };
}

/** Java XMQPrinter.enter_compound_level */
function enter_compound_level(l: Level): Level
{
    if (l === Level.ELEMENT_VALUE) return Level.ELEMENT_VALUE_COMPOUND;
    if (l === Level.ATTR_VALUE) return Level.ATTR_VALUE_COMPOUND;
    return l;
}

/** Java XMQPrinter.print_quotes: prints num quotes of the chosen kind. */
function print_quotes(ps: XMQPrintState, num: number, use_double_quotes: boolean): void
{
    // Raw writes of the quotes, like C print_quotes.
    const q = use_double_quotes ? "\"" : "'";
    write(ps, q.repeat(num));
    ps.current_indent += num;
    ps.last_char = q;
}

/** Java XMQPrinter.print_quoted_spaces: prints '...spaces...' */
function print_quoted_spaces(ps: XMQPrintState, num: number): void
{
    write(ps, "'");
    if (num > 0) write(ps, ps.explicit_space.repeat(num));
    ps.current_indent += num;
    ps.last_char = "'";
    write(ps, "'");
}

/** Java XMQPrinter.print_char_entity for a single character. */
function print_char_entity(ps: XMQPrintState, c: number): void
{
    // The html entity names of --escape-non-7bit are only used for htmq, not printed here.
    const buf = "&#" + c + ";";
    print_utf8(ps, buf);
    ps.last_char = ";";
    ps.current_indent += buf.length;
}

/** Java XMQPrinter.print_all_whitespace: spaces are printed as '...', other whitespace as entities. */
function print_all_whitespace(ps: XMQPrintState, s: string, level: Level): void
{
    let i = 0;
    while (i < s.length)
    {
        if (s.charAt(i) === " ")
        {
            let j = i;
            while (j < s.length && s.charAt(j) === " ")
            {
                j++;
            }
            check_space_before_quote(ps, level);
            print_quoted_spaces(ps, j - i);
            i = j;
        }
        else
        {
            check_space_before_entity_node(ps);
            print_char_entity(ps, cu(s, i));
            i++;
        }
    }
}

/** Java XMQPrinter.is_xml_whitespace (exactly space, tab, newline, CR). */
function is_xml_whitespace(c: string): boolean
{
    return c === " " || c === "\t" || c === "\n" || c === "\r";
}

/** Java XMQPrinter.has_all_quotes: all single or all double quotes. */
function has_all_quotes(v: string): boolean
{
    if (v.length === 0)
    {
        return false;
    }
    const first = v.charAt(0);
    if (first !== "'" && first !== "\"")
    {
        return false;
    }
    for (let i = 1; i < v.length; i++)
    {
        if (v.charAt(i) !== first)
        {
            return false;
        }
    }
    return true;
}

/** Java XMQPrinter.has_all_whitespace: [all_whitespace, all_space, only_newlines]. */
function has_all_whitespace(v: string): [boolean, boolean, boolean]
{
    let all_ws = true, all_space = true, only_nls = true;
    for (let i = 0; i < v.length; i++)
    {
        const c = v.charAt(i);
        if (!is_xml_whitespace(c))
        {
            all_ws = false;
            all_space = false;
            only_nls = false;
            break;
        }
        if (c !== " ") all_space = false;
        if (c !== "\n") only_nls = false;
    }
    return [all_ws, all_space, only_nls];
}

/**
 * Java XMQPrinter.has_leading_space_nl: null if no leading whitespace contains newlines,
 * else [the end of the leading whitespace, only_newlines] where only_newlines is the
 * number of leading newlines if they are only newlines, else 0.
 */
function has_leading_space_nl(v: string): [number, number] | null
{
    let i = 0;
    let only_nls = 0;
    while (i < v.length && v.charAt(i) === "\n")
    {
        i++;
        only_nls++;
    }
    const middle = only_nls > 0 ? i : -1;
    let found = only_nls > 0;
    while (i < v.length)
    {
        if (v.charAt(i) === "\n") found = true;
        if (!is_xml_whitespace(v.charAt(i)))
        {
            break;
        }
        i++;
    }
    if (!found)
    {
        return null;
    }
    return [i, middle === i ? only_nls : 0];
}

/**
 * Java XMQPrinter.has_ending_nl_space: null if no ending whitespace contains newlines,
 * else [the new stop, only_newlines] where only_newlines is the number of ending newlines
 * if they are only newlines, else 0.
 */
function has_ending_nl_space(v: string, start: number, stop: number): [number, number] | null
{
    if (start >= stop)
    {
        return null;
    }
    let i = stop;
    let found = false;
    let only_nls = 0;
    i--;
    while (i >= start && v.charAt(i) === "\n")
    {
        i--;
        only_nls++;
        found = true;
    }
    const middle = i;
    while (i >= start)
    {
        if (v.charAt(i) === "\n") found = true;
        if (!is_xml_whitespace(v.charAt(i)))
        {
            break;
        }
        i--;
    }
    if (!found)
    {
        return null;
    }
    const nls = middle === i ? only_nls : 0;
    i++;
    return [i, nls];
}

/** Java XMQPrinter.has_newlines */
function has_newlines(v: string): boolean
{
    return v.indexOf("\n") >= 0;
}

/** Java XMQPrinter.has_leading_ending_different_quotes */
function has_leading_ending_different_quotes(v: string): boolean
{
    if (v.length === 0)
    {
        return false;
    }
    const a = v.charAt(0);
    const b = v.charAt(v.length - 1);
    return (a === "'" && b === "\"") || (a === "\"" && b === "'");
}

/** The use_dquotes in and out argument of quote_needs_compounded. */
interface UseDQuotes
{
    v: boolean;
}

/** Java XMQPrinter.quote_needs_compounded: sets use_dquotes.v on return. */
function quote_needs_compounded(ps: XMQPrintState, v: string, prefer_d: boolean, use_dquotes: UseDQuotes): boolean
{
    const compact = ps.compact;
    const escape_tabs = ps.escape_tabs;

    if (v.length === 1)
    {
        const c = v.charAt(0);
        if (c === "'" || c === "\n" || c === "\r" || c === "\t")
        {
            return false;
        }
    }

    const ls = has_leading_space_nl(v);
    if (ls !== null && ls[1] === 0)
    {
        return true;
    }
    const es = has_ending_nl_space(v, 0, v.length);
    if (es !== null && es[1] === 0)
    {
        return true;
    }

    if (!compact && v.length > 0 && find_next_char_that_needs_escape(ps, v, 0, v.length, use_dquotes.v) < v.length)
    {
        // The quote is split, eg spaces at the end of a line are escaped.
        return true;
    }

    if (compact)
    {
        // In compact form newlines must be escaped: &#10;
        if (has_newlines(v))
        {
            return true;
        }
        // In compact form leading or ending different quotes trigger &#39; escapes.
        if (has_leading_ending_different_quotes(v))
        {
            return true;
        }
    }

    const newlines = ps.escape_newlines;
    const non7bit = ps.escape_non_7bit;
    let num_sq = 0, num_dq = 0;
    let needs = false;

    for (let i = 0; i < v.length; i++)
    {
        const c = cu(v, i);
        if (c === 0x27)
        {
            num_sq++;
            continue;
        }
        if (c === 0x22)
        {
            num_dq++;
            continue;
        }
        if (newlines && c === 0x0a)
        {
            needs = true;
            continue;
        }
        if (non7bit && c > 126)
        {
            needs = true;
            continue;
        }
        if (c < 32 && c !== 0x09 && c !== 0x0a)
        {
            needs = true;
            continue;
        }
        if (c === 0x09 && escape_tabs)
        {
            needs = true;
            continue;
        }
    }

    if (num_dq === 0 && num_sq === 0)
    {
        use_dquotes.v = prefer_d;
    }
    else if (num_dq > 0 && num_sq > 0)
    {
        const a = v.charAt(0);
        const b = v.charAt(v.length - 1);
        if (a === b)
        {
            if (a === "'" || a === "\"")
            {
                use_dquotes.v = (a === "'");
            }
            else
            {
                use_dquotes.v = prefer_d;
            }
        }
        else
        {
            const aq = a === "'" || a === "\"";
            const bq = b === "'" || b === "\"";
            if (aq && bq)
            {
                needs = true;
                use_dquotes.v = prefer_d;
            }
            else if (aq)
            {
                use_dquotes.v = (a === "'");
            }
            else if (bq)
            {
                use_dquotes.v = (b === "'");
            }
            else
            {
                use_dquotes.v = (num_sq > num_dq);
            }
        }
    }
    else
    {
        if (num_sq > 0) use_dquotes.v = true;
        if (num_dq > 0) use_dquotes.v = false;
    }
    return needs;
}

/**
 * Java XMQPrinter.find_next_char_that_needs_escape: the index of the first char at or
 * after from that must be printed as a character entity, or stop if there is none.
 */
function find_next_char_that_needs_escape(ps: XMQPrintState, v: string, from: number, stop: number, using_dquotes: boolean): number
{
    const compact = ps.compact;
    const newlines = ps.escape_newlines;
    const escape_tabs = ps.escape_tabs;
    const non7bit = ps.escape_non_7bit;

    const q = using_dquotes ? 0x22 : 0x27;
    if (from < stop && cu(v, from) === q && compact)
    {
        return from;
    }
    let pre_stop = stop - 1;
    if (compact && pre_stop >= from && cu(v, pre_stop) === q)
    {
        while (pre_stop > from && cu(v, pre_stop) === q)
        {
            pre_stop--;
        }
        pre_stop++;
    }

    // Spaces and tabs at the end of a line in a multiline quote are removed by the parser,
    // so they are escaped. Also at the end of a multiline text where every line is indented,
    // since then the closing quote is put on its own line (see print_quote_lines_and_color_uwhitespace).
    let escape_trailing = false;
    if (!compact && stop > from && (cu(v, stop - 1) === 0x20 || cu(v, stop - 1) === 0x09)
        && v.substring(from, stop).indexOf("\n") >= 0)
    {
        escape_trailing = all_lines_indented(v, from, stop);
    }

    let i = from;
    while (i < stop)
    {
        const c = cu(v, i);
        if (!compact && (c === 0x20 || c === 0x09))
        {
            let j = i;
            while (j < stop && (cu(v, j) === 0x20 || cu(v, j) === 0x09)) j++;
            if (j < stop && (cu(v, j) === 0x0a || cu(v, j) === 0x0d))
            {
                // Only if the newline is inside the quote, ie the next line continues the quote.
                let k = j;
                if (cu(v, k) === 0x0d && k + 1 < stop && cu(v, k + 1) === 0x0a) k++;
                k++;
                if (k < stop)
                {
                    const n = cu(v, k);
                    const next_escaped = (newlines && n === 0x0a) || (non7bit && n > 126)
                        || (n < 32 && n !== 0x09 && n !== 0x0a) || (n === 0x09 && escape_tabs);
                    if (!next_escaped) return i;
                }
            }
            if (j === stop && escape_trailing) return i;
        }
        if ((compact && c === q && i === pre_stop)
            || (newlines && c === 0x0a)
            || (non7bit && c > 126)
            || (c < 32 && c !== 0x09 && c !== 0x0a)
            || (c === 0x09 && escape_tabs))
        {
            return i;
        }
        i++;
    }
    return i;
}

/** Java XMQPrinter.print_safe_leaf_quote: prints one quoted leaf, possibly multiline. */
function print_safe_leaf_quote(ps: XMQPrintState, v: string): void
{
    const compact = ps.compact;
    const qc = count_necessary_quotes(v, ps.prefer_double_quotes);
    let numq = qc.max;
    let add_nls = qc.add_nls;

    if (numq > 0 && !add_nls)
    {
        // A quote that requested its own line (indent -1), or more than 3 quotes when not
        // compact, is printed with leading and ending newlines.
        if (ps.current_indent === -1 || !(numq < 4 || compact))
        {
            add_nls = true;
        }
    }

    if (numq === 0)
    {
        numq = 1;
    }

    let old_line_indent = 0;

    if (add_nls)
    {
        old_line_indent = ps.line_indent;
        ps.line_indent = ps.current_indent;
    }

    print_quotes(ps, numq, qc.use_double_quotes);

    if (!add_nls)
    {
        old_line_indent = ps.line_indent;
        ps.line_indent = ps.current_indent;
    }

    if (add_nls)
    {
        print_nl_and_indent(ps, null, null);
    }

    print_quote_lines_and_color_uwhitespace(ps, v);

    if (!add_nls)
    {
        ps.line_indent = old_line_indent;
    }

    if (add_nls)
    {
        print_nl_and_indent(ps, null, null);
    }

    print_quotes(ps, numq, qc.use_double_quotes);

    if (add_nls)
    {
        ps.line_indent = old_line_indent;
    }
}

/**
 * Java XMQPrinter.continuation_lines_indented: true if the content has lines after the
 * first and every such line with content starts with a space, eg x\n y, then the parser
 * would remove the space as incidental indentation.
 */
function continuation_lines_indented(v: string, start: number, stop: number): boolean
{
    let found = false;
    let i = start;
    while (i < stop && v.charAt(i) !== "\n") i++;
    while (i < stop)
    {
        i++;
        const line_start = i;
        while (i < stop && v.charAt(i) === " ") i++;
        if (i < stop && v.charAt(i) !== "\n" && v.charAt(i) !== "\r")
        {
            if (i === line_start) return false;
            found = true;
        }
        while (i < stop && v.charAt(i) !== "\n") i++;
    }
    return found;
}

/**
 * Java XMQPrinter.all_lines_indented: true if every line of the content with content
 * starts with a space (the leading empty lines are skipped).
 */
function all_lines_indented(v: string, start: number, stop: number): boolean
{
    let found = false;
    let i = start;
    while (i < stop)
    {
        const line_start = i;
        while (i < stop && v.charAt(i) === " ") i++;
        if (i < stop && v.charAt(i) !== "\n" && v.charAt(i) !== "\r")
        {
            if (i === line_start) return false;
            found = true;
        }
        while (i < stop && v.charAt(i) !== "\n") i++;
        if (i < stop) i++;
    }
    return found;
}

/** Java XMQPrinter.print_quote_lines_and_color_uwhitespace: prints the content of a quote. */
function print_quote_lines_and_color_uwhitespace(ps: XMQPrintState, v: string): void
{
    const pre: string | null = null;
    const post: string | null = null;

    const old_restart_line = ps.restart_line;
    ps.restart_line = null;

    // The parser removes the incidental indentation, the smallest indentation of the lines
    // after the first. If every such line with content starts with a space, eg 'x\n y',
    // that space would be removed too. Then the content starts on its own line, where the
    // parser takes the indentation of the closing quote line as the incidental indentation.
    const lead_nl = v.charAt(0) === "\n" || continuation_lines_indented(v, 0, v.length);

    // We are leading with a newline, print an extra into the quote, which will be trimmed away during parse.
    if (v.charAt(0) === "\n")
    {
        print_nl(ps, pre, post);
    }
    else if (lead_nl)
    {
        print_nl_and_indent(ps, pre, post);
    }

    let all_newlines = true;
    for (let i = 0; i < v.length; )
    {
        if (v.charAt(i) === "\n")
        {
            if (i + 1 < v.length && v.charAt(i + 1) !== "\n")
            {
                print_nl_and_indent(ps, pre, post);
            }
            else
            {
                print_nl(ps, pre, post);
            }
            i++;
        }
        else
        {
            i += print_utf8_char(ps, v, i);
            all_newlines = false;
        }
    }
    // We are ending with a newline, print an extra into the quote, which will be trimmed away during parse.
    // When the content started on its own line (lead_nl), the closing quote is aligned with
    // the content, otherwise with the quote. Only newlines, the indentation does not matter.
    if (v.charAt(v.length - 1) === "\n")
    {
        const align_with_quote = !lead_nl || all_newlines;
        if (align_with_quote) ps.line_indent--;
        if (!all_newlines)
        {
            print_nl_and_indent(ps, null, post);
        }
        else
        {
            ps.current_indent = 0;
            ps.last_char = "";
            print_white_spaces(ps, ps.line_indent);
        }
        if (align_with_quote) ps.line_indent++;
    }
    // The content started on its own line and every line is indented, then the closing quote
    // is put on its own line, aligned with the content, to give the incidental indentation.
    // The parser drops this last newline.
    if (lead_nl && v.charAt(v.length - 1) !== "\n" && all_lines_indented(v, 0, v.length))
    {
        print_nl_and_indent(ps, null, post);
    }
    else if (v.charAt(v.length - 1) !== "\n") write(ps, post);
    ps.restart_line = old_restart_line;
}

/** Java XMQPrinter.print_value_internal_text */
function print_value_internal_text(ps: XMQPrintState, v: string, level: Level, using_dquotes: boolean, already: boolean): void
{
    if (v.length === 0)
    {
        check_space_before_quote(ps, level);
        print_utf8(ps, "''");
        return;
    }

    if (has_all_quotes(v))
    {
        // A text with all single quotes or all double quotes.
        check_space_before_quote(ps, level);
        const is_dq = v.charAt(0) === "\"";
        print_quotes(ps, 1, !is_dq);
        print_quotes(ps, v.length, is_dq);
        print_quotes(ps, 1, !is_dq);
        return;
    }

    const ws = has_all_whitespace(v);
    if (ws[1])
    {
        // All normal spaces: '     '
        check_space_before_quote(ps, level);
        print_quoted_spaces(ps, v.length);
        return;
    }
    if (ws[0])
    {
        if (ws[2] && !ps.compact && v.length > 1)
        {
            // All newlines, more than one. This is printed as a multiline quote further on.
        }
        else
        {
            print_all_whitespace(ps, v, level);
            return;
        }
    }

    if (is_xmq_text_value(v) && (level === Level.ELEMENT_VALUE || level === Level.ATTR_VALUE))
    {
        // key = 123 or color=blue, no quoting needed.
        print_utf8(ps, v);
        return;
    }

    const start0 = 0;
    let start = start0;
    let stop = v.length;

    const lead = has_leading_space_nl(v);
    if (lead !== null && lead[1] === 0)
    {
        // Leading mix of newlines and whitespace: entities.
        print_all_whitespace(ps, v.substring(0, lead[0]), level);
        start = lead[0];
    }

    const tail = has_ending_nl_space(v, start, stop);
    const old_stop = stop;
    if (tail !== null && tail[1] === 0)
    {
        // Ending mix of newlines and whitespace: entities after the quote.
        stop = tail[0];
        while (stop < old_stop && v.charAt(stop) === " ")
        {
            stop++;
        }
    }

    for (let from = start; from < stop; )
    {
        const to = find_next_char_that_needs_escape(ps, v, from, stop, using_dquotes);
        if (from === to)
        {
            check_space_before_entity_node(ps);
            const c = v.codePointAt(from)!;
            print_char_entity(ps, c);
            let from1 = from + char_count(c);
            // C checks the byte after the escaped char, which is never a newline
            // after a multi byte utf8 char. So only ascii chars (eg CR) escape
            // the following newlines as well.
            while (c < 0x80 && from1 < stop && v.charAt(from1) === "\n")
            {
                // Special case, eg CR LF: escape the newline(s) as well.
                from1++;
                check_space_before_entity_node(ps);
                print_char_entity(ps, 0x0a);
            }
            from = from1;
        }
        else
        {
            const seg = v.substring(from, to);
            const qc = count_necessary_quotes(seg, ps.prefer_double_quotes);
            if (!qc.add_compound && (!qc.add_nls || !ps.compact))
            {
                check_space_before_quote(ps, level);
                print_safe_leaf_quote(ps, seg);
            }
            else
            {
                print_value_string(ps, seg, level, already);
            }
            from = to;
        }
    }

    if (tail !== null && tail[1] === 0)
    {
        // This trailing whitespace could not be printed inside the quote.
        print_all_whitespace(ps, v.substring(stop, old_stop), level);
    }
}

/** Java XMQPrinter.print_value_string: C print_value for a start-stop string value. */
function print_value_string(ps: XMQPrintState, v: string, level: Level, already_compounded: boolean): void
{
    const prefer_d = ps.prefer_double_quotes;
    const ud: UseDQuotes = { v: prefer_d };
    const is_compound = level !== Level.XMQ && quote_needs_compounded(ps, v, prefer_d, ud);

    const old_line_indent = ps.line_indent;

    if (is_compound && !already_compounded)
    {
        level = enter_compound_level(level);
        print_utf8(ps, "(");
        if (!ps.compact)
        {
            print_white_spaces(ps, 1);
        }
        ps.line_indent = ps.current_indent;
    }

    print_value_internal_text(ps, v, level, ud.v, is_compound);

    if (is_compound && !already_compounded)
    {
        if (!ps.compact)
        {
            print_white_spaces(ps, 1);
        }
        print_utf8(ps, ")");
    }

    ps.line_indent = old_line_indent;
}

/** Java XMQPrinter.print_value: C print_value for the parts of a value (Java: a node and its siblings). */
function print_value(ps: XMQPrintState, parts: ValuePart[], level: Level): void
{
    // Check if there are more than one part, if so the value has to be compounded.
    let is_compound = level !== Level.XMQ && parts.length > 1;
    const prefer_d = ps.prefer_double_quotes;
    const ud: UseDQuotes = { v: prefer_d };

    // Check if the single part will split into multiple parts and therefore needs to be compounded.
    if (level !== Level.XMQ && !is_compound && parts.length > 0 && "text" in parts[0])
    {
        is_compound = quote_needs_compounded(ps, parts[0].text, prefer_d, ud);
    }

    const old_line_indent = ps.line_indent;

    if (is_compound)
    {
        level = enter_compound_level(level);
        print_utf8(ps, "(");
        if (!ps.compact)
        {
            print_white_spaces(ps, 1);
        }
        ps.line_indent = ps.current_indent;
    }

    for (const p of parts)
    {
        if ("entity" in p)
        {
            // Like C print_value_internal, an entity in a value is printed as an entity node.
            print_entity_node(ps, p.entity);
        }
        else
        {
            print_value_string(ps, p.text, level, is_compound);
        }
        if (level === Level.XMQ) break;
    }

    if (is_compound)
    {
        if (!ps.compact)
        {
            print_white_spaces(ps, 1);
        }
        print_utf8(ps, ")");
    }

    ps.line_indent = old_line_indent;
}

/** Java XMQPrinter.print_element_name_and_attributes */
function print_element_name_and_attributes(ps: XMQPrintState, node: Element): number
{
    check_space_before_key(ps);

    // The qualified name, prefix:name, also for an undeclared prefix.
    const name = element_name(node);
    const total_u_len = u_len(name);

    print_utf8(ps, name);

    if (attributes(node).length > 0 || has_printable_namespace_defs(node))
    {
        print_utf8(ps, "(");
        print_attributes(ps, node);
        print_utf8(ps, ")");
    }
    return total_u_len;
}

/** The children of a key = value element as value parts. */
function child_value_parts(ps: XMQPrintState, node: Element): ValuePart[]
{
    const parts: ValuePart[] = [];
    for (let c = first_child(ps, node); c !== null; c = next_sibling(ps, c))
    {
        const e = entityName(c);
        if (e !== null) parts.push({ entity: e });
        else parts.push({ text: trimmed_text(ps, c)! });
    }
    return parts;
}

/** Java XMQPrinter.print_key_node */
function print_key_node(ps: XMQPrintState, node: Element, align: number): void
{
    print_element_name_and_attributes(ps, node);

    if (!ps.compact)
    {
        const len = ps.current_indent - ps.line_indent;
        let pad = 1;
        if (len < align) pad = 1 + align - len;
        print_white_spaces(ps, pad);
    }
    print_utf8(ps, "=");
    if (!ps.compact) print_white_spaces(ps, 1);

    print_value(ps, child_value_parts(ps, node), Level.ELEMENT_VALUE);
}

/**
 * Java XMQPrinter.find_element_key_max_width: scan the nodes until there is a node which
 * is not suitable for using the = sign, returns the max width and that node (or null).
 */
function find_element_key_max_width(ps: XMQPrintState, element: Node): [number, Node | null]
{
    let max = 0;
    let i: Node | null = element;
    while (i !== null)
    {
        if (!is_key_value_node(ps, i) || attributes(i as Element).length > 0)
        {
            if (i === element) return [max, next_sibling(ps, i)];
            return [max, i];
        }
        const len = u_len(element_name(i as Element));
        if (len > max) max = len;
        i = next_sibling(ps, i);
    }
    return [max, null];
}

/** An element whose children are printed between braces, saved while they are printed. */
interface Frame
{
    next: Node | null;
    restart_find_at_node: Node | null;
    max: number;
    old_line_indent: number;
}

/**
 * Java XMQPrinter.print_nodes: prints the node and all its following siblings, aligning
 * the equal signs of runs of key=value nodes, unless compact. The elements with children
 * (Java print_element_with_children) are not printed by a recursive call, their state is
 * pushed on a stack while their children are printed.
 */
function print_nodes(ps: XMQPrintState, from: Node | null, only_first = false): void
{
    const stack: Frame[] = [];
    let i = from;
    let restart_find_at_node = from;
    let max = 0;

    for (;;)
    {
        if (i === null)
        {
            const f = stack.pop();
            if (f === undefined) return;
            // The end of print_element_with_children.
            ps.line_indent = f.old_line_indent;
            check_space_before_closing_brace(ps);
            print_utf8(ps, "}");
            i = f.next;
            restart_find_at_node = f.restart_find_at_node;
            max = f.max;
            continue;
        }
        // We need to search ahead to find the max width of the node names so that we can align the equal signs.
        // Only the first node at the top: its siblings are not printed, nor aligned with.
        const last = only_first && stack.length === 0;
        if (!ps.compact && i === restart_find_at_node && !last)
        {
            [max, restart_find_at_node] = find_element_key_max_width(ps, i);
        }
        if (i.nodeType === 1 && first_child(ps, i) !== null && !is_key_value_node(ps, i))
        {
            // Java print_element_with_children.
            const element = i as Element;
            print_element_name_and_attributes(ps, element);

            check_space_before_opening_brace(ps);
            print_utf8(ps, "{");

            stack.push({ next: last ? null : next_sibling(ps, i), restart_find_at_node, max, old_line_indent: ps.line_indent });
            ps.line_indent += ps.indent_amount;

            i = first_child(ps, element);
            restart_find_at_node = i;
            max = 0;
            continue;
        }
        print_node(ps, i, max);
        i = last ? null : next_sibling(ps, i);
    }
}

/** Java XMQPrinter.is_content_node: a text or cdata node, not an entity. */
function is_content_node(node: Node): boolean
{
    return (node.nodeType === 3 || node.nodeType === 4) && entityName(node) === null;
}

/**
 * Java XMQPrinter.is_key_value_node: a single content or entity child, or several text
 * or entity children.
 */
function is_key_value_node(ps: XMQPrintState, node: Node): boolean
{
    if (node.nodeType !== 1) return false;
    const from = first_child(ps, node);
    const to = last_child(ps, node);

    // Single content or entity node.
    if (from !== null && from === to && (from.nodeType === 3 || from.nodeType === 4)) return true;
    if (from === null) return false;

    // Multiple text or entity nodes.
    for (let i: Node | null = from; i !== null; i = next_sibling(ps, i))
    {
        if (i.nodeType !== 3) return false;
    }
    return true;
}

/** Java XMQPrinter.print_node: prints a node that is not an element with children. */
function print_node(ps: XMQPrintState, n: Node, align: number): void
{
    const entity = entityName(n);
    if (entity !== null)
    {
        print_entity_node(ps, entity);
    }
    else if (is_content_node(n))
    {
        print_content_node(ps, n as CharacterData);
    }
    else if (n.nodeType === 8)
    {
        print_comment_node(ps, n as Comment);
    }
    else if (n.nodeType === 7)
    {
        print_pi_node(ps, n as ProcessingInstruction);
    }
    else if (n.nodeType === 10)
    {
        print_doctype(ps, n as DocumentType);
    }
    else if (n.nodeType === 1)
    {
        const e = n as Element;
        if (first_child(ps, e) === null)
        {
            // A node with no children, ie br
            print_element_name_and_attributes(ps, e);
        }
        else
        {
            // A key = value or key = 'value value' node.
            print_key_node(ps, e, align);
        }
    }
}

/**
 * Java XMQPrinter.count_necessary_slashes: if the comment contains asterisk plus slashes,
 * find the max number of slashes after an asterisk. Returns 1 + this max.
 */
function count_necessary_slashes(content: string): number
{
    let max = 0;
    let curr = 0;
    let counting = false;

    for (let i = 0; i < content.length; i++)
    {
        const c = content.charAt(i);
        if (counting)
        {
            if (c === "/")
            {
                curr++;
                if (curr > max) max = curr;
            }
            else
            {
                counting = false;
            }
        }

        if (!counting)
        {
            if (c === "*")
            {
                counting = true;
                curr = 0;
            }
        }
    }
    return max + 1;
}

/** Java XMQPrinter.print_slashes: pre + '/'*n + post, the indentation is not updated. */
function print_slashes(ps: XMQPrintState, pre: string | null, post: string | null, n: number): void
{
    write(ps, pre);
    write(ps, "/".repeat(n));
    write(ps, post);
}

/** Java XMQPrinter.print_comment_lines */
function print_comment_lines(ps: XMQPrintState, content: string, compact: boolean): void
{
    const num_slashes = count_necessary_slashes(content);

    // line_indent = current_indent + 1 + num_slashes (+ 1 for the space, when not compact)
    // so that continuation lines start just after /* .
    let add_spaces = ps.current_indent + 1 + num_slashes;
    print_slashes(ps, null, "*", num_slashes);
    if (!compact)
    {
        if (content.length > 0 && content.charAt(0) !== "\n")
        {
            print_white_spaces(ps, 1);
        }
        add_spaces++;
    }

    const prev_line_indent = ps.line_indent;
    ps.line_indent = add_spaces;

    let line_start = 0;
    for (let i = 0; i < content.length; i++)
    {
        if (content.charAt(i) === "\n")
        {
            if (line_start > 0)
            {
                if (compact)
                {
                    print_slashes(ps, "*", "*", num_slashes);
                }
                else if (i > 0 && content.charAt(i - 1) === "\n" && i + 1 < content.length)
                {
                    // This is an empty line. Do not indent.
                    // Except the last line which must be indented.
                    print_nl(ps, null, null);
                }
                else
                {
                    print_nl_and_indent(ps, null, null);
                }
            }
            print_utf8(ps, content.substring(line_start, i));
            line_start = i + 1;
        }
    }

    if (line_start === 0)
    {
        // No newlines found.
        print_utf8(ps, content);
    }
    else if (line_start < content.length)
    {
        // There is a remaining line that ends with stop and not newline.
        if (line_start > 0)
        {
            if (compact)
            {
                print_slashes(ps, "*", "*", num_slashes);
            }
            else
            {
                print_nl_and_indent(ps, null, null);
            }
        }
        print_utf8(ps, content.substring(line_start));
    }

    if (!compact)
    {
        print_white_spaces(ps, 1);
    }
    print_slashes(ps, "*", null, num_slashes);
    ps.last_char = "/";
    ps.line_indent = prev_line_indent;
}

/** Java XMQPrinter.print_comment_node */
function print_comment_node(ps: XMQPrintState, comment: Comment): void
{
    const content = trimmed_text(ps, comment)!;

    check_space_before_comment(ps);

    const has_newline = content.indexOf("\n") >= 0;
    if (!has_newline)
    {
        if (ps.compact)
        {
            print_utf8(ps, "/*", content, "*/");
            ps.last_char = "/";
        }
        else
        {
            print_utf8(ps, "// ", content);
            ps.last_char = "\u0001";
        }
    }
    else
    {
        print_comment_lines(ps, content, ps.compact);
        ps.last_char = "/";
    }
}

/** Java XMQPrinter.need_separation_before_comment */
function need_separation_before_comment(ps: XMQPrintState): boolean
{
    // If the previous value was quoted, then then no space is needed, ie.
    // 'x y z'/*comment*/
    // If the previous value was an entity &...; then then no space is needed, ie.
    // &nbsp;/*comment*/
    // if previous value was text, then a space is necessary, ie.
    // xyz /*comment*/
    // if previous value was } or )) then no space is is needed.
    // }/*comment*/   ((...))/*comment*/
    const c = ps.last_char;
    return c !== "" && c !== "'" && c !== "\"" && c !== "{" && c !== ")" && c !== "}" && c !== ";";
}

/** Java XMQPrinter.check_space_before_comment */
function check_space_before_comment(ps: XMQPrintState): void
{
    const c = ps.last_char;
    if (c === "") return;
    if (!ps.compact)
    {
        print_nl_and_indent(ps, null, null);
    }
    else if (need_separation_before_comment(ps))
    {
        print_white_spaces(ps, 1);
    }
}

/** Java XMQPrinter.print_pi_node: prints ?target = value */
function print_pi_node(ps: XMQPrintState, node: ProcessingInstruction): void
{
    check_space_before_key(ps);
    print_utf8(ps, "?" + node.target);
    if (!ps.compact) print_white_spaces(ps, 1);
    print_utf8(ps, "=");
    if (!ps.compact) print_white_spaces(ps, 1);

    // Java dumps the pi as xml and takes the text between <?target and ?>, that is the data.
    let content = potentially_add_leading_ending_space(node.data);
    if (ps.compact) content = content.replace(/\n/g, " ");
    print_value_string(ps, content, Level.ELEMENT_VALUE, false);
}

/** A public or system id quoted like libxml2 xmlBufWriteQuotedString. */
function quoted_id(s: string): string
{
    // @xmldom/xmldom keeps the quotes of the ids it parses, eg "-//W3C//DTD HTML 4.01//EN"
    // with the quotes. An id cannot contain the quote that delimits it, so remove them.
    if (s.length >= 2 && (s.charAt(0) === "\"" || s.charAt(0) === "'") && s.charAt(s.length - 1) === s.charAt(0))
    {
        s = s.substring(1, s.length - 1);
    }
    if (s.indexOf("\"") < 0) return "\"" + s + "\"";
    if (s.indexOf("'") < 0) return "'" + s + "'";
    return "\"" + s.replace(/"/g, "&#x22;") + "\"";
}

/**
 * The internal subset of a doctype text, the text between the first [ outside of quotes
 * and the last ], or null.
 */
function internal_subset(text: string): string | null
{
    let q = "";
    for (let i = 0; i < text.length; i++)
    {
        const c = text.charAt(i);
        if (q !== "")
        {
            if (c === q) q = "";
        }
        else if (c === "\"" || c === "'") q = c;
        else if (c === "[")
        {
            const e = text.lastIndexOf("]");
            return e > i ? text.substring(i + 1, e) : null;
        }
    }
    return null;
}

/**
 * The markup declarations, comments, pis and parameter entity references of an internal
 * subset, as they are written, without the whitespace between them.
 */
function subset_parts(subset: string): string[]
{
    const parts: string[] = [];
    let i = 0;
    while (i < subset.length)
    {
        const c = subset.charAt(i);
        if (c === " " || c === "\t" || c === "\n" || c === "\r")
        {
            i++;
            continue;
        }
        let e: number;
        if (subset.startsWith("<!--", i))
        {
            e = subset.indexOf("-->", i + 4);
            e = e < 0 ? subset.length : e + 3;
        }
        else if (c === "<")
        {
            // Up to the > outside of quotes.
            let q = "";
            e = i + 1;
            while (e < subset.length)
            {
                const d = subset.charAt(e++);
                if (q !== "")
                {
                    if (d === q) q = "";
                }
                else if (d === "\"" || d === "'") q = d;
                else if (d === ">") break;
            }
        }
        else
        {
            // A parameter entity reference %name; or stray text, up to the next whitespace or <.
            e = i + 1;
            while (e < subset.length && !/[\s<]/.test(subset.charAt(e))) e++;
        }
        parts.push(subset.substring(i, e));
        i = e;
    }
    return parts;
}

/**
 * The doctype as libxml2 dumps it (xmlDtdDumpOutput), without <!DOCTYPE and >: the name,
 * the public and system ids, and an internal subset as [, a newline, every declaration
 * on its own line and ]. A W3C DOM has no internal subset, it is taken from the doctype
 * text that the xmq parser read (or the internalSubset of a DOM level 2), and its declarations are printed as they were written
 * (libxml2 writes them in its own normalized form).
 */
export function doctype_content(ps: XMQPrintState, node: DocumentType): string
{
    let c = node.name;
    if (node.publicId !== null && node.publicId !== "")
    {
        c += " PUBLIC " + quoted_id(node.publicId);
        if (node.systemId !== null && node.systemId !== "") c += " " + quoted_id(node.systemId);
    }
    else if (node.systemId !== null && node.systemId !== "")
    {
        c += " SYSTEM " + quoted_id(node.systemId);
    }
    // The doctype text of the xmq parser, or the internalSubset of the DOM (DOM level 2,
    // eg @xmldom/xmldom, a browser has none).
    const text = doctypeText(node);
    const dom_subset = (node as { internalSubset?: string | null }).internalSubset;
    const subset = text !== undefined ? internal_subset(text) : (dom_subset ? dom_subset : null);
    if (subset !== null)
    {
        c += " [\n";
        for (let p of subset_parts(subset))
        {
            if (p.startsWith("<!--") && ps.trim)
            {
                // C trims the comments of the internal subset too, they are nodes of the dtd.
                const t = trim_content(p.substring(4, p.endsWith("-->") ? p.length - 3 : p.length), true);
                if (t === null) continue;
                p = "<!--" + t + "-->";
            }
            // libxml2 ends every declaration with a newline, but not a comment or a pi.
            c += p + (p.startsWith("<!--") || p.startsWith("<?") ? "" : "\n");
        }
        c += "]";
    }
    return c;
}

/** Java XMQPrinter.print_doctype: prints !DOCTYPE = value */
function print_doctype(ps: XMQPrintState, node: DocumentType): void
{
    check_space_before_key(ps);
    print_utf8(ps, "!DOCTYPE");
    if (!ps.compact) print_white_spaces(ps, 1);
    print_utf8(ps, "=");
    if (!ps.compact) print_white_spaces(ps, 1);

    let c = doctype_content(ps, node);
    if (ps.compact) c = c.replace(/\n/g, " ");
    print_value_string(ps, c, Level.ELEMENT_VALUE, false);
}

/**
 * Java XMQPrinter.toXmq / print_node of a document: print a document, a fragment or a
 * node as xmq, without a final newline.
 */
export function print_xmq(node: Node, options: PrintOptions): string
{
    const doc = node.nodeType === 9 ? node as Document : node.ownerDocument;
    const ps = new XMQPrintState(options, options.trim ?? !isXmqDocument(doc));
    if (node.nodeType === 9 || node.nodeType === 11)
    {
        print_nodes(ps, first_child(ps, node));
    }
    else if (node.nodeType === 2)
    {
        throw new Error("libxmq: an attribute cannot be printed as xmq, print its element");
    }
    else if (!skipped(ps, node))
    {
        // Only this node, not its following siblings, like Java print_node(ps, node, 0).
        print_nodes(ps, node, true);
    }
    return ps.buffer;
}
