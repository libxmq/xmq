/* libxmq - Copyright (C) 2023-2026 Fredrik Öhrström (spdx: MIT) */

/**
 * Converting a W3C DOM to a javascript value with the json conventions of xmq: the value
 * is the one that JSON.parse would give for the text of xmq to-json, but it is built
 * directly, no json text is printed. The decisions (which node becomes which key and
 * value) are those of the Java JsonPrinter (a port of json.c), with a `// Java name`
 * comment on every function; the printing itself (commas, indentation, escapes) is left
 * out. Also:
 * - Java first rewrites the DOM (fixup_json collapses the text and the entities of an
 *   element into one text). Here the DOM is not changed: children() gives the children
 *   of an element as a list where such text is one collapsed item.
 * - The nesting does not recurse (see doc/DECISIONS.md): an object or array of an
 *   element is a frame on a stack, with the items still to add and the code that ends it.
 * - Like the xmq printer, a DOM that parse did not make is trimmed like C trims the xml
 *   and html it reads.
 * - Where C prints text that is not a json value, the value is: a comment, text or entity
 *   at the top or in an array ("//":"c" without braces in C) is an object of its own
 *   { "//": "c" }, and several values at the top (C prints them separated by commas, eg
 *   a processing instruction before the root element) are an array.
 */

import { attrParts, entityName, isXmqDocument, nsDecls } from "./marks.js";
import { XMQPrintState, attributes, doctype_content, element_name, first_child, next_sibling, ns_defs, ns_prefix, trimmed_text } from "./printer.js";

/** The options of toJson. */
export interface JsonOptions
{
    /** Trim the whitespace like print does (see PrintOptions.trim). */
    trim?: boolean;
    /**
     * Called with the warnings that xmq prints on stderr, eg when an element name is lost
     * in a json array. Default: no warnings.
     */
    onWarning?: (message: string) => void;
}

/** A json value. */
export type JsonValue = string | number | boolean | null | JsonValue[] | { [key: string]: JsonValue };
type JsonObject = { [key: string]: JsonValue };

/** An item of the children of an element: a node, or the collapsed text of the children. */
type Item = Node | { collapsed: string };

/** The state: the trimming of the xmq printer, and the Java fields of the json printer. */
class JsonState extends XMQPrintState
{
    // The leading and ending nodes, taken from the front like stack_rock in C.
    pre: Node[] = [];
    post: Node[] = [];
    pre_post_num_comments_total = 0;
    pre_post_num_comments_used = 0;
    on_warning: ((message: string) => void) | null;

    constructor(o: JsonOptions, trim: boolean)
    {
        super({}, trim);
        this.on_warning = o.onWarning ?? null;
    }
}

/**
 * The items of an object or an array being built (Java print_object_nodes and
 * print_array_nodes), and the code that ends it (the ending comments of the root).
 */
interface Frame
{
    items: Item[];
    i: number;
    // The items are added as values of a json array (print_array_nodes), else as members.
    array: boolean;
    target: JsonObject | JsonValue[];
    container: Node | null;
    counters: Map<string, [number, number]>; // name -> [total, used]
    end: (() => void) | null;
}

function is_node(i: Item): i is Node
{
    return !("collapsed" in i);
}

/** Java JsonPrinter.is_content: a text or cdata node (or a collapsed text), not an entity. */
function is_content(i: Item): boolean
{
    if (!is_node(i)) return true;
    return (i.nodeType === 3 || i.nodeType === 4) && entityName(i) === null;
}

/** Java JsonPrinter.is_entity: the text node of an entity reference. */
function is_entity(i: Item): boolean
{
    return is_node(i) && entityName(i) !== null;
}

/** Java JsonPrinter.element_content: the content of the item itself, null for an element. */
function element_content(ps: XMQPrintState, i: Item | null): string | null
{
    if (i === null || !is_content(i)) return null;
    return is_node(i) ? trimmed_text(ps, i) : i.collapsed;
}

