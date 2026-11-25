// libxmq - Copyright (C) 2026 Fredrik Öhrström (spdx: MIT)
// Convert DOMs to javascript values with toJson of libxmq for javascript and compare them
// with the json of the C xmq: the text of xmq ... to-json is read into a value with
// read_c_json below and both values are compared (as JSON.stringify, the key order
// included). The C text is not always strict json, read_c_json reads what C prints: the
// escapes \a \v \xHH, the numbers that C accepts (eg 1.e5, -01, read as javascript reads
// them with Number), and where C prints something that is not one json value, the value
// toJson gives: a "key":value pair at the top or in an array is an object { key: value },
// several values at the top are an array.
// The DOMs:
//   xmq   the inputs of inputs.mjs parsed by parse (with the implicit root root: "root"
//         and C --root=root when needed, like test_005_print), C: xmq --xmq FILE to-json
//   dom   every tracked .html, .htm and .xml file, the DOM of C (c_dom.mjs) trimmed like
//         C trims what it reads, C: xmq --html|--xml FILE to-json
// Every case is classified into OUTPUT/json_results.txt:
//   SAME          the same value
//   DIFF          a different value, see OUTPUT/json/*.c and *.js
// Skipped, with the reason:
//   SKIP_C_REJECTS         C cannot read the input.
//   SKIP_JS_REJECTS        javascript cannot parse the xmq into a W3C Document (see
//                          test_005_print).
//   SKIP_C_XML_UNREADABLE, SKIP_ENTITY_REF  no DOM for the file, see c_dom.mjs.
// The test fails on any DIFF that is not listed with the reason in
// tests/libxmq_js/json_expected_differences.txt (name|reason).
//   node tests/libxmq_js/test_008_json.mjs build/libxmq_js/package OUTPUT_DIR
// The xmq binary is $XMQ, default build/default/release/xmq.
import { execFileSync, spawnSync } from "node:child_process";
import { createRequire } from "node:module";
import fs from "node:fs";
import path from "node:path";
import { xmq_inputs } from "./inputs.mjs";
import { c_dom } from "./c_dom.mjs";

const require = createRequire(import.meta.url);
const pkg = path.resolve(process.argv[2]);
const output = path.join(path.resolve(process.argv[3]), "json");
const xmq = process.env.XMQ || "build/default/release/xmq";
const lib = require(path.join(pkg, "cjs/index.js"));
const xmldom = require("@xmldom/xmldom");
const dom = new xmldom.DOMImplementation();

/** Read the json text that C prints into a value, see the header. */
function read_c_json(text)
{
    let i = 0;
    const ws = () => { while (i < text.length && /[ \t\n\r]/.test(text[i])) i++; };
    const fail = (m) => { throw new Error("c json: " + m + " at " + i + ": " + JSON.stringify(text.substring(i, i + 30))); };
    function string()
    {
        if (text[i] !== "\"") fail("expected a string");
        i++;
        let s = "";
        while (i < text.length && text[i] !== "\"")
        {
            const c = text[i++];
            if (c !== "\\")
            {
                s += c;
                continue;
            }
            const e = text[i++];
            const simple = { "\\": "\\", "\"": "\"", "/": "/", a: "\u0007", b: "\b", t: "\t", n: "\n", v: "\u000b", f: "\f", r: "\r" };
            if (e in simple) s += simple[e];
            else if (e === "x")
            {
                s += String.fromCharCode(parseInt(text.substring(i, i + 2), 16));
                i += 2;
            }
            else if (e === "u")
            {
                s += String.fromCharCode(parseInt(text.substring(i, i + 4), 16));
                i += 4;
            }
            else fail("unknown escape \\" + e);
        }
        if (text[i] !== "\"") fail("string not closed");
        i++;
        return s;
    }
    // A value, or in a list (top or array) a "key":value pair as an object.
    function item(in_list)
    {
        ws();
        if (in_list && text[i] === "\"")
        {
            const save = i;
            const s = string();
            ws();
            if (text[i] === ":")
            {
                i++;
                return { [s]: value() };
            }
            i = save;
        }
        return value();
    }
    function value()
    {
        ws();
        const c = text[i];
        if (c === "{")
        {
            i++;
            const o = {};
            ws();
            if (text[i] === "}")
            {
                i++;
                return o;
            }
            for (;;)
            {
                ws();
                const k = string();
                ws();
                if (text[i++] !== ":") fail("expected :");
                o[k] = value();
                ws();
                if (text[i] === ",")
                {
                    i++;
                    continue;
                }
                if (text[i++] !== "}") fail("expected }");
                return o;
            }
        }
        if (c === "[")
        {
            i++;
            const a = [];
            ws();
            if (text[i] === "]")
            {
                i++;
                return a;
            }
            for (;;)
            {
                a.push(item(true));
                ws();
                if (text[i] === ",")
                {
                    i++;
                    continue;
                }
                if (text[i++] !== "]") fail("expected ]");
                return a;
            }
        }
        if (c === "\"") return string();
        const m = /^(true|false|null|[-+0-9.eE]+)/.exec(text.substring(i));
        if (m === null) fail("unexpected");
        i += m[0].length;
        if (m[0] === "true") return true;
        if (m[0] === "false") return false;
        if (m[0] === "null") return null;
        const n = Number(m[0]);
        return Number.isNaN(n) ? m[0] : n;
    }
    ws();
    if (i >= text.length) return undefined;
    const values = [];
    for (;;)
    {
        values.push(item(true));
        ws();
        if (i >= text.length) break;
        if (text[i++] !== ",") fail("expected , at the top");
    }
    return values.length === 1 ? values[0] : values;
}

