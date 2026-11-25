// libxmq - Copyright (C) 2026 Fredrik Öhrström (spdx: MIT)
// Parse xmq into a W3C DOM (@xmldom/xmldom) with libxmq for javascript and compare the
// DOM with the one of the C xmq: xmq --xmq FILE to-xml is parsed with the xmldom
// DOMParser, and both DOMs are compared as a structural dump (names, namespaces,
// attributes in order, text with the entities as &name;, comments, pis, doctype name).
// When C rejects the input, javascript must reject it with the same message.
//
// The inputs are those of inputs.mjs: the xmq files and the START sections of the tests,
// also those that are not xmq (then both reject them). Every case is classified into OUTPUT/parse_results.txt:
//   SAME          the same DOM
//   REJECT_SAME   both reject it with the same message
//   ROOT_SAME     more than one root element or text outside of it: a W3C Document cannot
//                 hold it and parse throws XMQDomError, but with the implicit root option
//                 root: "root" it gives the same DOM as xmq --root=root --xmq FILE to-xml
//   ROOT_NOT_ENOUGH  parse throws XMQDomError also with the root option: the implicit root is
//                 only created when the first node is an element (not after a comment or a
//                 quote, not for xmq without elements), and not around the elements after
//                 a first element named root. C does the same, it prints the same top level
//                 nodes with --root=root, so this is not a difference (see doc/TODO.md).
//   DIFF, REJECT_DIFF, ONLY_C_REJECTS, ONLY_JS_REJECTS
// Skipped, with the reason:
//   SKIP_C_NO_MESSAGE  C fails without a message: xmq --xmq on input that C detects as
//                      xml or json exits with 1 and prints nothing, there is nothing to
//                      compare with (a C bug, the message expected xmq source is missing).
//   SKIP_C_XML_UNREADABLE  the xml that C prints cannot be read by the xmldom DOMParser,
//                      eg a namespace prefix that is never declared, so there is no C DOM.
// The test fails on any case that is not SAME, REJECT_SAME, ROOT_SAME, ROOT_NOT_ENOUGH or skipped,
// unless it is listed with the reason in tests/libxmq_js/parse_expected_differences.txt
// (name|class|reason).
//   node tests/libxmq_js/test_003_parse.mjs build/libxmq_js/package OUTPUT_DIR
// The xmq binary is $XMQ, default build/default/release/xmq.
import { spawnSync } from "node:child_process";
import { createRequire } from "node:module";
import fs from "node:fs";
import path from "node:path";
import { xmq_inputs } from "./inputs.mjs";

const require = createRequire(import.meta.url);
const pkg = path.resolve(process.argv[2]);
const output = path.join(path.resolve(process.argv[3]), "parse");
const xmq = process.env.XMQ || "build/default/release/xmq";
const lib = require(path.join(pkg, "cjs/index.js"));
const xmldom = require("@xmldom/xmldom");
const dom = new xmldom.DOMImplementation();

// The xml parser for embedded xml in xmq and for the C output: unknown entities are kept
// as the text &name; (xmldom reports them as errors, they are not errors here).
function xml_parser(strict)
{
    return new xmldom.DOMParser({
        onError: (level, msg) =>
        {
            if (level === "warning") return;
            if (/entity not found/.test(msg)) return;
            if (strict) throw new Error(msg);
        },
    });
}

/** A structural dump of a node, adjacent text (and entity text) is merged. */
function dump(n, indent, out)
{
    const pad = "  ".repeat(indent);
    if (n.nodeType === 9 || n.nodeType === 11)
    {
        dump_children(n, indent, out, true);
        return;
    }
    if (n.nodeType === 1)
    {
        let line = pad + "E " + n.nodeName + (n.namespaceURI ? " {" + n.namespaceURI + "}" : "");
        for (let i = 0; i < n.attributes.length; i++)
        {
            const a = n.attributes[i];
            line += " " + a.name + (a.namespaceURI && !a.name.startsWith("xmlns") ? "{" + a.namespaceURI + "}" : "") + "=" + JSON.stringify(a.value);
        }
        out.push(line);
        dump_children(n, indent + 1, out, false);
        return;
    }
    // An xml comment cannot contain --, C prints -\u2410- (xmq_replace_transform_markers),
    // the DOM has --.
    if (n.nodeType === 8) out.push(pad + "C " + JSON.stringify(n.data.replace(/-\u2410/g, "-")));
    else if (n.nodeType === 7) out.push(pad + "P " + n.target + " " + JSON.stringify(n.data));
    else if (n.nodeType === 10) out.push(pad + "D " + n.name + " " + JSON.stringify(n.publicId || "") + " " + JSON.stringify(n.systemId || ""));
    else out.push(pad + "? " + n.nodeType);
}

