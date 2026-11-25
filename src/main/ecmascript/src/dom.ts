/* libxmq - Copyright (C) 2023-2026 Fredrik Öhrström (spdx: MIT) */

/**
 * Parsing xmq into a W3C DOM. XMQParseIntoDOM is a port of the Java XMQParseIntoDOM (a
 * port of the parse callbacks of the C xmq.c), it builds the tree of tree.ts, which is
 * then turned into a W3C DOM document. The entity references, the attribute
 * values with entities or without a value, and the doctype text are remembered in
 * marks.ts, since a W3C DOM cannot store them.
 */

import { ParseErrorCode } from "./errors.js";
import { markAttrParts, markDoctype, markEntity, type AttrPart } from "./marks.js";
import { XMQParser } from "./parser.js";
import { TAttr, TDocument, TElement, TNode, TNs, XML_NS } from "./tree.js";
import { unQuote } from "./trim.js";

/** The namespace of the xmlns attributes. */
const XMLNS_NS = "http://www.w3.org/2000/xmlns/";

/** An embedded xml fragment parsed by a DOMParser, imported as it is. */
class TForeign extends TNode
{
    constructor(public dom: Node)
    {
        super("foreign");
    }
}

/** Something that can parse xml text, a DOMParser. */
export interface XmlTextParser
{
    parseFromString(text: string, type: "text/xml"): Document;
}

/** Java XMQParseIntoDOM.potentially_add_leading_ending_space */
function potentially_add_leading_ending_space(s: string): string
{
    if (s.length === 0) return s;
    const prefix = s.charAt(0) === "'";
    const postfix = s.charAt(s.length - 1) === "'";
    if (!prefix && !postfix) return s;
    return (prefix ? " " : "") + s + (postfix ? " " : "");
}

/** Java XMQParseIntoDOM.find_ns: a declaration on node or above, also one without uri yet. */
function find_ns(node: TNode | null, prefix: string | null): TNs | null
{
    for (let cur = node; cur !== null; cur = cur.parent)
    {
        if (cur instanceof TElement)
        {
            for (const ns of cur.nsDefs)
            {
                if (ns.prefix === prefix) return ns;
            }
        }
    }
    return null;
}

/** The parser that builds the tree (Java XMQParseIntoDOM). */
export class XMQParseIntoDOM extends XMQParser
{
    doc_ = new TDocument();
    private element_stack_: (TNode | null)[] = []; // The top is the current parent.
    private element_last_: TNode | null = null; // The last created node.
    private parsing_doctype_ = false;
    private parsing_pi_ = false;
    private pi_name_: string | null = null;
    private element_namespace_: string | null = null;
    private attribute_namespace_: string | null = null;
    private declaring_xmlns_ = false;
    private declaring_xmlns_namespace_: TNs | null = null;
    private default_namespace_: TNs | null = null;
    private implicit_root_: string | null; // Assume that this is the first element name.

    /**
     * @param xml_parser Parses the embedded xml fragments <...>, without one they are an error.
     * @param implicit_root The implicit root element, or null (the C --root option).
     */
    constructor(private xml_parser: XmlTextParser | null, implicit_root: string | null = null)
    {
        super();
        this.implicit_root_ = implicit_root === null || implicit_root === "" ? null : implicit_root;
    }

    protected setup(): void
    {
        this.doc_ = new TDocument();
        this.element_stack_ = [this.doc_];
        this.element_last_ = null;
    }

    /**
     * XMQParseIntoDOM.top: the node that gets the new children, the top of the stack, or
     * the document when the top is not a node, eg in the body of a pi ?a { b }.
     */
    private top(): TNode
    {
        const n = this.element_stack_[this.element_stack_.length - 1];
        return n !== null && n !== undefined ? n : this.doc_;
    }

    private push(n: TNode | null): void
    {
        this.element_stack_.push(n);
    }

    private pop(): TNode | null
    {
        return this.element_stack_.pop() ?? null;
    }

    /** XMQParseIntoDOM.un_quote: xmq_un_quote(start, stop, true, true) of a quote token. */
    private un_quote(start: number, stop: number): string
    {
        return unQuote(this.buffer_, start, stop, true, true);
    }

