// libxmq - Copyright (C) 2026 Fredrik Öhrström (spdx: MIT)
// The package loads as an es module and as commonjs, and the parse errors print like C.
//   node tests/libxmq_js/test_001_smoke.mjs build/libxmq_js/package
import { createRequire } from "node:module";
import path from "node:path";

const pkg = path.resolve(process.argv[2]);
const esm = await import(path.join(pkg, "esm/index.js"));
const cjs = createRequire(import.meta.url)(path.join(pkg, "cjs/index.js"));

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
    check(kind + " tokens", lib.tokenize("a=1", "debugtokens"), '[element_key "a" 1:1][equals "=" 1:2][element_value_text "1" 1:3]\n');
}

if (failed == 0) console.log("OK: smoke (esm and cjs)");
process.exit(failed == 0 ? 0 : 1);
