// libxmq - Copyright (C) 2026 Fredrik Öhrström (spdx: MIT)
// A differential fuzzer of the xmq parsers and pretty printers of C (build/default/release/xmq),
// Java (build/xmqj, run in one JVM by FuzzCliDriver.java) and javascript (libxmq for
// javascript, build/libxmq_js/package). Every round generates random xmq: elements,
// key = value, attributes, namespaces, quotes of every kind of content and quote runs,
// entities, compounds, comments, pis, doctypes, several roots, and also mutations (some
// inserted, deleted or duplicated characters) of the generated xmq and of the xmq inputs
// of the test suite, so many cases are invalid. Every case is compared:
//   C_JAVA        xmq and xmqj with --xmq FILE to-xml, and to-xmq with every option set
//                 (also --compact): the same exit code, stdout and stderr.
//   C_JS_PARSE    javascript parse: the same error message as C, or the same DOM as the
//                 to-xml of C (read with xmldom). With several roots javascript parses
//                 with the implicit root root: "root" and C with --root=root.
//   C_JS_PRINT    javascript print with every option set (not compact): the same text as
//                 to-xmq of C.
//   ROUNDTRIP     javascript print(parse(print(doc))) is print(doc): printing is stable.
// Skipped, counted: C_SILENT (C fails without a message, a known C bug in doc/TODO.md,
// then C cannot be compared), JS_NO_DOM (javascript cannot hold the content in a W3C
// Document, also not with the implicit root), C_XML_UNREADABLE (xmldom cannot read the xml
// of C, eg an undeclared namespace prefix), JS_DOCTYPE_NOT_CHECKED (C rejects the doctype, javascript does not check it, a
// known limitation in the README). The DOMs are compared
// as the dump of dom_dump.mjs, with the predefined entities (that javascript keeps as
// entity text) as their characters, the doctype ids without the quotes that xmldom keeps,
// a CR in a comment as a newline (xml normalizes it when xmldom reads the C xml) and the
// characters that xml does not allow as U+FFFD (C writes them as &#xFFFD;).
// Every finding is saved under OUTPUT/SEED/case_N/ with the input and the outputs. A
// round prints its counts; the test fails if there is any finding.
//   node tests/fuzz_parse_print/fuzz_parse_print.mjs build/libxmq_js/package OUTPUT
// Environment: CASES (cases per round, default 500), ROUNDS (default 1, 0 runs until
// stopped), SEED (default random, printed so that a run can be repeated), XMQ, XMQJ_CLASSES
// (default build/classes).
import { spawn, spawnSync } from "node:child_process";
import { createRequire } from "node:module";
import fs from "node:fs";
import path from "node:path";
import { xmq_inputs } from "../libxmq_js/inputs.mjs";
import { dump } from "../libxmq_js/dom_dump.mjs";

const require = createRequire(import.meta.url);
const pkg = path.resolve(process.argv[2]);
const output = path.resolve(process.argv[3]);
const lib = require(path.join(pkg, "cjs/index.js"));
const xmldom = require("@xmldom/xmldom");
const dom = new xmldom.DOMImplementation();
const xmq = path.resolve(process.env.XMQ || "build/default/release/xmq");
const classes = path.resolve(process.env.XMQJ_CLASSES || "build/classes");
const driver = path.resolve("tests/fuzz_parse_print/FuzzCliDriver.java");
const CASES = Number(process.env.CASES ?? 500);
const ROUNDS = Number(process.env.ROUNDS ?? 1);
let seed = process.env.SEED !== undefined ? Number(process.env.SEED) : Math.floor(Math.random() * 2 ** 31);
const first_seed = seed;

// The option sets of to-xmq. The compact form is only compared between C and Java.
const PRINT_OPTIONS = [
    [[], {}],
    [["--indent=2"], { indent: 2 }],
    [["--escape-newlines"], { escapeNewlines: true }],
    [["--escape-non-7bit"], { escapeNon7bit: true }],
    [["--escape-tabs"], { escapeTabs: true }],
    [["--prefer-double-quotes"], { preferDoubleQuotes: true }],
];
const C_JAVA_ONLY = [["--compact"]];

// A small deterministic random generator (mulberry32), seeded by SEED.
function random()
{
    seed = (seed + 0x6d2b79f5) | 0;
    let t = seed;
    t = Math.imul(t ^ (t >>> 15), t | 1);
    t ^= t + Math.imul(t ^ (t >>> 7), t | 61);
    return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
}
const pick = (a) => a[Math.floor(random() * a.length)];
const chance = (p) => random() < p;

