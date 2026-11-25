// libxmq - Copyright (C) 2026 Fredrik Öhrström (spdx: MIT)
// A typescript user of libxmq as an es module in node, with @xmldom/xmldom as the DOM.
// Compiled and run by tests/libxmq_js/test_009_types.mjs.
import { DOMImplementation, DOMParser, type Document as XDocument } from "@xmldom/xmldom";
import { parse, print, toJson, xmqToJson, tokenize, entityName, XMQParseError, XMQDomError, ParseErrorCode,
         type JsonValue, type PrintOptions, type ParseOptions, type JsonOptions } from "libxmq";

const dom = new DOMImplementation();
// parse gives the document type of the DOM implementation.
const doc: XDocument = parse("car { model = Volvo nbsp = &nbsp; }", { dom, xmlParser: new DOMParser(), source: "car.xmq" });
const options: PrintOptions = { indent: 2, escapeNon7bit: true, trim: false };
const text: string = print(doc, options);
const value: JsonValue | undefined = toJson(doc, { trim: false, onWarning: (m: string) => console.log(m) });
const value2 = xmqToJson("a = 1", { dom });
const p: ParseOptions = { root: "list" };
const j: JsonOptions = {};
const tokens: string = tokenize("a = 1", "debugtokens");
const nbsp = doc.documentElement!.lastChild!.firstChild!;
const name: string | null = entityName(nbsp);

// @ts-expect-error the indentation is a number
print(doc, { indent: "2" });
// @ts-expect-error tokenize has two types
tokenize("a = 1", "tokens");
// @ts-expect-error toJson has no pretty option, it gives values
toJson(doc, { pretty: true });

let code: ParseErrorCode | null = null;
try
{
    parse("a = (x)", { dom });
}
catch (e)
{
    if (e instanceof XMQParseError) code = e.code;
    if (e instanceof XMQDomError) code = null;
}

const ok = text === "car {\n  model = Volvo\n  nbsp  = &nbsp;\n}" && JSON.stringify(value) === JSON.stringify({ _: "car", model: "Volvo", nbsp: "\u00a0" })
    && value2 === 1 && tokens.startsWith("[element_key") && name === "nbsp" && code === ParseErrorCode.XMQ_ERROR_COMPOUND_MAY_NOT_CONTAIN
    && p.root === "list" && j.trim === undefined;
console.log(ok ? "OK: esm consumer" : "ERR: esm consumer " + JSON.stringify([text, value, value2, tokens, name, code]));