    /** XMQParseIntoDOM.add_text: add a text node, merged with a previous text node. */
    private add_text(parent: TNode | null, content: string): TNode | null
    {
        if (parent === null) return null;
        return parent.addChild(new TNode("text", "", content));
    }

    /** XMQParseIntoDOM.create_node */
    private create_node(start: number, stop: number): void
    {
        const name = this.buffer_.substring(start, stop);

        if (name === "!DOCTYPE")
        {
            this.parsing_doctype_ = true;
            return;
        }
        if (name.length > 0 && name.charAt(0) === "?")
        {
            this.parsing_pi_ = true;
            this.pi_name_ = name.substring(1);
            return;
        }

        const new_node = new TElement(name);
        if (this.element_last_ === null)
        {
            if (this.implicit_root_ === null || name === this.implicit_root_)
            {
                // There is no implicit root, or name is the same as the implicit root.
                this.element_last_ = new_node;
                this.doc_.setRootElement(new_node);
            }
            else
            {
                // We have an implicit root and it is different from name.
                const root = new TElement(this.implicit_root_);
                this.element_last_ = root;
                this.doc_.setRootElement(root);
                this.push(root);
            }
        }
        const parent = this.top();
        if (new_node.parent === null) parent.addChild(new_node);

        if (this.element_namespace_ !== null)
        {
            // Have a namespace before the element name, eg abc:work
            let ns = new_node.searchNs(this.element_namespace_);
            if (ns === null)
            {
                // The namespace does not yet exist. Lets hope it will be declared
                // inside the attributes of this node. Use a temporary href for now.
                ns = new_node.declareNs(this.element_namespace_, null);
            }
            new_node.ns = ns;
            this.element_namespace_ = null;
        }
        else if (this.default_namespace_ !== null)
        {
            new_node.ns = this.default_namespace_;
        }
        this.element_last_ = new_node;
    }

    protected do_whitespace(): void
    {
    }

    /** XMQParseIntoDOM.do_embedded_xml: parse the fragment and add its element. */
    protected do_embedded_xml(start: number, stop: number): void
    {
        let node: Element | null = null;
        if (this.xml_parser !== null)
        {
            try
            {
                const fdoc = this.xml_parser.parseFromString(this.buffer_.substring(start, stop), "text/xml");
                node = fdoc.documentElement;
                // A browser reports the error in the document.
                if (node !== null && node.getElementsByTagName("parsererror").length > 0) node = null;
            }
            catch
            {
                node = null;
            }
        }
        if (node === null)
        {
            throw this.error_here(ParseErrorCode.XMQ_ERROR_PARSING_XML);
        }
        // Like the Java XmlParser.NOCDATA | NOBLANKS: cdata is text, blank text is dropped.
        remove_blanks_and_cdata(node);
        const f = new TForeign(node);
        if (this.element_last_ === null)
        {
            if (this.implicit_root_ !== null && node.nodeName !== this.implicit_root_)
            {
                const root = new TElement(this.implicit_root_);
                this.element_last_ = root;
                this.doc_.setRootElement(root);
                this.push(root);
            }
            else
            {
                this.element_last_ = f;
            }
        }
        this.top().addChild(f);
        this.element_last_ = f;
    }

    protected do_quote(start_line: number, start_col: number, start: number, stop: number): void
    {
        this.element_last_ = this.add_text(this.top(), this.un_quote(start, stop));
    }

    protected do_element_value_quote(start_line: number, start_col: number, start: number, stop: number): void
    {
        const trimmed = this.un_quote(start, stop);
        if (this.parsing_pi_)
        {
            this.top().addChild(new TNode("pi", this.pi_name_ ?? "", potentially_add_leading_ending_space(trimmed)));
            this.parsing_pi_ = false;
            this.pi_name_ = null;
        }
        else if (this.parsing_doctype_)
        {
            // The C and Java xmq check the doctype with the xml parser (doctype could not be
            // parsed), the javascript version keeps the text as it is.
            this.top().appendRaw(new TNode("dtd", "", trimmed));
            this.parsing_doctype_ = false;
        }
        else
        {
            this.add_text(this.element_last_, trimmed);
        }
    }

    protected do_element_value_compound_quote(start_line: number, start_col: number, start: number, stop: number): void
    {
        this.do_quote(start_line, start_col, start, stop);
    }

