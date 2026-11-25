/* libxmq - Copyright (C) 2026 Fredrik Öhrström (spdx: MIT) */

/**
 * What the xmq parser remembers about the W3C DOM it built, because a W3C DOM cannot
 * store it (see doc/DECISIONS.md): the entity references, the attribute values that
 * contain entity references or have no value, and the full doctype. The nodes are held
 * weakly; a clone or normalize() loses the marks.
 */

/** The text nodes whose text is an entity reference, eg &nbsp; */
const entity_texts = new WeakSet<object>();

/** An attribute value as its parts: text, or an entity reference. */
export type AttrPart = { text: string } | { entity: string };

/**
 * The attributes whose value contains an entity reference, with the parts of the value,
 * and the attributes without a value (null), eg a(checked).
 */
const attr_parts = new WeakMap<object, AttrPart[] | null>();

/** The doctype text, eg 'a [<!ENTITY foo "bar">]', of a document type node. */
const doctype_texts = new WeakMap<object, string>();

/** Remember that the text node is an entity reference. */
export function markEntity(t: Text): void
{
    entity_texts.add(t);
}

/**
 * The name of the entity (without & and ;) if the node is a text node that the parser
 * created for an entity reference, otherwise null.
 */
export function entityName(n: Node): string | null
{
    if (!entity_texts.has(n)) return null;
    const d = (n as Text).data;
    return d.startsWith("&") && d.endsWith(";") ? d.substring(1, d.length - 1) : null;
}

/** Remember the parts of an attribute value, or null for an attribute without value. */
export function markAttrParts(a: Attr, parts: AttrPart[] | null): void
{
    attr_parts.set(a, parts);
}

/**
 * The parts of an attribute value that contains entity references, null if the attribute
 * has no value, undefined if nothing is remembered (the value is plain text).
 */
export function attrParts(a: Attr): AttrPart[] | null | undefined
{
    return attr_parts.has(a) ? attr_parts.get(a) : undefined;
}

/** Remember the full doctype text of a document type node. */
export function markDoctype(dt: DocumentType, text: string): void
{
    doctype_texts.set(dt, text);
}

/** The full doctype text that the parser read, or undefined. */
export function doctypeText(dt: DocumentType): string | undefined
{
    return doctype_texts.get(dt);
}

/** The documents that the xmq parser created, they are printed without trimming. */
const xmq_documents = new WeakSet<object>();

/** Remember that the document was parsed from xmq. */
export function markXmqDocument(d: Document): void
{
    xmq_documents.add(d);
}

/** True if the document was parsed from xmq by this library. */
export function isXmqDocument(d: Document | null): boolean
{
    return d !== null && xmq_documents.has(d);
}

/** A namespace declaration of an element, href null for a prefix that is never declared. */
export interface NsDecl
{
    prefix: string | null;
    href: string | null;
}

/**
 * The namespace declarations of the elements that use a prefix that is never declared,
 * in the order of the parser: a W3C DOM has no declaration without uri, C prints them as
 * "_xmlns:p": null in json.
 */
const ns_decls = new WeakMap<object, NsDecl[]>();

/** Remember all the namespace declarations of an element, also the undeclared prefixes. */
export function markNsDecls(e: Element, decls: NsDecl[]): void
{
    ns_decls.set(e, decls);
}

/** The namespace declarations of an element with undeclared prefixes, or undefined. */
export function nsDecls(e: Element): NsDecl[] | undefined
{
    return ns_decls.get(e);
}
