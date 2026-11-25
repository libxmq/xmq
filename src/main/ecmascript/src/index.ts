/* libxmq - Copyright (C) 2026 Fredrik Öhrström (spdx: MIT) */

/**
 * libxmq for javascript: parse xmq into a W3C DOM, print a DOM as xmq and convert a DOM
 * to json. A port of the Java implementation of libxmq (org.libxmq), which gives the
 * same output as the C implementation.
 */

import { tree_to_document, XMQParseIntoDOM, type XmlTextParser } from "./dom.js";
import { print_xmq, type PrintOptions } from "./printer.js";
import { json_value, type JsonOptions, type JsonValue } from "./json.js";

export { ParseErrorCode, parseErrorToString, XMQParseError } from "./errors.js";
export { tokenize, type TokenizeType } from "./tokens.js";
export { XMQDomError, type XmlTextParser } from "./dom.js";
export { entityName, attrParts, doctypeText, type AttrPart } from "./marks.js";
export { type PrintOptions } from "./printer.js";
export { type JsonOptions, type JsonValue } from "./json.js";

/** The options of parse. */
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
    /**
     * The implicit root element, like the --root option of the xmq command line: unless
     * the first element of the xmq has this name, it and everything after it is placed
     * inside a new root element with this name. Needed for xmq with several elements at
     * the top level, eg a = 1 b = 2, since a W3C Document has only one root element.
     */
    root?: string;
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
 *         elements or text outside of the root element (give the root option).
 */
export function parse(xmq: string, options: ParseOptions = {}): Document
{
    // The syntax errors need no DOM.
    const p = new XMQParseIntoDOM(xml_parser(options), options.root ?? null);
    p.parse(xmq, options.source ?? "-");
    return tree_to_document(p.doc_, implementation(options));
}

/**
 * Print a DOM node as xmq, like xmq FILE to-xmq: a document or a fragment with all its
 * nodes, or a single node with its descendants. Not the compact form (yet). The text
 * nodes that parse created for entities are printed as entities. A DOM that parse did not
 * make, eg the page of a browser, is printed trimmed like xmq trims the xml and html it
 * reads (see PrintOptions.trim), the elements of html with their lower case localName.
 * @param node The document, fragment, element or other node.
 * @param options The indentation and the escapes, as the to-xmq options of xmq.
 * @returns The xmq text, without a final newline.
 */
export function print(node: Node, options: PrintOptions = {}): string
{
    return print_xmq(node, options);
}

/**
 * Convert a DOM node to a javascript value with the json conventions of xmq, the value of
 * the json that xmq FILE to-json prints: eg car { model = Volvo year = 1999 } gives
 * { _: "car", model: "Volvo", year: 1999 }. The root element name is "_", attributes are
 * "_name", repeated names get [n], text among elements is "|", a comment "//". Numbers
 * become numbers, true false null become booleans and null. A DOM that parse did not make
 * is trimmed like print trims it. Where xmq prints text that is not one json value: a
 * comment, text or entity at the top or in an array is an object with its key, eg
 * { "//": "comment" }, and several values at the top (eg a processing instruction before
 * the root element) are an array.
 * @param node The document, fragment or element.
 * @param options trim, onWarning.
 * @returns The value, undefined for an empty document.
 */
export function toJson(node: Node, options: JsonOptions = {}): JsonValue | undefined
{
    return json_value(node, options);
}
