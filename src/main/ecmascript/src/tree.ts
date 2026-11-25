/* libxmq - Copyright (C) 2026 Fredrik Öhrström (spdx: MIT) */

/**
 * The tree that the xmq parser builds before it is turned into a W3C DOM. It is the part
 * of the Java DOM (org.libxmq.imp.xml XmlNode, XmlElement, XmlNs...) that XMQParseIntoDOM
 * uses, so that the parser can be ported as it is: a W3C DOM cannot change the namespace
 * of an element after it was created (xmq declares it later in the attributes), and it
 * cannot store entity references.
 */

/** A namespace (Java XmlNs), the href is null while it is not declared yet. */
export class TNs
{
    constructor(public prefix: string | null, public href: string | null)
    {
    }
}

/** The predefined xml namespace (Java XmlElement.XML_NS). */
export const XML_NS = new TNs("xml", "http://www.w3.org/XML/1998/namespace");

export type TKind = "document" | "element" | "attribute" | "text" | "entity" | "comment" | "pi" | "dtd" | "foreign";

/** A node (Java XmlNode): the children are kept in an array. */
export class TNode
{
    parent: TNode | null = null;
    children: TNode[] = [];

    /**
     * @param kind The kind of node.
     * @param name The element, attribute, entity (without & and ;) or pi name.
     * @param content The text, comment or pi content, or the dtd text.
     */
    constructor(public kind: TKind, public name: string = "", public content: string = "")
    {
    }

    /** XmlNode.appendRaw: append a child without merging adjacent text nodes. */
    appendRaw(child: TNode): TNode
    {
        child.unlink();
        child.parent = this;
        this.children.push(child);
        return child;
    }

    /**
     * XmlNode.addChild (libxml2 xmlAddChild): a text child is merged into a last child
     * that is a text node, then that node is returned.
     */
    addChild(child: TNode): TNode
    {
        const last = this.children.length > 0 ? this.children[this.children.length - 1] : null;
        if (child.kind === "text" && last !== null && last.kind === "text")
        {
            last.content += child.content;
            return last;
        }
        return this.appendRaw(child);
    }

    /** XmlNode.unlink */
    unlink(): void
    {
        if (this.parent !== null)
        {
            const k = this.parent.children.indexOf(this);
            if (k >= 0) this.parent.children.splice(k, 1);
            this.parent = null;
        }
    }

    /** XmlNode.replaceWith: put n where this node is. */
    replaceWith(n: TNode): void
    {
        const p = this.parent;
        if (p === null) return;
        n.unlink();
        const k = p.children.indexOf(this);
        p.children[k] = n;
        n.parent = p;
        this.parent = null;
    }
}

/** An element (Java XmlElement). */
export class TElement extends TNode
{
    ns: TNs | null = null;
    nsDefs: TNs[] = [];
    attributes: TAttr[] = [];

    constructor(name: string)
    {
        super("element", name);
    }

    /** XmlElement.declareNs: declare a namespace, or return the declaration of the prefix. */
    declareNs(prefix: string | null, href: string | null): TNs
    {
        for (const n of this.nsDefs)
        {
            if (n.prefix === prefix) return n;
        }
        const n = new TNs(prefix, href);
        this.nsDefs.push(n);
        return n;
    }

    /** XmlElement.searchNs (libxml2 xmlSearchNs): the namespace of a prefix here or above. */
    searchNs(prefix: string | null): TNs | null
    {
        if (prefix === "xml") return XML_NS;
        for (let n: TNode | null = this; n instanceof TElement; n = n.parent)
        {
            for (const ns of n.nsDefs)
            {
                if (ns.prefix === prefix) return ns;
            }
        }
        return null;
    }

    /** XmlElement.addAttribute */
    addAttribute(a: TAttr): void
    {
        a.parent = this;
        this.attributes.push(a);
    }
}

/** An attribute (Java XmlAttr): the value is its text and entity children, none if it has no value. */
export class TAttr extends TNode
{
    ns: TNs | null = null;

    constructor(name: string)
    {
        super("attribute", name);
    }
}

/** The document (Java XmlDocument). */
export class TDocument extends TNode
{
    constructor()
    {
        super("document");
    }

    /** XmlDocument.rootElement: the first element child. */
    rootElement(): TElement | null
    {
        for (const c of this.children)
        {
            if (c instanceof TElement) return c;
        }
        return null;
    }

    /** XmlDocument.setRootElement (libxml2 xmlDocSetRootElement): replace any existing root. */
    setRootElement(e: TElement): void
    {
        const old = this.rootElement();
        if (old === e) return;
        if (old !== null) old.replaceWith(e);
        else this.appendRaw(e);
    }
}