const NAMES = ["a", "b", "car", "x1", "_", "long_name", "a.b", "a-b", "p:x", "q:y", "xmlns", "_x", "A", "Z9"];
const WORDS = ["1", "123", "-1.5", "true", "null", "Volvo", "x", "abc", "a/b", "a=b", "&", "http://x.org/y", "_"];
const PIECES = ["a", "b", "xyz", " ", "  ", "\n", "\n\n", " \n", "\n  ", "\t", "\r\n", "\r", "'", "''", "'''", "\"",
                "\"\"", "(", ")", "{", "}", "&", "&lt;", "=", "/", "*", "//", "/*", "*/", "<", ">", " ", " ",
                "å", "€", "\u{1f600}", "-", "    x", "\n    y", "\\", "\u0007"];

function ws()
{
    return pick([" ", " ", "  ", "\n", "\n    ", "\n\n", "\n  "]);
}

function name()
{
    return pick(NAMES);
}

function quote_content()
{
    let s = "";
    const n = Math.floor(random() * 6);
    for (let i = 0; i < n; i++) s += pick(PIECES);
    return s;
}

function quote()
{
    const q = chance(0.7) ? "'" : "\"";
    const content = quote_content();
    // Enough quotes for the content, sometimes too few or too many (invalid).
    let run = 0, max = 0;
    for (const c of content)
    {
        run = c === q ? run + 1 : 0;
        if (run > max) max = run;
    }
    let n = max === 0 ? 1 : max + 1;
    if (n === 2) n = 3;
    if (chance(0.05)) n = Math.max(1, n + pick([-1, 1]));
    const nl = n >= 3 || content.startsWith(q) || content.endsWith(q);
    if (nl && chance(0.8)) return q.repeat(n) + "\n" + content + "\n" + q.repeat(n);
    return q.repeat(n) + content + q.repeat(n);
}

function entity()
{
    return pick(["&nbsp;", "&lt;", "&gt;", "&amp;", "&quot;", "&apos;", "&#10;", "&#x9;", "&#32;", "&copy;", "&unknown;", "&#128512;"]);
}

function value()
{
    const r = random();
    if (r < 0.35) return pick(WORDS);
    if (r < 0.75) return quote();
    if (r < 0.85) return entity();
    // A compound.
    let c = "(";
    const n = 1 + Math.floor(random() * 3);
    for (let i = 0; i < n; i++) c += ws() + (chance(0.6) ? quote() : entity());
    return c + ws() + ")";
}