/** Java JsonPrinter.attr: the attribute without namespace with the name, eg A, S or _. */
function attr(i: Item, name: string): Attr | null
{
    if (!is_node(i) || i.nodeType !== 1) return null;
    for (const a of attributes(i as Element)) if (a.name === name) return a;
    return null;
}

/** Java JsonPrinter.has_attr */
function has_attr(i: Item, name: string): boolean
{
    return attr(i, name) !== null;
}

/** Java JsonPrinter.has_attr_other_than_AS_ */
function has_attr_other_than_AS_(i: Item): boolean
{
    if (!is_node(i) || i.nodeType !== 1) return false;
    for (const a of attributes(i as Element))
    {
        if (a.name !== "A" && a.name !== "S" && a.name !== "_") return true;
    }
    return false;
}

/** Java JsonPrinter.has_attributes: attributes, not namespace declarations. */
function has_attributes(i: Item): boolean
{
    return is_node(i) && i.nodeType === 1 && attributes(i as Element).length > 0;
}

/** A part of an attribute value or of the content of an element. */
type Part = { text: string } | { entity: string };

/** The value parts of an attribute, the parts the parser remembered or the value. */
function attr_parts(a: Attr): Part[]
{
    const parts = attrParts(a);
    if (parts === null) return [];
    if (parts !== undefined) return parts;
    // An empty value has no parts in xml (libxml2 stores no text, json null), like an
    // attribute without value. The html parser of C stores an empty text (json "").
    if (a.value === "" && a.ownerDocument?.contentType !== "text/html") return [];
    return [{ text: a.value }];
}

/**
 * Java JsonPrinter.attribute_value: the value of an attribute in json, the same as the
 * content of an element: the text with the character entities replaced, or, when it
 * contains an unknown entity, the text with every entity kept as &name;.
 */
function attribute_value(a: Attr): string
{
    const parts = attr_parts(a);
    const collapsed = collapse_parts(parts);
    if (collapsed !== null) return collapsed;
    let b = "";
    for (const p of parts) b += "entity" in p ? "&" + p.entity + ";" : p.text;
    return b;
}

/**
 * Java JsonPrinter.attr_value: the value of an attribute where a reference to an entity
 * contributes nothing and a character reference its character.
 */
function attr_value(a: Attr): string
{
    let b = "";
    for (const p of attr_parts(a))
    {
        if ("entity" in p)
        {
            if (p.entity.startsWith("#"))
            {
                const c = decode_entity_ref(p.entity);
                if (c > 0 && c <= 0x10ffff) b += String.fromCodePoint(c);
            }
        }
        else b += p.text;
    }
    return b;
}

/** Java JsonPrinter.collapse_text for the parts of a value, or null. */
function collapse_parts(parts: Part[]): string | null
{
    let num_text = 0, num_entities = 0;
    for (const p of parts)
    {
        if ("text" in p)
        {
            num_text++;
            continue;
        }
        // An unknown entity (or an invalid character reference) cannot be replaced by its
        // character, keep the reference.
        const uc = decode_entity_ref(p.entity);
        if (uc <= 0 || uc > 0x10ffff) return null;
        num_entities++;
    }
    if (num_text <= 1 && num_entities === 0) return null;
    let b = "";
    for (const p of parts) b += "text" in p ? p.text : String.fromCodePoint(decode_entity_ref(p.entity));
    return b;
}

/** Java JsonPrinter.decode_entity_ref (XsltSupport.decodeEntityRef): an unknown named entity gives 0. */
function decode_entity_ref(name: string): number
{
    switch (name)
    {
    case "amp": return 0x26;
    case "apos": return 0x27;
    case "gt": return 0x3e;
    case "lt": return 0x3c;
    case "quot": return 0x22;
    case "nbsp": return 160;
    default: break;
    }
    if (!name.startsWith("#")) return 0;
    if (name.length > 1 && name.charAt(1) === "x")
    {
        // C strtol: the leading hex digits, 0 if there are none.
        const m = /^[0-9a-fA-F]*/.exec(name.substring(2))![0];
        return m === "" ? 0 : parseInt(m, 16);
    }
    // C atoi: the leading digits, 0 if there are none.
    const m = /^[+-]?[0-9]*/.exec(name.substring(1))![0];
    const n = parseInt(m, 10);
    return Number.isNaN(n) ? 0 : n;
}