    protected do_attr_value_quote(start_line: number, start_col: number, start: number, stop: number): void
    {
        const trimmed = this.un_quote(start, stop);
        if (this.declaring_xmlns_)
        {
            if (this.declaring_xmlns_namespace_ !== null)
            {
                this.update_namespace_href(this.declaring_xmlns_namespace_, trimmed);
            }
            this.declaring_xmlns_ = false;
            this.declaring_xmlns_namespace_ = null;
            return;
        }
        this.add_text(this.element_last_, trimmed);
    }

    protected do_attr_value_compound_quote(start_line: number, start_col: number, start: number, stop: number): void
    {
        this.do_quote(start_line, start_col, start, stop);
    }

    /** XMQParseIntoDOM.create_entity: a character reference is text, a named one an entity node. */
    private create_entity(start: number, stop: number, parent: TNode | null): TNode | null
    {
        // There is no node for the value, eg the value of a processing instruction
        // attribute ?a(b=&x;), the value is dropped.
        if (parent === null) return null;
        const tmp = this.buffer_.substring(start, stop);
        if (tmp.length > 1 && tmp.charAt(1) === "#")
        {
            // Character entity, parsed like the Java Long.parseLong.
            let uc = -1;
            const hex = tmp.charAt(2) === "x";
            const digits = tmp.substring(hex ? 3 : 2, tmp.length - 1);
            if (hex ? /^[+-]?[0-9a-fA-F]+$/.test(digits) : /^[+-]?[0-9]+$/.test(digits))
            {
                uc = parseInt(digits, hex ? 16 : 10);
            }
            if (!(uc >= 0 && uc <= 0x10ffff))
            {
                throw this.error_here(ParseErrorCode.XMQ_ERROR_INVALID_CHAR);
            }
            return parent.addChild(new TNode("text", "", String.fromCodePoint(uc)));
        }
        // Named references are kept as is. Like xmlNewReference, drop the & and a
        // trailing ; if present (&ff needs no ; and a lone & gives an empty name).
        let name = tmp.substring(1);
        if (name.endsWith(";")) name = name.substring(0, name.length - 1);
        return parent.appendRaw(new TNode("entity", name));
    }

    protected do_entity(start_line: number, start_col: number, start: number, stop: number): void
    {
        this.element_last_ = this.create_entity(start, stop, this.top());
    }

    protected do_element_value_entity(start_line: number, start_col: number, start: number, stop: number): void
    {
        // The content of a processing instruction ?a=&amp; is the text of the entity.
        if (this.parsing_pi_)
        {
            this.do_element_value_text(start_line, start_col, start, stop);
            return;
        }
        this.create_entity(start, stop, this.element_last_);
    }

    protected do_element_value_compound_entity(start_line: number, start_col: number, start: number, stop: number): void
    {
        this.do_entity(start_line, start_col, start, stop);
    }

    protected do_attr_value_entity(start_line: number, start_col: number, start: number, stop: number): void
    {
        this.create_entity(start, stop, this.element_last_);
    }

    protected do_attr_value_compound_entity(start_line: number, start_col: number, start: number, stop: number): void
    {
        this.do_entity(start_line, start_col, start, stop);
    }

    protected do_comment(start_line: number, start_col: number, start: number, stop: number): void
    {
        const c = new TNode("comment", "", this.xmq_un_comment(start, stop));
        this.top().appendRaw(c);
        this.element_last_ = c;
    }

    protected do_comment_continuation(start_line: number, start_col: number, start: number, stop: number): void
    {
        // We have ///* alfa *///* beta *///* gamma *///
        // and this function is invoked with "* beta *///"
        let k = stop - 1;
        let n = 0;
        while (k > start && this.buffer_.charAt(k) === "/")
        {
            n++;
            k--;
        }
        const trimmed = this.xmq_un_comment(start - n, stop);
        const last = this.element_last_;
        if (last !== null)
        {
            last.content = last.content + "\n" + trimmed;
        }
    }

    protected do_element_key(start_line: number, start_col: number, start: number, stop: number): void
    {
        this.create_node(start, stop);
    }

    protected do_element_name(start_line: number, start_col: number, start: number, stop: number): void
    {
        this.create_node(start, stop);
    }

    protected do_element_ns(start_line: number, start_col: number, start: number, stop: number): void
    {
        this.element_namespace_ = this.buffer_.substring(start, stop);
    }

