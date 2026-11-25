// libxmq - Copyright (C) 2026 Fredrik Öhrström (spdx: MIT)
// The package loads as an es module and as commonjs, the parse errors print like C, and
// parse and print work (with the xmldom DOM).
//   node tests/libxmq_js/test_001_smoke.mjs build/libxmq_js/package
import { createRequire } from "node:module";
import path from "node:path";

const pkg = path.resolve(process.argv[2]);
const esm = await import(path.join(pkg, "esm/index.js"));
const require = createRequire(import.meta.url);
const cjs = require(path.join(pkg, "cjs/index.js"));
const dom = new (require("@xmldom/xmldom").DOMImplementation)();

let failed = 0;
function check(name, got, expected)
{
    if (got === expected) return;
    console.log("ERR: smoke " + name + "\nexpected: " + JSON.stringify(expected) + "\n     got: " + JSON.stringify(got));
    failed++;
}

for (const [kind, lib] of [["esm", esm], ["cjs", cjs]])
{
    check(kind + " code", lib.ParseErrorCode.XMQ_ERROR_COMPOUND_MAY_NOT_CONTAIN, 11);
    check(kind + " message", lib.parseErrorToString(lib.ParseErrorCode.XMQ_ERROR_BODY_NOT_CLOSED), "body is not closed");
    // The same text as: printf "a = (x)\n" | xmq - tokenize --type=debugtokens
    let e = null;
    try { lib.tokenize("a = (x)\n", "debugtokens", "stdin"); } catch (x) { e = x; }
    check(kind + " error", e && e.message, "stdin:1:6: error: compound may only contain quotes and entities\na = (x)\n     ^");
    check(kind + " instanceof", e instanceof Error && e instanceof lib.XMQParseError, true);
    check(kind + " position", e && (e.code + " " + e.line + ":" + e.column), "11 1:6");
    // The same text as: printf "car(id=1){model=Volvo note='a b' nbsp=&nbsp;}\n" | xmq --xmq - to-xmq
    const doc = lib.parse("car(id=1){model=Volvo note='a b' nbsp=&nbsp;}", { dom });
    check(kind + " print", lib.print(doc), "car(id = 1)\n{\n    model = Volvo\n    note  = 'a b'\n    nbsp  = &nbsp;\n}");
    check(kind + " print node", lib.print(doc.documentElement.firstChild, { indent: 2 }), "model = Volvo");
    // A browser like html document: upper case nodeName, whitespace text, trimmed like
    // printf '<html><body>\n  <p>  Hi  </p>\n  <!--  c  -->\n</body></html>' | xmq --html - to-xmq
    const h = dom.createHTMLDocument("");
    while (h.firstChild !== null) h.removeChild(h.firstChild);
    const mk = (name) =>
    {
        const e = h.createElement(name);
        Object.defineProperty(e, "nodeName", { value: name.toUpperCase() });
        return e;
    };
    const html = h.appendChild(mk("html")), body = html.appendChild(mk("body")), p = mk("p");
    body.appendChild(h.createTextNode("\n  "));
    body.appendChild(p).appendChild(h.createTextNode("  Hi  "));
    body.appendChild(h.createTextNode("\n  "));
    body.appendChild(h.createComment("  c  "));
    body.appendChild(h.createTextNode("\n"));
    check(kind + " print html", lib.print(h), "html {\n    body {\n        p = '  Hi  '\n        // c\n    }\n}");
    // Untrimmed the whitespace is printed, with a leading newline like C prints a leading
    // entity: printf '&#10;\n' | xmq --xmq - to-xmq
    check(kind + " print html untrimmed", lib.print(p.parentNode.firstChild, { trim: false }), "\n&#10;\n'  '");
    // The same value as: printf "car(id=1){model=Volvo note='a b' nbsp=&nbsp;}\n" | xmq --xmq - to-json
    check(kind + " json", JSON.stringify(lib.toJson(doc)), '{"_":"car","_id":"1","model":"Volvo","note":"a b","nbsp":"\u00a0"}');
    check(kind + " json year", lib.toJson(lib.parse("car { year = 1999 ok = true }", { dom })).year, 1999);
    check(kind + " tokens", lib.tokenize("a=1", "debugtokens"), '[element_key "a" 1:1][equals "=" 1:2][element_value_text "1" 1:3]\n');
}

if (failed == 0) console.log("OK: smoke (esm and cjs)");
process.exit(failed == 0 ? 0 : 1);
