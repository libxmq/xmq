// libxmq - Copyright (C) 2026 Fredrik Öhrström (spdx: MIT)
// Parse xmq into a W3C DOM (@xmldom/xmldom) with libxmq for javascript, print it again as
// xmq and compare the text with the C xmq: xmq --xmq FILE to-xmq OPTIONS. The inputs are
// those of inputs.mjs. Every input is printed with each option set below (the to-xmq
// options of the command line), and every case is classified into
// OUTPUT/print_results.txt:
//   SAME          the same xmq text
//   DIFF          a different text, see OUTPUT/print/*.diff
// Skipped, with the reason:
//   SKIP_C_REJECTS     C cannot parse the input, there is nothing printed to compare with.
//                      test_003_parse checks that javascript rejects it too.
//   SKIP_JS_REJECTS    javascript cannot parse the input into a W3C Document, also not with
//                      the implicit root (ROOT_NOT_ENOUGH of test_003_parse, which also
//                      checks the inputs that only javascript rejects).
// More than one root element: the input is parsed with the implicit root root: "root",
// and printed by C with --root=root, like test_003_parse.
// The test fails on any DIFF that is not listed with the reason in
// tests/libxmq_js/print_expected_differences.txt (name|options|reason, the options * are
// all option sets).
//   node tests/libxmq_js/test_005_print.mjs build/libxmq_js/package OUTPUT_DIR
// The xmq binary is $XMQ, default build/default/release/xmq.
import { spawnSync } from "node:child_process";
import { createRequire } from "node:module";
import fs from "node:fs";
import path from "node:path";
import { xmq_inputs } from "./inputs.mjs";

const require = createRequire(import.meta.url);
const pkg = path.resolve(process.argv[2]);
const output = path.join(path.resolve(process.argv[3]), "print");
const xmq = process.env.XMQ || "build/default/release/xmq";
const lib = require(path.join(pkg, "cjs/index.js"));
const xmldom = require("@xmldom/xmldom");
const dom = new xmldom.DOMImplementation();

// The option sets: the command line options and the print options.
const OPTION_SETS = [
    [[], {}],
    [["--indent=2"], { indent: 2 }],
    [["--indent=0"], { indent: 0 }],
    [["--escape-newlines"], { escapeNewlines: true }],
    [["--escape-non-7bit"], { escapeNon7bit: true }],
    [["--escape-tabs"], { escapeTabs: true }],
    [["--prefer-double-quotes"], { preferDoubleQuotes: true }],
];

// The xml parser for embedded xml in xmq: unknown entities are kept as the text &name;
// (xmldom reports them as errors, they are not errors here).
function xml_parser()
{
    return new xmldom.DOMParser({
        onError: (level, msg) =>
        {
            if (level === "warning") return;
            if (/entity not found/.test(msg)) return;
            throw new Error(msg);
        },
    });
}

const expected_diffs = new Set();
const ed_file = "tests/libxmq_js/print_expected_differences.txt";
if (fs.existsSync(ed_file))
{
    for (const l of fs.readFileSync(ed_file, "utf8").split("\n"))
    {
        if (l === "" || l.startsWith("#")) continue;
        const [name, opts] = l.split("|");
        expected_diffs.add(name + "|" + opts);
    }
}

fs.rmSync(output, { recursive: true, force: true });
fs.mkdirSync(output, { recursive: true });
const results = [];
const counts = {};
const failures = [];
for (const [name, text] of xmq_inputs())
{
    const base = name.replace(/[\/]/g, "_");
    const file = path.join(output, base + ".xmq");
    fs.writeFileSync(file, text);
    // Parse once, with the implicit root when needed.
    let doc = null, root = false, js_rejects = false;
    try
    {
        doc = lib.parse(text, { dom, source: file, xmlParser: xml_parser() });
    }
    catch (e)
    {
        if (e instanceof lib.XMQDomError)
        {
            root = true;
            try
            {
                doc = lib.parse(text, { dom, source: file, xmlParser: xml_parser(), root: "root" });
            }
            catch (e2)
            {
                js_rejects = true;
            }
        }
        else js_rejects = true;
    }
    for (const [args, options] of OPTION_SETS)
    {
        const opts = args.join(" ");
        const c = spawnSync(xmq, [...(root ? ["--root=root"] : []), "--xmq", file, "to-xmq", ...args], { encoding: "utf8" });
        let cls, detail = "";
        if (c.status !== 0)
        {
            cls = "SKIP_C_REJECTS";
        }
        else if (js_rejects)
        {
            cls = "SKIP_JS_REJECTS";
        }
        else
        {
            let js;
            try
            {
                // Like C, an empty document gets no final newline.
                js = lib.print(doc, options);
                if (js !== "") js += "\n";
            }
            catch (e)
            {
                js = "JS EXCEPTION " + e.stack;
            }
            cls = js === c.stdout ? "SAME" : "DIFF";
            if (cls === "DIFF")
            {
                const f = path.join(output, base + (opts ? "." + opts.replace(/[^a-z0-9]+/g, "_") : ""));
                fs.writeFileSync(f + ".c", c.stdout);
                fs.writeFileSync(f + ".js", js);
                detail = f;
            }
        }
        counts[cls] = (counts[cls] ?? 0) + 1;
        results.push(cls + " " + name + (opts ? " " + opts : ""));
        if (cls === "DIFF" && !expected_diffs.has(name + "|" + opts) && !expected_diffs.has(name + "|*"))
        {
            failures.push(name + (opts ? "(" + opts + ")" : ""));
        }
    }
}
fs.writeFileSync(path.join(path.dirname(output), "print_results.txt"), results.join("\n") + "\n");

const summary = Object.entries(counts).map(([k, v]) => k + " " + v).join(", ");
if (failures.length === 0)
{
    console.log("OK: print (" + summary + ")");
    process.exit(0);
}
console.log("ERR: print (" + summary + ") unexpected: " + failures.slice(0, 20).join(" ") + (failures.length > 20 ? " ..." : "") +
            "\nsee " + output + "/*.c and *.js");
process.exit(1);