    protected do_apar_left(): void
    {
        this.push(this.element_last_);
    }

    /**
     * XMQParseIntoDOM.end_valueless_ns_declaration (C end_valueless_ns_declaration): a
     * namespace declaration without a value, eg xmlns:abc without = uri, declares nothing.
     */
    private end_valueless_ns_declaration(): void
    {
        if (this.declaring_xmlns_)
        {
            this.declaring_xmlns_ = false;
            this.declaring_xmlns_namespace_ = null;
        }
    }

    protected do_apar_right(): void
    {
        this.end_valueless_ns_declaration();
        this.element_last_ = this.pop();
    }

    protected do_cpar_left(): void
    {
        this.push(this.element_last_);
    }

    protected do_cpar_right(): void
    {
        this.element_last_ = this.pop();
    }

    protected do_brace_left(): void
    {
        this.push(this.element_last_);
        // The document is at the bottom of the stack, then one entry per nesting level.
        if (this.element_stack_.length > XMQParser.MAX_NESTING_DEPTH + 1)
        {
            throw this.error_here(ParseErrorCode.XMQ_ERROR_TOO_DEEP);
        }
    }

    protected do_brace_right(): void
    {
        this.element_last_ = this.pop();
    }

    protected do_equals(): void
    {
    }

    protected do_equals_done(): void
    {
    }

    /** XMQParseIntoDOM.update_namespace_href: xmlns='' removes the default namespace. */
    private update_namespace_href(ns: TNs, href: string): void
    {
        ns.href = href;
        if (href.length === 0 && ns === this.default_namespace_)
        {
            const element = this.top();
            if (element instanceof TElement && element.ns === ns)
            {
                element.ns = null;
            }
            this.default_namespace_ = null;
        }
    }

    protected do_attr_value_text(start_line: number, start_col: number, start: number, stop: number): void
    {
        if (this.declaring_xmlns_)
        {
            if (this.declaring_xmlns_namespace_ !== null)
            {
                this.update_namespace_href(this.declaring_xmlns_namespace_, this.buffer_.substring(start, stop));
            }
            this.declaring_xmlns_ = false;
            this.declaring_xmlns_namespace_ = null;
            return;
        }
        this.add_text(this.element_last_, this.buffer_.substring(start, stop));
    }

    protected do_element_value_text(start_line: number, start_col: number, start: number, stop: number): void
    {
        const text = this.buffer_.substring(start, stop);
        if (this.parsing_pi_)
        {
            this.top().addChild(new TNode("pi", this.pi_name_ ?? "", potentially_add_leading_ending_space(text)));
            this.parsing_pi_ = false;
            this.pi_name_ = null;
        }
        else if (this.parsing_doctype_)
        {
            this.top().appendRaw(new TNode("dtd", "", text));
            this.parsing_doctype_ = false;
        }
        else
        {
            this.add_text(this.element_last_, text);
        }
    }

    protected do_ns_declaration(start_line: number, start_col: number, start: number, stop: number, stop_suffix: number): void
    {
        this.end_valueless_ns_declaration();
        // A namespace declaration attribute, either a default declaration xmlns=... or
        // xmlns:prefix=... The difference is that stop_suffix is stop (xmlns=) or after
        // stop (xmlns:foo=).
        const element = this.top();
        if (!(element instanceof TElement))
        {
            // There is no element node, eg a processing instruction element.
            this.declaring_xmlns_ = true;
            this.declaring_xmlns_namespace_ = null;
            return;
        }
        let ns: TNs | null;
        if (stop === stop_suffix)
        {
            // A default namespace, eg: xmlns=uri
            ns = null;
            for (const d of element.nsDefs)
            {
                if (d.prefix === null)
                {
                    ns = d;
                    break;
                }
            }
            if (ns === null) ns = element.declareNs(null, null);
            // An element without prefix has the inherited default namespace (or none),
            // the default namespace declared on the element itself replaces it.
            if (element.ns === null || element.ns.prefix === null)
            {
                element.ns = ns;
            }
            this.default_namespace_ = ns;
        }
        else
        {
            // A namespace with a prefix: xmlns:prefix=uri
            const name = this.buffer_.substring(stop + 1, stop_suffix);
            ns = find_ns(element, name);
            if (ns === null && name === "xml") ns = XML_NS;
            if (ns === null) ns = element.declareNs(name, null);
        }
        this.declaring_xmlns_ = true;
        this.declaring_xmlns_namespace_ = ns;
    }