/**
 * The children of a node as items (Java: after fixup_json). The text and entities of an
 * element that collapse_text can replace by one text are one collapsed item. A cdata
 * node counts as text: C reads xml without cdata (the cdata is text).
 */
function children(ps: XMQPrintState, i: Item): Item[]
{
    if (!is_node(i)) return [];
    const list: Item[] = [];
    for (let c = first_child(ps, i); c !== null; c = next_sibling(ps, c)) list.push(c);
    if (i.nodeType !== 1) return list;
    const parts: Part[] = [];
    for (const c of list)
    {
        if (is_entity(c)) parts.push({ entity: entityName(c as Node)! });
        else if (is_content(c)) parts.push({ text: element_content(ps, c)! });
        else return list;
    }
    const collapsed = collapse_parts(parts);
    return collapsed !== null ? [{ collapsed }] : list;
}

/** The name of an element, a pi (its target) or a doctype, else null. */
function node_name(i: Item): string | null
{
    if (!is_node(i)) return null;
    if (i.nodeType === 1) return element_name(i as Element);
    if (i.nodeType === 7) return (i as ProcessingInstruction).target;
    if (i.nodeType === 10) return (i as DocumentType).name;
    return null;
}

/** The local name of an element (Java XmlNode.name), the name without the prefix. */
function local_name(i: Item): string | null
{
    const n = node_name(i);
    if (n === null) return null;
    const e = i as Element;
    if (e.nodeType === 1 && e.prefix !== null && n.startsWith(e.prefix + ":")) return n.substring(e.prefix.length + 1);
    return n;
}

/** The prefix of an element (Java e.ns().prefix()), or null. */
function prefix_of(i: Item): string | null
{
    const e = i as Element;
    return is_node(i) && e.nodeType === 1 && e.prefix !== null && element_name(e).startsWith(e.prefix + ":") ? e.prefix : null;
}

/**
 * Java JsonPrinter.name: the libxml2 node name, elements and entity references have
 * names, text is "text" and comments "comment" (these are counted like names in json.c).
 */
function name(i: Item): string | null
{
    if (!is_node(i)) return "text";
    if (is_entity(i)) return entityName(i);
    switch (i.nodeType)
    {
    case 3:
    case 4:
        return "text";
    case 8:
        return "comment";
    default:
        return local_name(i);
    }
}

/** JsonParser.is_jnumber of the whole string, like json_is_number. */
function is_number(s: string): boolean
{
    return is_jnumber(s, 0, s.length) === s.length;
}

/** JsonParser.is_keyword */
function is_keyword(s: string): boolean
{
    return s === "true" || s === "false" || s === "null";
}

/** JsonParser.has_number_ended */
function has_number_ended(c: string): boolean
{
    return c === " " || c === "\n" || c === "," || c === "}" || c === "]";
}

