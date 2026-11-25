/* libxmq - Copyright (C) 2026 Fredrik Öhrström (spdx: MIT) */

/**
 * libxmq for javascript: parse xmq into a W3C DOM, print a DOM as xmq and convert a DOM
 * to json. A port of the Java implementation of libxmq (org.libxmq), which gives the
 * same output as the C implementation.
 */

import { tree_to_document, tree_to_fragment, XMQParseIntoDOM, type XmlTextParser } from "./dom.js";

export { ParseErrorCode, parseErrorToString, XMQParseError } from "./errors.js";
export { tokenize, type TokenizeType } from "./tokens.js";
export { XMQDomError, type XmlTextParser } from "./dom.js";
export { entityName, attrParts, doctypeText, type AttrPart } from "./marks.js";

/** The options of parse and parseFragment. */
export interface ParseOptions
{
    /**
     * The DOM implementation that creates the document. In a browser the default is
     * document.implementation, node has no DOM: pass eg new (require("@xmldom/xmldom").DOMImplementation)().
     */
    dom?: DOMImplementation;
    /** Where the xmq comes from, eg a file name, used in the error messages. Default "-". */
    source?: string;
    /**
     * Parses the xml fragments embedded in xmq (<b>bold</b>), default a new DOMParser
     * if there is one (a browser). Without one an embedded fragment is an error.
     */
    xmlParser?: XmlTextParser;
}

/** The options of parseFragment. */
export interface ParseFragmentOptions extends ParseOptions
{
    /** The document that owns the fragment, by default a new document of the dom. */
    document?: Document;
}

/** The DOM implementation of the options, or of the browser. */
function implementation(options: ParseOptions): DOMImplementation
{
    if (options.dom !== undefined) return options.dom;
    const g = globalThis as { document?: Document };
    if (g.document !== undefined && g.document.implementation !== undefined) return g.document.implementation;
    throw new Error("libxmq: there is no DOM, pass a DOMImplementation as the dom option (eg from @xmldom/xmldom)");
}

/** The xml parser of the options, or a DOMParser of the browser, or null. */
function xml_parser(options: ParseOptions): XmlTextParser | null
{
    if (options.xmlParser !== undefined) return options.xmlParser;
    const g = globalThis as { DOMParser?: new () => XmlTextParser };
    return g.DOMParser !== undefined ? new g.DOMParser() : null;
}

/**
 * Parse xmq into a new W3C Document.
 *
 * Entity references (eg &nbsp;) become text nodes whose text is the entity, which the
 * library remembers (see entityName), since a W3C DOM cannot store entity references.
 * @param xmq The xmq text.
 * @param options The DOM implementation, the source name for errors...
 * @returns The document.
 * @throws XMQParseError If the xmq cannot be parsed, with the same message as the C xmq.
 * @throws XMQDomError If the content cannot be stored in a Document, eg two root
 *         elements or text outside of the root element (use parseFragment).
 */
export function parse(xmq: string, options: ParseOptions = {}): Document
{
    // The syntax errors need no DOM.
    const p = new XMQParseIntoDOM(xml_parser(options));
    p.parse(xmq, options.source ?? "-");
    return tree_to_document(p.doc_, implementation(options));
}

/**
 * Parse xmq into a W3C DocumentFragment, which can hold any xmq content except a doctype:
 * several elements and text, eg to insert into an existing document.
 * @param xmq The xmq text.
 * @param options The DOM implementation or the owner document, the source name for errors...
 * @returns The fragment.
 * @throws XMQParseError If the xmq cannot be parsed, with the same message as the C xmq.
 * @throws XMQDomError If the content cannot be stored in a fragment (a doctype).
 */
export function parseFragment(xmq: string, options: ParseFragmentOptions = {}): DocumentFragment
{
    const p = new XMQParseIntoDOM(xml_parser(options));
    p.parse(xmq, options.source ?? "-");
    const doc = options.document ?? implementation(options).createDocument(null, "", null);
    return tree_to_fragment(p.doc_, doc);
}
