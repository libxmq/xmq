// libxmq - Copyright (C) 2026 Fredrik Öhrström (spdx: MIT)
// Print random documents as xmq with libxmq for javascript and with the C xmq, and compare
// the text. The xmq inputs of test_005_print are few and small, this test reaches the
// quoting, the compounds, the entities, the alignment and the comment printing with
// random text: quotes, newlines, CR, tabs, spaces at the ends of lines, nbsp and the other
// unicode spaces, braces, & = / *, non-ascii and characters outside of the BMP.
// A random document is written as xml (the text escaped as xml), read by the xmldom
// DOMParser into the DOM that javascript prints, and read by C. Every case is compared
// twice: untrimmed, print(doc, { trim: false }) against xmq --trim=none FILE.xml to-xmq
// OPTIONS, and trimmed (the default for a DOM that parse did not make) against
// xmq FILE.xml to-xmq OPTIONS. Many cases are
// one document, the children of the root element; when the document differs every case
// of it is compared alone, to name the cases that differ. The cases are the same every
// run (a fixed seed). Writes OUTPUT/print_fuzz/*.xml of the cases that differ.
//   node tests/libxmq_js/test_006_print_fuzz.mjs build/libxmq_js/package OUTPUT_DIR [CASES]
// The xmq binary is $XMQ, default build/default/release/xmq.
import { spawnSync } from "node:child_process";
import { createRequire } from "node:module";
import fs from "node:fs";
import path from "node:path";

const require = createRequire(import.meta.url);
const pkg = path.resolve(process.argv[2]);
const output = path.join(path.resolve(process.argv[3]), "print_fuzz");
const num_cases = Number(process.argv[4] ?? 4000);
const xmq = process.env.XMQ || "build/default/release/xmq";
const lib = require(path.join(pkg, "cjs/index.js"));
const xmldom = require("@xmldom/xmldom");

const OPTION_SETS = [
    [[], {}],
    [["--indent=2"], { indent: 2 }],
    [["--escape-newlines"], { escapeNewlines: true }],
    [["--escape-non-7bit"], { escapeNon7bit: true }],
    [["--escape-tabs"], { escapeTabs: true }],
    [["--prefer-double-quotes"], { preferDoubleQuotes: true }],
];
const PER_DOCUMENT = 100;

// A small deterministic random generator (mulberry32).
let seed = 4711;
function random()
{
    seed = (seed + 0x6d2b79f5) | 0;
    let t = seed;
    t = Math.imul(t ^ (t >>> 15), t | 1);
    t ^= t + Math.imul(t ^ (t >>> 7), t | 61);
    return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
}
function pick(a)
{
    return a[Math.floor(random() * a.length)];
}

const PIECES = ["a", "b", "xyz", "123", " ", "  ", "\n", "\n\n", " \n", "\n ", "\t", "\r\n", "\r", "'", "''", "'''",
                "\"", "\"\"", "(", ")", "{", "}", "&", "=", "/", "*", "//", "/*", "*/", "<", ">", " ", " ",
                "å", "€", "\u{1f600}", "-", "    x", "\n    y"];

/** A random text of a few pieces, sometimes empty. */
function random_text(max)
{
    const n = Math.floor(random() * max);
    let s = "";
    for (let i = 0; i < n; i++) s += pick(PIECES);
    return s;
}

/** A random comment text: an xml comment cannot contain -- or end with -. */
function random_comment()
{
    let s = random_text(6).replace(/\r/g, "");
    while (s.includes("--")) s = s.replace("--", "-");
    if (s.endsWith("-")) s += "x";
    return s;
}

/** The text escaped for xml content, the CR kept as a character reference. */
function esc_text(s)
{
    return s.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;").replace(/\r/g, "&#13;");
}