/** JsonParser.is_jnumber (is_jnumber in json.c): the position after the number, or -1. */
function is_jnumber(s: string, start: number, stop: number): number
{
    if (start === stop) return -1;
    let found_e = false, found_e_sign = false, leading_zero = false, last_is_digit = false, found_dot = false;
    let i;
    for (i = start; i < stop; ++i)
    {
        const c = s.charAt(i);
        last_is_digit = false;
        const current_is_not_digit = (c < "0" || c > "9");
        if (i === start)
        {
            if (current_is_not_digit && c !== "-") return -1;
            if (c === "0") leading_zero = true;
            if (c !== "-") last_is_digit = true;
            continue;
        }
        if (leading_zero)
        {
            leading_zero = false;
            if (has_number_ended(c)) return i;
            if (c !== ".") return -1;
            found_dot = true;
        }
        else if (c === ".")
        {
            if (found_dot) return -1;
            found_dot = true;
        }
        else if (c === "e" || c === "E")
        {
            if (found_e) return -1;
            found_e = true;
        }
        else if (found_e && !found_e_sign)
        {
            if (has_number_ended(c)) return i;
            if (current_is_not_digit && c !== "-" && c !== "+") return -1;
            if (c === "+" || c === "-") found_e_sign = true;
            else last_is_digit = true;
        }
        else
        {
            found_e_sign = false;
            if (has_number_ended(c)) return i;
            if (current_is_not_digit) return -1;
            last_is_digit = true;
        }
    }
    if (!last_is_digit) return -1;
    return i;
}

/** Java JsonPrinter.is_key_value_node, on the children items. */
function is_key_value_node(kids: Item[]): boolean
{
    if (kids.length === 1 && (is_content(kids[0]) || is_entity(kids[0]))) return true;
    if (kids.length === 0) return false;
    for (const i of kids)
    {
        if (!is_content(i) && !is_entity(i)) return false;
    }
    return true;
}

/** A number as javascript reads it, C accepts numbers that json does not, eg 1.e5 or -01. */
function number_value(content: string): JsonValue
{
    if (content === "true") return true;
    if (content === "false") return false;
    if (content === "null") return null;
    const n = Number(content);
    return Number.isNaN(n) ? content : n;
}

/** Java JsonPrinter.print_value: the value of the items of a key = value element. */
function value_of_items(ps: JsonState, items: Item[], force_string: boolean): JsonValue
{
    // An element without content, eg _(S) in an array, is the empty string.
    if (items.length === 0) return "";
    const content = element_content(ps, items[0]);
    if (items.length === 1 && !force_string && content !== null && (is_number(content) || is_keyword(content)))
    {
        // A number or a keyword, 123 true false null
        return number_value(content);
    }
    // The text and the entities (kept as &name; when the content could not be collapsed,
    // ie it contains an unknown entity).
    let v = "";
    for (const node of items)
    {
        if (is_entity(node)) v += "&" + entityName(node as Node)! + ";";
        else v += element_content(ps, node) ?? "";
    }
    return v;
}

/** Java JsonPrinter.print_element_name: the key of an element, prefix:name or the _ attribute, [used] if repeated. */
function element_key(node: Item, total: number, used: number): string
{
    const n = local_name(node) ?? "";
    const prefix = prefix_of(node);
    let k = prefix !== null ? prefix + ":" : "";
    if (n !== "_")
    {
        k += n;
    }
    else
    {
        // The key was stored in the attribute _ since it is not a valid element name.
        const a = attr(node, "_");
        if (a !== null) k += attr_value(a);
    }
    if (total > 1) k += "[" + used + "]";
    return k;
}

/** Java JsonPrinter.print_attributes: the attributes as _name members, the namespace declarations as _xmlns. */
function add_attributes(node: Item, o: JsonObject): void
{
    if (!is_node(node) || node.nodeType !== 1) return;
    for (const a of attributes(node as Element))
    {
        // The _ attribute holds the element name when it is not a valid xml name.
        if (a.name === "_") continue;
        o["_" + a.name] = attr_parts(a).length > 0 ? attribute_value(a) : null;
    }
    // The declarations that the parser remembered, with the undeclared prefixes (null).
    const decls = nsDecls(node as Element);
    if (decls !== undefined)
    {
        for (const d of decls) o["_xmlns" + (d.prefix !== null ? ":" + d.prefix : "")] = d.href;
        return;
    }
    for (const ns of ns_defs(node as Element))
    {
        const prefix = ns_prefix(ns);
        o["_xmlns" + (prefix !== null ? ":" + prefix : "")] = ns.value;
    }
}

