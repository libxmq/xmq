// libxmq - Copyright (C) 2026 Fredrik Öhrström (spdx: MIT)
// Print a DOM that was not parsed from xmq, eg the DOM of a browser page, with libxmq for
// javascript and compare with what the C xmq prints after reading the same html or xml:
//   xmq --html|--xml FILE to-xmq             the default trimming, javascript print(doc)
//   xmq --trim=none --html|--xml FILE to-xmq  javascript print(doc, { trim: false })
// The DOM is the one of C, so that only the printing is compared, not two parsers: the
// untrimmed DOM of xmq --trim=none --html|--xml FILE to-xml is read with the xmldom
// DOMParser. For html it is then rebuilt as an html document like a browser builds it:
// the elements in the xhtml namespace with the nodeName in upper case, the attributes
// (also xmlns) without a namespace. The html parsers themselves differ (a browser adds
// head and body, C adds a doctype and gives a boolean attribute its name as value), that
// is not tested here.
// The inputs are every tracked .html and .htm file (html) and .xml file (xml), also the
// copied libxml2/libxslt suites. Every case is classified into OUTPUT/print_dom_results.txt:
//   SAME          the same xmq text, both with and without trimming
//   DIFF          a different text, see OUTPUT/print_dom/*.c and *.js
// Skipped, with the reason:
//   SKIP_C_REJECTS         C cannot read the file (many of the libxml2 tests are invalid).
//   SKIP_C_XML_UNREADABLE  the xml that C prints cannot be read by xmldom, eg a namespace
//                          prefix that is never declared or an invalid name in the
//                          libxml2 error tests, so there is no DOM to print.
//   SKIP_ENTITY_REF        the xml has entity references that xmldom cannot resolve: a W3C
//                          DOM cannot hold them (only parse marks entities as text nodes).
// The test fails on any DIFF that is not listed with the reason in
// tests/libxmq_js/print_dom_expected_differences.txt (name|reason).
//   node tests/libxmq_js/test_007_print_dom.mjs build/libxmq_js/package OUTPUT_DIR
// The xmq binary is $XMQ, default build/default/release/xmq.
import { execFileSync, spawnSync } from "node:child_process";
import { createRequire } from "node:module";
import fs from "node:fs";
import path from "node:path";
import { c_dom } from "./c_dom.mjs";

const require = createRequire(import.meta.url);
const pkg = path.resolve(process.argv[2]);
const output = path.join(path.resolve(process.argv[3]), "print_dom");
const xmq = process.env.XMQ || "build/default/release/xmq";
const lib = require(path.join(pkg, "cjs/index.js"));

const expected_diffs = new Set();
const ed_file = "tests/libxmq_js/print_dom_expected_differences.txt";
if (fs.existsSync(ed_file))
{
    for (const l of fs.readFileSync(ed_file, "utf8").split("\n"))
    {
        if (l === "" || l.startsWith("#")) continue;
        expected_diffs.add(l.split("|")[0]);
    }
}

const files = execFileSync("git", ["ls-files", "*.html", "*.htm", "*.xml"], { encoding: "utf8" }).split("\n").filter(f => f !== "");

fs.rmSync(output, { recursive: true, force: true });
fs.mkdirSync(output, { recursive: true });
const results = [];
const counts = {};
const failures = [];
for (const file of files)
{
    const html = /\.html?$/.test(file);
    const type = html ? "--html" : "--xml";
    const base = file.replace(/[\/]/g, "_");
    const run = (args) => spawnSync(xmq, args, { encoding: "utf8", timeout: 60000 });
    const [cls0, doc] = c_dom(xmq, file);
    let cls = cls0;
    if (cls === null)
    {
            cls = "SAME";
            for (const [args, options, what] of [[[type, file, "to-xmq"], {}, "trim"],
                                                 [["--trim=none", type, file, "to-xmq"], { trim: false }, "notrim"]])
            {
                const c = run(args);
                if (c.status !== 0)
                {
                    cls = "SKIP_C_REJECTS";
                    break;
                }
                let js;
                try
                {
                    js = lib.print(doc, options);
                    if (js !== "") js += "\n";
                }
                catch (e)
                {
                    js = "JS EXCEPTION " + e.stack;
                }
                if (js !== c.stdout)
                {
                    cls = "DIFF";
                    fs.writeFileSync(path.join(output, base + "." + what + ".c"), c.stdout);
                    fs.writeFileSync(path.join(output, base + "." + what + ".js"), js);
                }
            }
    }
    counts[cls] = (counts[cls] ?? 0) + 1;
    results.push(cls + " " + file);
    if (cls === "DIFF" && !expected_diffs.has(file)) failures.push(file);
}
fs.writeFileSync(path.join(path.dirname(output), "print_dom_results.txt"), results.join("\n") + "\n");

const summary = Object.entries(counts).map(([k, v]) => k + " " + v).join(", ");
if (failures.length === 0)
{
    console.log("OK: print dom (" + summary + ")");
    process.exit(0);
}
console.log("ERR: print dom (" + summary + ") unexpected: " + failures.slice(0, 20).join(" ") + (failures.length > 20 ? " ..." : "") +
            "\nsee " + output + "/*.c and *.js");
process.exit(1);
