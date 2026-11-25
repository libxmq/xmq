// libxmq - Copyright (C) 2026 Fredrik Öhrström (spdx: MIT)
// A typescript user of libxmq as commonjs (require) in node, with @xmldom/xmldom.
// Compiled and run by tests/libxmq_js/test_009_types.mjs.
import xmldom = require("@xmldom/xmldom");
import libxmq = require("libxmq");

const doc = libxmq.parse("a = 1 b = 2", { dom: new xmldom.DOMImplementation(), root: "r" });
const text: string = libxmq.print(doc);
const value = libxmq.toJson(doc);
const ok = text === "r {\n    a = 1\n    b = 2\n}" && JSON.stringify(value) === '{"_":"r","a":1,"b":2}';
console.log(ok ? "OK: cjs consumer" : "ERR: cjs consumer " + JSON.stringify([text, value]));
