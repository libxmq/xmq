// libxmq - Copyright (C) 2026 Fredrik Öhrström (spdx: MIT)
// Like the other parsers (tests/test_max_depth_parsers.sh) 10000 nested elements are
// parsed into the DOM, 10001 are an error with the same message as the C xmq, and not a
// stack overflow (node and the browsers have a much smaller stack than C and Java).
//   node tests/libxmq_js/test_004_depth.mjs build/libxmq_js/package OUTPUT_DIR
// The xmq binary is $XMQ, default build/default/release/xmq.
import { spawnSync } from "node:child_process";
import { createRequire } from "node:module";
import fs from "node:fs";
import path from "node:path";

const require = createRequire(import.meta.url);
const lib = require(path.join(path.resolve(process.argv[2]), "cjs/index.js"));
const output = path.resolve(process.argv[3]);
const xmq = process.env.XMQ || "build/default/release/xmq";
const xmldom = require("@xmldom/xmldom");
const dom = new xmldom.DOMImplementation();

const MAX = 10000;
let failed = 0;
function err(m)
{
    console.log("ERR: depth " + m);
    failed++;
}

const ok = "a{".repeat(MAX) + "}".repeat(MAX) + "\n";
try
{
    const d = lib.parse(ok, { dom });
    let k = 0;
    for (let e = d.documentElement; e !== null; e = e.firstChild) k++;
    if (k !== MAX) err(MAX + " gave " + k + " nested elements");
    lib.tokenize(ok, "debugtokens");
}
catch (e)
{
    err(MAX + " nested elements: " + e.message);
}

const deep = "a{".repeat(MAX + 1) + "}".repeat(MAX + 1) + "\n";
fs.mkdirSync(output, { recursive: true });
const file = path.join(output, "deep.xmq");
fs.writeFileSync(file, deep);
const c = spawnSync(xmq, [file, "tokenize", "--type=debugtokens"], { encoding: "utf8" });
for (const [what, f] of [["parse", () => lib.parse(deep, { dom, source: file })],
                         ["tokenize", () => lib.tokenize(deep, "debugtokens", file)]])
{
    try
    {
        f();
        err(what + " accepted " + (MAX + 1) + " nested elements");
    }
    catch (e)
    {
        if (!(e instanceof lib.XMQParseError)) err(what + " " + (MAX + 1) + ": " + e.message);
        else if (e.message !== c.stderr.replace(/\n+$/, "")) err(what + " " + (MAX + 1) + "\nexpected: " + c.stderr + "     got: " + e.message);
    }
}

if (failed === 0) console.log("OK: depth (" + MAX + " nested elements parse, " + (MAX + 1) + " is the same error as " + xmq + ")");
process.exit(failed === 0 ? 0 : 1);