function comment()
{
    const text = quote_content().replace(/\*\//g, "* /");
    if (chance(0.4) && !text.includes("\n")) return "// " + text + "\n";
    const slashes = chance(0.8) ? "/" : "//";
    return slashes + "*" + text + "*" + slashes;
}

function attributes()
{
    let a = "(";
    const used = new Set();
    const n = 1 + Math.floor(random() * 3);
    for (let i = 0; i < n; i++)
    {
        let k = chance(0.15) ? pick(["xmlns", "xmlns:p", "xmlns:q"]) : name();
        if (used.has(k)) continue;
        used.add(k);
        a += (i > 0 ? ws() : "") + k;
        if (k.startsWith("xmlns")) a += "=" + pick(["urn:x", "http://a.b/c", "''", "'u v'"]);
        else if (chance(0.8)) a += ws() + "=" + ws() + value();
    }
    return a + ")";
}

function element(depth)
{
    let e = name();
    if (chance(0.25)) e += attributes();
    const r = random();
    if (r < 0.45) return e + ws() + "=" + ws() + value();
    if (r < 0.75 && depth < 5)
    {
        let b = e + ws() + "{";
        const n = Math.floor(random() * 4);
        for (let i = 0; i < n; i++) b += ws() + content(depth + 1);
        return b + ws() + "}";
    }
    return e;
}

function content(depth)
{
    const r = random();
    if (r < 0.75) return element(depth);
    if (r < 0.85) return quote();
    if (r < 0.92) return comment();
    if (r < 0.96) return entity();
    return "?pi" + ws() + "=" + ws() + value();
}

function document()
{
    let d = "";
    if (chance(0.1)) d += "!DOCTYPE = " + pick(["html", "'x [<!ENTITY a \"b\">]'", "'html PUBLIC \"-//W3C//DTD XHTML 1.0//EN\" \"x.dtd\"'"]) + "\n";
    if (chance(0.15)) d += comment() + "\n";
    if (chance(0.05)) d += "?xml-stylesheet = 'type=\"text/xsl\"'\n";
    d += element(0);
    // Sometimes several roots or text at the top.
    while (chance(0.1)) d += ws() + content(1);
    if (chance(0.1)) d += "\n" + comment();
    return d + "\n";
}

const MUTATION = ["{", "}", "(", ")", "'", "\"", "=", "&", ";", "/", "*", " ", "\n", "\t", "<", ">", "!", "?", "a", ":", " "];
function mutate(s)
{
    const n = 1 + Math.floor(random() * 3);
    for (let k = 0; k < n; k++)
    {
        const i = Math.floor(random() * (s.length + 1));
        const r = random();
        if (r < 0.4) s = s.substring(0, i) + pick(MUTATION) + s.substring(i);
        else if (r < 0.8) s = s.substring(0, i) + s.substring(i + 1);
        else
        {
            const j = Math.min(s.length, i + 1 + Math.floor(random() * 10));
            s = s.substring(0, j) + s.substring(i, j) + s.substring(j);
        }
    }
    return s;
}

const corpus = xmq_inputs().map(([, t]) => t).filter(t => t.length < 4000);
function random_case()
{
    const r = random();
    // A mutation can split a surrogate pair, the file would then have U+FFFD where the
    // javascript text has a lone surrogate: make the text well formed.
    if (r < 0.15) return mutate(pick(corpus)).toWellFormed();
    const d = document();
    return r < 0.35 ? mutate(d).toWellFormed() : d;
}

/** Run the commands with C, 8 at a time. */
async function run_c(commands)
{
    const results = new Array(commands.length);
    let next = 0;
    async function worker()
    {
        while (next < commands.length)
        {
            const k = next++;
            results[k] = await new Promise((resolve) =>
            {
                const p = spawn(xmq, commands[k]);
                const out = [], err = [];
                p.stdout.on("data", (d) => out.push(d));
                p.stderr.on("data", (d) => err.push(d));
                p.on("close", (rc) => resolve({ rc, out: Buffer.concat(out).toString("utf8"), err: Buffer.concat(err).toString("utf8") }));
            });
        }
    }
    await Promise.all(Array.from({ length: 8 }, worker));
    return results;
}

/** Run the commands with Java in one JVM. */
function run_java(commands, dir)
{
    const manifest = path.join(dir, "java_manifest.txt");
    const result = path.join(dir, "java_results.jsonl");
    fs.writeFileSync(manifest, commands.map(c => c.join("\u0001")).join("\n") + "\n");
    const r = spawnSync("java", ["-cp", classes, driver, manifest, result], { encoding: "utf8", maxBuffer: 1 << 28 });
    if (r.status !== 0) throw new Error("FuzzCliDriver failed: " + r.stderr);
    return fs.readFileSync(result, "utf8").trim().split("\n").map(l =>
    {
        const o = JSON.parse(l);
        return { rc: o.rc, out: Buffer.from(o.out, "base64").toString("utf8"), err: Buffer.from(o.err, "base64").toString("utf8") };
    });
}

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

// The predefined entities, that the xml of C gives xmldom as characters.
const PREDEFINED = { lt: "<", gt: ">", amp: "&", quot: "\"", apos: "'" };

/** The dump of the DOM of C (its to-xml read by xmldom). */
function dump_c(n)
{
    const a = [];
    dump(n, 0, a, { normalize: true });
    return a.join("\n");
}

/**
 * The namespace of an element as the xml of C shows it: the namespace that its prefix (or
 * no prefix) has from the xmlns attributes in scope. C writes an element without declaring
 * its namespace when the namespace was not declared on it or above it.
 */
function visible_ns(e)
{
    const prefix = e.nodeName.includes(":") ? e.nodeName.substring(0, e.nodeName.indexOf(":")) : null;
    const decl = prefix === null ? "xmlns" : "xmlns:" + prefix;
    for (let p = e; p !== null && p.nodeType === 1; p = p.parentNode)
    {
        const a = p.getAttributeNode(decl);
        if (a !== null) return a.value === "" ? null : a.value;
    }
    return null;
}

/** The dump of the DOM of javascript, the predefined entities as their characters like the C side. */
function dump_js(n, scoped_ns = false)
{
    const a = [];
    dump(n, 0, a, {
        normalize: true,
        ns: scoped_ns ? visible_ns : undefined,
        fffd: true,
        text: (t) =>
        {
            const e = lib.entityName(t);
            return e !== null && e in PREDEFINED ? PREDEFINED[e] : t.data;
        },
        attr: (attr) =>
        {
            const parts = lib.attrParts(attr);
            if (!parts) return attr.value;
            return parts.map(p => "entity" in p ? (p.entity in PREDEFINED ? PREDEFINED[p.entity] : "&" + p.entity + ";") : p.text).join("");
        },
    });
    return a.join("\n");
}

/** The javascript parse of a case: { doc } or { error } or { no_dom }, root says if the implicit root was used. */
function js_parse(text, file)
{
    try
    {
        return { doc: lib.parse(text, { dom, source: file, xmlParser: xml_parser() }), root: false };
    }
    catch (e)
    {
        if (!(e instanceof lib.XMQDomError)) return { error: e, root: false };
    }
    try
    {
        return { doc: lib.parse(text, { dom, source: file, xmlParser: xml_parser(), root: "root" }), root: true };
    }
    catch (e)
    {
        if (e instanceof lib.XMQDomError) return { no_dom: true, root: true };
        return { error: e, root: true };
    }
}

/**
 * True if an element in the xml of C has the same attribute twice (C keeps a duplicate
 * attribute of the xmq, a W3C element can hold it once, see doc/TODO.md).
 */
function has_duplicate_attribute(xml)
{
    for (const m of xml.matchAll(/<[^\s>\/!?]+((?:\s+[^\s=>]+="[^"]*")*)\s*\/?>/g))
    {
        const names = [...m[1].matchAll(/([^\s=]+)="/g)].map(n => n[1]);
        if (new Set(names).size !== names.length) return true;
    }
    return false;
}

const totals = {};
let total_findings = 0;
const count = (k) => { totals[k] = (totals[k] ?? 0) + 1; };

// The findings of an earlier run with the same seed are removed.
fs.rmSync(path.join(output, String(first_seed)), { recursive: true, force: true });
console.log("fuzz_parse_print: SEED=" + first_seed + " CASES=" + CASES + " ROUNDS=" + (ROUNDS === 0 ? "until stopped" : ROUNDS));
for (let round = 0; ROUNDS === 0 || round < ROUNDS; round++)
{
    const round_seed = seed;
    const dir = path.join(output, String(first_seed), "round_" + round);
    fs.rmSync(dir, { recursive: true, force: true });
    fs.mkdirSync(dir, { recursive: true });
    const cases = [];
    for (let i = 0; i < CASES; i++)
    {
        const text = random_case();
        const file = path.join(dir, "case_" + i + ".xmq");
        fs.writeFileSync(file, text);
        cases.push({ i, text, file, js: js_parse(text, file) });
    }

    // The command lines: the same for C and Java, and for C with --root=root when
    // javascript needed the implicit root.
    const base = [];
    const root = [];
    for (const c of cases)
    {
        c.base = base.length;
        base.push(["--xmq", c.file, "to-xml"]);
        for (const [args] of PRINT_OPTIONS) base.push(["--xmq", c.file, "to-xmq", ...args]);
        for (const args of C_JAVA_ONLY) base.push(["--xmq", c.file, "to-xmq", ...args]);
        if (c.js.root)
        {
            c.rootk = root.length;
            root.push(["--root=root", "--xmq", c.file, "to-xml"]);
            for (const [args] of PRINT_OPTIONS) root.push(["--root=root", "--xmq", c.file, "to-xmq", ...args]);
        }
    }
    const cres = await run_c(base);
    const croot = await run_c(root);
    const jres = run_java(base, dir);

    let findings = 0;
    for (const c of cases)
    {
        const found = [];
        const per = 1 + PRINT_OPTIONS.length + C_JAVA_ONLY.length;
        const cb = cres.slice(c.base, c.base + per);
        const jb = jres.slice(c.base, c.base + per);
        // C silently failing: the known C bug, nothing to compare C with.
        if (cb[0].rc !== 0 && cb[0].err === "")
        {
            count("C_SILENT");
            continue;
        }
        // C and Java.
        for (let k = 0; k < per; k++)
        {
            const a = cb[k], b = jb[k];
            if (a.rc !== b.rc || a.out !== b.out || a.err !== b.err) found.push(["C_JAVA", base[c.base + k].slice(2).join(" "), a, b]);
        }
        if (found.length === 0) count("C_JAVA_SAME");
        // C and javascript.
        const cc = c.js.root ? croot.slice(c.rootk, c.rootk + 1 + PRINT_OPTIONS.length) : cb;
        if (c.js.no_dom)
        {
            count("JS_NO_DOM");
        }
        else if (cc[0].rc === 0 && has_duplicate_attribute(cc[0].out))
        {
            count("KNOWN_DUPLICATE_ATTRIBUTE");
        }
        else if (cc[0].rc !== 0 && /error: doctype could not be parsed/.test(cc[0].err))
        {
            // A known limitation: javascript does not check the doctype (README).
            count("JS_DOCTYPE_NOT_CHECKED");
        }
        else if (cc[0].rc !== 0)
        {
            const msg = c.js.error ? c.js.error.message : "(javascript accepted it)";
            if (msg === cc[0].err.replace(/\n+$/, "")) count("C_JS_PARSE_SAME_ERROR");
            else found.push(["C_JS_PARSE", "error", cc[0], { rc: c.js.error ? 1 : 0, out: "", err: msg + "\n" }]);
        }
        else if (c.js.error)
        {
            found.push(["C_JS_PARSE", "javascript rejects", cc[0], { rc: 1, out: "", err: String(c.js.error.stack) }]);
        }
        else
        {
            let cdoc = null;
            try
            {
                cdoc = xml_parser().parseFromString(cc[0].out.replace(/^<\?xml[^>]*\?>\n?/, ""), "text/xml");
            }
            catch (e)
            {
                cdoc = null;
            }
            if (cdoc === null) count("C_XML_UNREADABLE");
            else
            {
                const a = dump_c(cdoc);
                const b = dump_js(c.js.doc);
                if (a === b) count("C_JS_PARSE_SAME_DOM");
                // The known C bug of doc/TODO.md: a default namespace is also used by the
                // following siblings, C writes them without it (the DOMs are the same).
                else if (a === dump_js(c.js.doc, true)) count("KNOWN_DEFAULT_NS_LEAK");
                else found.push(["C_JS_PARSE", "dom", { rc: 0, out: a, err: "" }, { rc: 0, out: b, err: "" }]);
            }
            for (let k = 0; k < PRINT_OPTIONS.length; k++)
            {
                let js;
                try
                {
                    js = lib.print(c.js.doc, PRINT_OPTIONS[k][1]);
                    if (js !== "") js += "\n";
                }
                catch (e)
                {
                    js = "JS EXCEPTION " + e.stack;
                }
                if (js !== cc[1 + k].out) found.push(["C_JS_PRINT", PRINT_OPTIONS[k][0].join(" ") || "default", cc[1 + k], { rc: 0, out: js, err: "" }]);
                else count("C_JS_PRINT_SAME");
            }
            // Printing is stable.
            try
            {
                const p1 = lib.print(c.js.doc);
                const p2 = lib.print(lib.parse(p1, { dom, xmlParser: xml_parser(), root: c.js.root ? "root" : undefined }));
                if (p1 === p2) count("ROUNDTRIP_SAME");
                else found.push(["ROUNDTRIP", "print(parse(print))", { rc: 0, out: p1, err: "" }, { rc: 0, out: p2, err: "" }]);
            }
            catch (e)
            {
                found.push(["ROUNDTRIP", "the printed xmq does not parse", { rc: 0, out: lib.print(c.js.doc), err: "" }, { rc: 1, out: "", err: String(e.message) }]);
            }
        }
        if (found.length === 0) continue;
        findings++;
        const cdir = path.join(output, String(first_seed), "case_" + round + "_" + c.i);
        fs.mkdirSync(cdir, { recursive: true });
        fs.writeFileSync(path.join(cdir, "input.xmq"), c.text);
        let report = "";
        for (const [kind, what, a, b] of found)
        {
            count(kind);
            const left = kind === "C_JAVA" ? "C" : kind === "ROUNDTRIP" ? "first print" : "C";
            const right = kind === "C_JAVA" ? "Java" : kind === "ROUNDTRIP" ? "second print" : "javascript";
            report += "== " + kind + " " + what + "\n-- " + left + " rc=" + a.rc + "\n" + a.out + a.err + "\n-- " + right + " rc=" + b.rc + "\n" + b.out + b.err + "\n";
        }
        fs.writeFileSync(path.join(cdir, "report.txt"), report);
    }
    total_findings += findings;
    fs.rmSync(dir, { recursive: true, force: true });
    const summary = Object.entries(totals).sort().map(([k, v]) => k + " " + v).join(", ");
    console.log((findings === 0 ? "OK" : "ERR") + ": round " + round + " (seed " + round_seed + "): " + findings + " findings of " + CASES +
                " cases. Total: " + summary);
}
if (total_findings > 0) console.log("see " + path.join(output, String(first_seed)) + "/case_*/report.txt");
process.exit(total_findings === 0 ? 0 : 1);