// The xml parser for embedded xml in xmq.
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
const ed_file = "tests/libxmq_js/json_expected_differences.txt";
if (fs.existsSync(ed_file))
{
    for (const l of fs.readFileSync(ed_file, "utf8").split("\n"))
    {
        if (l === "" || l.startsWith("#")) continue;
        expected_diffs.add(l.split("|")[0]);
    }
}

fs.rmSync(output, { recursive: true, force: true });
fs.mkdirSync(output, { recursive: true });
const results = [];
const counts = {};
const failures = [];

/** Compare the value of the DOM with the C json of args, classify the case. */
function compare(name, doc, args, options)
{
    const c = spawnSync(xmq, args, { encoding: "utf8", timeout: 60000 });
    let cls;
    if (c.status !== 0) cls = "SKIP_C_REJECTS";
    else
    {
        let cv, js;
        try
        {
            cv = JSON.stringify(read_c_json(c.stdout));
        }
        catch (e)
        {
            cv = "C JSON UNREADABLE " + e.message;
        }
        try
        {
            js = JSON.stringify(lib.toJson(doc, options));
        }
        catch (e)
        {
            js = "JS EXCEPTION " + e.stack;
        }
        cls = cv === js ? "SAME" : "DIFF";
        if (cls === "DIFF")
        {
            const base = path.join(output, name.replace(/[\/]/g, "_"));
            fs.writeFileSync(base + ".c", c.stdout + "\n" + cv + "\n");
            fs.writeFileSync(base + ".js", js + "\n");
        }
    }
    record(name, cls);
}

function record(name, cls)
{
    counts[cls] = (counts[cls] ?? 0) + 1;
    results.push(cls + " " + name);
    if (cls === "DIFF" && !expected_diffs.has(name)) failures.push(name);
}

// The xmq inputs.
for (const [name, text] of xmq_inputs())
{
    const file = path.join(output, name.replace(/[\/]/g, "_") + ".xmq");
    fs.writeFileSync(file, text);
    let doc = null, root = false;
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
                doc = null;
            }
        }
    }
    if (doc === null)
    {
        const c = spawnSync(xmq, ["--xmq", file, "to-json"], { encoding: "utf8" });
        record(name, c.status !== 0 ? "SKIP_C_REJECTS" : "SKIP_JS_REJECTS");
        continue;
    }
    compare(name, doc, [...(root ? ["--root=root"] : []), "--xmq", file, "to-json"], {});
}

// The html and xml files.
for (const file of execFileSync("git", ["ls-files", "*.html", "*.htm", "*.xml"], { encoding: "utf8" }).split("\n").filter(f => f !== ""))
{
    const [skip, doc] = c_dom(xmq, file);
    if (skip !== null)
    {
        record(file, skip);
        continue;
    }
    compare(file, doc, [/\.html?$/.test(file) ? "--html" : "--xml", file, "to-json"], {});
}
fs.writeFileSync(path.join(path.dirname(output), "json_results.txt"), results.join("\n") + "\n");

const summary = Object.entries(counts).map(([k, v]) => k + " " + v).join(", ");
if (failures.length === 0)
{
    console.log("OK: json (" + summary + ")");
    process.exit(0);
}
console.log("ERR: json (" + summary + ") unexpected: " + failures.slice(0, 20).join(" ") + (failures.length > 20 ? " ..." : "") +
            "\nsee " + output + "/*.c and *.js");
process.exit(1);