/** The text escaped for an xml attribute value in double quotes, the whitespace kept. */
function esc_attr(s)
{
    return esc_text(s).replace(/"/g, "&quot;").replace(/\n/g, "&#10;").replace(/\t/g, "&#9;");
}

const NAMES = ["a", "bb", "ccc", "dddddd", "e1", "f-g", "h.i"];

/** A random case: the xml of an element with random content. */
function random_case(depth)
{
    const name = pick(NAMES);
    let attrs = "";
    if (random() < 0.3)
    {
        const used = new Set();
        const n = 1 + Math.floor(random() * 3);
        for (let i = 0; i < n; i++)
        {
            const a = pick(NAMES);
            if (used.has(a)) continue;
            used.add(a);
            attrs += " " + a + "=\"" + esc_attr(random_text(5)) + "\"";
        }
    }
    const r = random();
    let content = "";
    if (r < 0.45)
    {
        // A key = value element.
        content = esc_text(random_text(7));
    }
    else if (r < 0.6 || depth > 2)
    {
        // An empty element, or a comment in it.
        if (random() < 0.5) content = "<!--" + random_comment() + "-->";
    }
    else
    {
        // Mixed content: elements, text and comments.
        const n = 1 + Math.floor(random() * 4);
        for (let i = 0; i < n; i++)
        {
            const k = random();
            if (k < 0.5) content += random_case(depth + 1);
            else if (k < 0.8) content += esc_text(random_text(5));
            else content += "<!--" + random_comment() + "-->";
        }
    }
    return "<" + name + attrs + ">" + content + "</" + name + ">";
}

function document(cases)
{
    return "<root>" + cases.join("") + "</root>";
}

const parser = new xmldom.DOMParser({ onError: (level, msg) => { if (level !== "warning") throw new Error(msg); } });

/** Compare the xml document with options, returns null or the [c, js] outputs. */
function compare(xml, file, args, options)
{
    fs.writeFileSync(file, xml);
    const c = spawnSync(xmq, [...(options.trim === false ? ["--trim=none"] : []), file, "to-xmq", ...args], { encoding: "utf8" });
    if (c.status !== 0) return ["C FAILED " + c.stderr, ""];
    const doc = parser.parseFromString(xml, "text/xml");
    let js = lib.print(doc, options);
    if (js !== "") js += "\n";
    return js === c.stdout ? null : [c.stdout, js];
}

fs.rmSync(output, { recursive: true, force: true });
fs.mkdirSync(output, { recursive: true });
const cases = [];
for (let i = 0; i < num_cases; i++) cases.push(random_case(0));

let same = 0;
const failures = [];
const SETS = [];
for (const [args, options] of OPTION_SETS)
{
    SETS.push([args, { ...options, trim: false }, "notrim"]);
    SETS.push([args, options, "trim"]);
}
for (const [args, options, trim] of SETS)
{
    const opts = [trim, ...args].join(" ");
    for (let d = 0; d < cases.length; d += PER_DOCUMENT)
    {
        const part = cases.slice(d, d + PER_DOCUMENT);
        const file = path.join(output, "doc.xml");
        if (compare(document(part), file, args, options) === null)
        {
            same += part.length;
            continue;
        }
        // Find the cases that differ.
        for (let i = 0; i < part.length; i++)
        {
            const r = compare(document([part[i]]), file, args, options);
            if (r === null)
            {
                same++;
                continue;
            }
            const n = "case_" + (d + i) + "_" + opts.replace(/[^a-z0-9]+/g, "_");
            fs.writeFileSync(path.join(output, n + ".xml"), document([part[i]]));
            fs.writeFileSync(path.join(output, n + ".c"), r[0]);
            fs.writeFileSync(path.join(output, n + ".js"), r[1]);
            failures.push(n);
        }
    }
}
fs.rmSync(path.join(output, "doc.xml"), { force: true });

const total = cases.length * SETS.length;
if (failures.length === 0)
{
    console.log("OK: print fuzz (" + same + " of " + total + " random cases the same as " + xmq + ")");
    process.exit(0);
}
console.log("ERR: print fuzz (" + failures.length + " of " + total + " random cases differ): " + failures.slice(0, 10).join(" ") +
            (failures.length > 10 ? " ..." : "") + "\nsee " + output);
process.exit(1);
