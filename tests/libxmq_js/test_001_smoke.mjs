// libxmq - Copyright (C) 2026 Fredrik Öhrström (spdx: MIT)
// The package loads as an es module and as commonjs.
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
    const e = new lib.XMQParseError(lib.ParseErrorCode.XMQ_ERROR_BODY_NOT_CLOSED, "a message", 2, 3, "stdin");
    check(kind + " instanceof", e instanceof Error && e instanceof lib.XMQParseError, true);
    check(kind + " position", e.code + " " + e.line + ":" + e.column + " " + e.source + " " + e.message, "8 2:3 stdin a message");
}

if (failed == 0) console.log("OK: smoke (esm and cjs)");
process.exit(failed == 0 ? 0 : 1);