/** Java JsonPrinter.print_comment_node: the key of a comment. */
function comment_key(prefix_ul: boolean, total: number, used: number): string
{
    return (prefix_ul ? "_//" : "//") + (total > 1 ? "[" + used + "]" : "");
}

/** Java JsonPrinter.print_leaf_node: the value of an element without children. */
function leaf_value(node: Item): JsonValue
{
    if (has_attr(node, "A")) return [];
    // An empty element forced to be a string: key(S) ---> "key":""
    if (has_attr(node, "S") && !has_attr_other_than_AS_(node)) return "";
    const o: JsonObject = {};
    if (has_attributes(node)) add_attributes(node, o);
    return o;
}

function warn(ps: JsonState, m: string): void
{
    if (ps.on_warning !== null) ps.on_warning(m);
}

/** Add the value to the object (with the key) or to the array. */
function add(target: JsonObject | JsonValue[], key: string, value: JsonValue): void
{
    if (Array.isArray(target)) target.push(value);
    else target[key] = value;
}

/**
 * Java JsonPrinter.print_node: add the node to the object being built (container is the
 * element of the object) or, with container null, as a value to the array (at the top or
 * in a json array). Returns the frame of a new object or array, whose items are added later.
 */
function add_node(ps: JsonState, f: Frame, container: Node | null, node: Item, total: number, used: number): Frame | null
{
    // A comment, text or entity is a member, at the top or in an array an object of its own.
    let pair: [string, JsonValue] | null = null;
    if (is_node(node) && node.nodeType === 8) pair = [comment_key(false, total, used), trimmed_text(ps, node) ?? ""];
    else if (is_content(node)) pair = ["|" + (total > 1 ? "[" + used + "]" : ""), element_content(ps, node)!];
    else if (is_entity(node)) pair = ["&", "&" + entityName(node as Node)! + ";"];
    if (pair !== null)
    {
        if (container !== null) add(f.target, pair[0], pair[1]);
        else add(f.target, "", { [pair[0]]: pair[1] });
        return null;
    }
    const kids = children(ps, node);
    if (kids.length === 0 && container !== null)
    {
        add(f.target, element_key(node, total, used), leaf_value(node));
        return null;
    }
    const key = container !== null ? element_key(node, total, used) : "";
    if (is_key_value_node(kids) && (!has_attributes(node) || !has_attr_other_than_AS_(node)))
    {
        // Java print_key_node.
        add(f.target, key, value_of_items(ps, kids, has_attr(node, "S")));
        return null;
    }
    if (has_attr(node, "A"))
    {
        const n = local_name(node) ?? "";
        if (n !== "_" && container === null)
        {
            warn(ps, "xmq: Warning! The element name \"" + n + "\" is lost when converted to an unnamed json array!\n");
        }
        if (has_attr_other_than_AS_(node))
        {
            warn(ps, "xmq: Warning! The element \"" + n + "\" loses its attributes when converted to a json array!\n");
        }
        // Java print_array_with_children.
        const a: JsonValue[] = [];
        add(f.target, key, a);
        return { items: kids, i: 0, array: true, target: a, container: null, counters: new Map(), end: null };
    }
    // Java print_element_with_children.
    const o: JsonObject = {};
    add(f.target, key, o);
    while (container === null && ps.pre.length > 0)
    {
        const n = ps.pre.shift()!;
        if (n.nodeType === 10) o["!DOCTYPE"] = doctype_content(ps, n as DocumentType);
        else o[comment_key(true, ps.pre_post_num_comments_total, ps.pre_post_num_comments_used++)] = trimmed_text(ps, n) ?? "";
    }
    const n = local_name(node);
    if (container === null && n !== null && n !== "_")
    {
        // The name of a top level object, or an object in an array, is stored inside the
        // object with the key _, x{a=1} -> {"_":"x","a":1}
        o["_"] = element_key(node, total, used);
    }
    add_attributes(node, o);
    return object_frame(node as Node, kids, o, () =>
    {
        while (container === null && ps.post.length > 0)
        {
            const c = ps.post.shift()!;
            o[comment_key(true, ps.pre_post_num_comments_total, ps.pre_post_num_comments_used++)] = trimmed_text(ps, c) ?? "";
        }
    });
}