    protected do_attr_key(start_line: number, start_col: number, start: number, stop: number): void
    {
        this.end_valueless_ns_declaration();
        const key = this.buffer_.substring(start, stop);
        const element = this.top();
        if (!(element instanceof TElement))
        {
            // There is no element node, eg a processing instruction with attributes.
            // The attribute is created but not attached.
            this.element_last_ = new TAttr(key);
            this.attribute_namespace_ = null;
            return;
        }
        const attr = new TAttr(key);
        if (this.attribute_namespace_ !== null)
        {
            let ns = find_ns(element, this.attribute_namespace_);
            if (ns === null && this.attribute_namespace_ === "xml") ns = XML_NS;
            if (ns === null)
            {
                // The namespace does not yet exist. Lets hope it will be declared
                // inside the attributes of this node. Use a temporary href for now.
                ns = element.declareNs(this.attribute_namespace_, null);
            }
            attr.ns = ns;
            this.attribute_namespace_ = null;
        }
        element.addAttribute(attr);
        this.element_last_ = attr;
    }

    protected do_ns_colon(): void
    {
    }

    protected do_attr_ns(start_line: number, start_col: number, start: number, stop: number): void
    {
        if (!this.declaring_xmlns_)
        {
            // Normal attribute namespace found before the attribute key, eg x:alfa=123
            this.attribute_namespace_ = this.buffer_.substring(start, stop);
        }
    }
}

/** Remove blank text nodes and turn cdata into text, like XmlParser.NOCDATA | NOBLANKS. */
function remove_blanks_and_cdata(node: Node): void
{
    let c = node.firstChild;
    while (c !== null)
    {
        const next = c.nextSibling;
        if (c.nodeType === 4) // CDATA_SECTION_NODE
        {
            node.replaceChild(node.ownerDocument!.createTextNode((c as CharacterData).data), c);
        }
        else if (c.nodeType === 3 && /^[ \t\r\n]*$/.test((c as Text).data))
        {
            node.removeChild(c);
        }
        else if (c.nodeType === 1)
        {
            remove_blanks_and_cdata(c);
        }
        c = next;
    }
}

/**
 * The xmq content cannot be stored in a W3C DOM: a Document has at most one element and
 * no text, and the names must be valid xml names.
 */
export class XMQDomError extends Error
{
    constructor(message: string)
    {
        super(message);
        this.name = "XMQDomError";
    }
}