function dump_children(n, indent, out, top)
{
    let text = null;
    const flush = () =>
    {
        // Whitespace between the top level nodes is how C prints them, not content. An
        // empty text node (the quote '') cannot be seen in xml.
        if (text !== null && text !== "" && !(top && /^\s*$/.test(text))) out.push("  ".repeat(indent) + "T " + JSON.stringify(text));
        text = null;
    };
    for (let c = n.firstChild; c !== null; c = c.nextSibling)
    {
        if (c.nodeType === 3 || c.nodeType === 4)
        {
            text = (text ?? "") + c.data;
            continue;
        }
        flush();
        dump(c, indent, out);
    }
    flush();
}

const expected_diffs = new Map();
const ed_file = "tests/libxmq_js/parse_expected_differences.txt";
if (fs.existsSync(ed_file))
{
    for (const l of fs.readFileSync(ed_file, "utf8").split("\n"))
    {
        if (l === "" || l.startsWith("#")) continue;
        const [name, cls] = l.split("|");
        expected_diffs.set(name, cls);
    }
}

fs.rmSync(output, { recursive: true, force: true });
fs.mkdirSync(output, { recursive: true });
const results = [];
const counts = {};
const failures = [];
for (const [name, text] of xmq_inputs())
{
    const file = path.join(output, name.replace(/[\/]/g, "_") + ".xmq");
    fs.writeFileSync(file, text);
    let c = spawnSync(xmq, ["--xmq", file, "to-xml"], { encoding: "utf8" });
    let js = null, js_error = null, root = false, not_enough = false;
    try
    {
        js = lib.parse(text, { dom, source: file, xmlParser: xml_parser(true) });
    }
    catch (e)
    {
        if (e instanceof lib.XMQDomError)
        {
            root = true;
            c = spawnSync(xmq, ["--root=root", "--xmq", file, "to-xml"], { encoding: "utf8" });
            try
            {
                js = lib.parse(text, { dom, source: file, xmlParser: xml_parser(true), root: "root" });
            }
            catch (e2)
            {
                if (!(e2 instanceof lib.XMQDomError)) throw e2;
                not_enough = true;
            }
        }
        else js_error = e;
    }
    let cls, detail = "";
    if (c.status !== 0 && c.stderr === "")
    {
        cls = "SKIP_C_NO_MESSAGE";
    }
    else if (c.status !== 0)
    {
        const cmsg = c.stderr.replace(/\n+$/, "");
        if (js_error instanceof lib.XMQParseError)
        {
            cls = js_error.message === cmsg ? "REJECT_SAME" : "REJECT_DIFF";
            if (cls !== "REJECT_SAME") detail = "C:\n" + cmsg + "\nJS:\n" + js_error.message;
        }
        else
        {
            cls = "ONLY_C_REJECTS";
            detail = "C:\n" + cmsg + (js_error ? "\nJS: " + js_error.message : "");
        }
    }
    else if (js_error !== null)
    {
        cls = "ONLY_JS_REJECTS";
        detail = "JS:\n" + js_error.message + (js_error.stack && !(js_error instanceof lib.XMQParseError) ? "\n" + js_error.stack : "");
    }
    else
    {
        // The C xml without its xml declaration.
        const xml = c.stdout.replace(/^<\?xml[^>]*\?>\n?/, "");
        let cdoc;
        try
        {
            cdoc = xml_parser(true).parseFromString(xml, "text/xml");
        }
        catch (e)
        {
            cdoc = null;
        }
        if (not_enough)
        {
            // C has the same nodes at the top level, so its xml is not a document either.
            cls = cdoc === null ? "ROOT_NOT_ENOUGH" : "DIFF";
            if (cdoc !== null) detail = "C gives a single root with --root=root, JS throws XMQDomError";
        }
        else if (cdoc === null)
        {
            cls = "SKIP_C_XML_UNREADABLE";
        }
        else
        {
            const a = [], b = [];
            dump(cdoc, 0, a);
            dump(js, 0, b);
            const same = a.join("\n") === b.join("\n");
            cls = same ? (root ? "ROOT_SAME" : "SAME") : "DIFF";
            if (!same) detail = "C:\n" + a.join("\n") + "\nJS:\n" + b.join("\n");
        }
    }
    counts[cls] = (counts[cls] ?? 0) + 1;
    results.push(cls + " " + name);
    const ok = cls === "SAME" || cls === "REJECT_SAME" || cls === "ROOT_SAME" || cls === "ROOT_NOT_ENOUGH" || cls.startsWith("SKIP_") ||
        expected_diffs.get(name) === cls;
    if (!ok)
    {
        failures.push(name);
        fs.writeFileSync(path.join(output, name.replace(/[\/]/g, "_") + ".diff"), detail + "\n");
    }
}
fs.writeFileSync(path.join(path.dirname(output), "parse_results.txt"), results.join("\n") + "\n");

const summary = Object.entries(counts).map(([k, v]) => k + " " + v).join(", ");
if (failures.length === 0)
{
    console.log("OK: parse (" + summary + ")");
    process.exit(0);
}
console.log("ERR: parse (" + summary + ") unexpected: " + failures.slice(0, 20).join(" ") + (failures.length > 20 ? " ..." : "") +
            "\nsee " + output + "/*.diff");
process.exit(1);