/** Java JsonPrinter.print_object_nodes: a frame that adds the items as members of the object. */
function object_frame(container: Node | null, items: Item[], target: JsonObject | JsonValue[], end: (() => void) | null): Frame
{
    const counters = new Map<string, [number, number]>();
    for (const i of items)
    {
        const n = name(i);
        if (n !== null && n !== "_")
        {
            const c = counters.get(n);
            if (c === undefined) counters.set(n, [1, 0]);
            else c[0]++;
        }
    }
    return { items, i: 0, array: false, target, container, counters, end };
}

/** One step of Java JsonPrinter.print_object_nodes. */
function object_step(ps: JsonState, f: Frame, i: Item): Frame | null
{
    const n = name(i);
    if (n === null || n === "_") return add_node(ps, f, f.container, i, 1, 0);
    const c = f.counters.get(n)!;
    const frame = add_node(ps, f, f.container, i, c[0], c[1]);
    c[1]++;
    return frame;
}

/** One step of Java JsonPrinter.print_array_nodes. */
function array_step(ps: JsonState, f: Frame, i: Item): Frame | null
{
    const force_string = has_attr(i, "S");
    const c = element_content(ps, i);
    if (force_string || (c !== null && (is_number(c) || is_keyword(c))))
    {
        add(f.target, "", value_of_items(ps, children(ps, i), force_string));
        return null;
    }
    return add_node(ps, f, null, i, 1, 0);
}

/**
 * Java JsonPrinter.toJson: the value of a document, the leading doctype and comments and
 * the ending comments are members of the root object. A node that is not a document or
 * fragment is converted as if it were the only node of a document.
 */
export function json_value(node: Node, options: JsonOptions): JsonValue | undefined
{
    const doc = node.nodeType === 9 ? node as Document : node.ownerDocument;
    const ps = new JsonState(options, options.trim ?? !isXmqDocument(doc));
    if (node.nodeType === 2) throw new Error("libxmq: an attribute cannot be converted to json, convert its element");
    const top = node.nodeType === 9 || node.nodeType === 11;
    const nodes: Node[] = [];
    if (top)
    {
        for (let c = first_child(ps, node); c !== null; c = next_sibling(ps, c)) nodes.push(c);
    }
    else nodes.push(node);
    if (nodes.length === 0) return undefined;
    let first = 0;
    let last = nodes.length - 1;
    if (top)
    {
        let k;
        for (k = first; k < last; k++)
        {
            const n = nodes[k];
            if (n.nodeType !== 10 && n.nodeType !== 8) break;
            ps.pre.push(n);
            if (n.nodeType === 8) ps.pre_post_num_comments_total++;
        }
        first = k;
        for (k = last; k > first; k--)
        {
            const n = nodes[k];
            if (n.nodeType !== 8) break;
            ps.post.push(n);
            ps.pre_post_num_comments_total++;
        }
        last = k;
    }

    // The values at the top, print_object_nodes(ps, null, first, last), without recursion.
    const values: JsonValue[] = [];
    const stack: Frame[] = [object_frame(null, nodes.slice(first, last + 1), values, null)];
    while (stack.length > 0)
    {
        const f = stack[stack.length - 1];
        if (f.i >= f.items.length)
        {
            stack.pop();
            if (f.end !== null) f.end();
            continue;
        }
        const item = f.items[f.i++];
        const next = f.array ? array_step(ps, f, item) : object_step(ps, f, item);
        if (next !== null) stack.push(next);
    }
    return values.length === 1 ? values[0] : values;
}