/** The name, public id and system id of a doctype text, eg html PUBLIC "a" "b". */
function doctype_parts(text: string): [string, string, string]
{
    const m = /^\s*([^\s\[]+)\s*(?:(PUBLIC)\s+("[^"]*"|'[^']*')\s*("[^"]*"|'[^']*')?|(SYSTEM)\s+("[^"]*"|'[^']*'))?/.exec(text);
    if (m === null) return [text.trim(), "", ""];
    const unq = (s: string | undefined) => s === undefined ? "" : s.substring(1, s.length - 1);
    if (m[2] === "PUBLIC") return [m[1], unq(m[3]), unq(m[4])];
    if (m[5] === "SYSTEM") return [m[1], "", unq(m[6])];
    return [m[1], "", ""];
}

/** The qualified name, prefix:name with a prefix. */
function qname(name: string, ns: TNs | null): string
{
    return ns !== null && ns.prefix !== null ? ns.prefix + ":" + name : name;
}

/**
 * Convert a node of the tree and its descendants into W3C nodes of doc. Without recursion,
 * the stack of node and the browsers is too small for 10000 nested elements: every node is
 * appended to its parent when it is created and its children are converted from a stack.
 */
function to_w3c(root: TNode, doc: Document): Node
{
    const top = to_w3c_node(root, doc);
    const stack: [TNode, Node][] = [[root, top]];
    while (stack.length > 0)
    {
        const [t, w] = stack.pop()!;
        if (t.kind !== "element") continue;
        for (const c of t.children)
        {
            const cw = to_w3c_node(c, doc);
            w.appendChild(cw);
            if (c.kind === "element") stack.push([c, cw]);
        }
    }
    return top;
}

/** Convert one node of the tree into a W3C node of doc, an element without its children. */
function to_w3c_node(n: TNode, doc: Document): Node
{
    switch (n.kind)
    {
    case "element":
    {
        const t = n as TElement;
        // A prefix that is never declared (libxml2 keeps the namespace without uri)
        // cannot be a W3C namespace, the element gets the name prefix:name without one.
        const q = qname(t.name, t.ns);
        const href = t.ns !== null && t.ns.href !== null && t.ns.href !== "" ? t.ns.href : null;
        const e = href === null && t.ns !== null && t.ns.prefix !== null ? doc.createElement(q) : doc.createElementNS(href, q);
        // The namespace declarations first, like xml serializes them.
        for (const d of t.nsDefs)
        {
            if (d.href === null) continue;
            e.setAttributeNS(XMLNS_NS, d.prefix === null ? "xmlns" : "xmlns:" + d.prefix, d.href);
        }
        for (const a of t.attributes)
        {
            set_attribute(e, a);
        }
        return e;
    }
    case "text":
        return doc.createTextNode(n.content);
    case "entity":
    {
        const t = doc.createTextNode("&" + n.name + ";");
        markEntity(t);
        return t;
    }
    case "comment":
        return doc.createComment(n.content);
    case "pi":
        return doc.createProcessingInstruction(n.name, n.content);
    case "foreign":
        return doc.importNode((n as TForeign).dom, true);
    default:
        throw new XMQDomError("xmq: a " + n.kind + " cannot be stored here in a W3C DOM");
    }
}

/** Set the attribute, remembering a value with entities or no value at all. */
function set_attribute(e: Element, a: TAttr): void
{
    const q = qname(a.name, a.ns);
    const href = a.ns !== null && a.ns.href !== null && a.ns.href !== "" ? a.ns.href : null;
    let value = "";
    let has_entity = false;
    const parts: AttrPart[] = [];
    for (const c of a.children)
    {
        if (c.kind === "entity")
        {
            has_entity = true;
            value += "&" + c.name + ";";
            parts.push({ entity: c.name });
        }
        else
        {
            value += c.content;
            parts.push({ text: c.content });
        }
    }
    if (href === null && a.ns !== null && a.ns.prefix !== null) e.setAttribute(q, value);
    else e.setAttributeNS(href, q, value);
    const attr = href === null && a.ns !== null && a.ns.prefix !== null ? e.getAttributeNode(q) : e.getAttributeNodeNS(href, a.name);
    if (attr === null) return;
    if (a.children.length === 0) markAttrParts(attr, null);
    else if (has_entity) markAttrParts(attr, parts);
}

/** A DOM exception while building the DOM, as an XMQDomError. */
function dom_error(e: unknown): XMQDomError
{
    const m = e instanceof Error ? e.message : String(e);
    return new XMQDomError("xmq: the xmq cannot be stored in a W3C DOM: " + m);
}

/**
 * Turn the parsed tree into a W3C Document. A Document holds at most one element, a
 * doctype before it, comments and processing instructions, but no text.
 */
export function tree_to_document(tdoc: TDocument, impl: DOMImplementation): Document
{
    let elements = 0;
    for (const c of tdoc.children)
    {
        if (c.kind === "element" || c.kind === "foreign") elements++;
        if (c.kind === "text" || c.kind === "entity" || elements > 1)
        {
            throw new XMQDomError("xmq: a W3C Document can only have one root element and no text outside of it, use the root option to give an implicit root element");
        }
    }
    try
    {
        const doc = impl.createDocument(null, "", null);
        for (const c of tdoc.children)
        {
            if (c.kind === "dtd")
            {
                if (doc.doctype !== null || doc.documentElement !== null)
                {
                    throw new XMQDomError("xmq: a W3C Document can only have one doctype, before the root element");
                }
                const [name, pub, sys] = doctype_parts(c.content);
                const dt = impl.createDocumentType(name, pub, sys);
                markDoctype(dt, c.content);
                doc.appendChild(dt);
                continue;
            }
            doc.appendChild(to_w3c(c, doc));
        }
        return doc;
    }
    catch (e)
    {
        if (e instanceof XMQDomError) throw e;
        throw dom_error(e);
    }
}
