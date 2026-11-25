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
