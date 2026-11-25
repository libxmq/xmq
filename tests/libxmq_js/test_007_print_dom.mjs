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

const require = createRequire(import.meta.url);
const pkg = path.resolve(process.argv[2]);
const output = path.join(path.resolve(process.argv[3]), "print_dom");
const xmq = process.env.XMQ || "build/default/release/xmq";
const lib = require(path.join(pkg, "cjs/index.js"));
const xmldom = require("@xmldom/xmldom");
const impl = new xmldom.DOMImplementation();
const XHTML_NS = "http://www.w3.org/1999/xhtml";

/** Read the xml that C printed, strictly. */
function read_xml(xml)
{
    const parser = new xmldom.DOMParser({
        onError: (level, msg) =>
        {
            if (level === "warning") return;
            throw new Error(msg);
        },
    });
    return parser.parseFromString(xml, "text/xml");
}

/** Make the element look like one of a browser: the nodeName and tagName in upper case. */
function browser_names(e)
{
    const upper = e.nodeName.toUpperCase();
    Object.defineProperty(e, "nodeName", { value: upper });
    Object.defineProperty(e, "tagName", { value: upper });
}

/** Rebuild the xml DOM as an html document, like a browser has it. */
function to_html_document(xdoc)
{
    const hdoc = impl.createHTMLDocument("");
    while (hdoc.firstChild !== null) hdoc.removeChild(hdoc.firstChild);
    const stack = [[xdoc, hdoc]];
    while (stack.length > 0)
    {
        const [x, h] = stack.pop();
        for (let c = x.firstChild; c !== null; c = c.nextSibling)
        {
            let n;
            switch (c.nodeType)
            {
            case 1:
                n = hdoc.createElement(c.nodeName);
                if (n.namespaceURI !== XHTML_NS) throw new Error("not an html element: " + c.nodeName);
                browser_names(n);
                for (let i = 0; i < c.attributes.length; i++) n.setAttribute(c.attributes[i].name, c.attributes[i].value);
                stack.push([c, n]);
                break;
            case 3:
            case 4:
                n = hdoc.createTextNode(c.data);
                break;
            case 8:
                // C escapes -- in an xml comment as -␐- and reads it back as --.
                n = hdoc.createComment(c.data.replace(/-␐/g, "-"));
                break;
            case 7:
                n = hdoc.createProcessingInstruction(c.target, c.data);
                break;
            case 10:
                n = impl.createDocumentType(c.name, c.publicId, c.systemId);
                break;
            default:
                throw new Error("unexpected node type " + c.nodeType);
            }
            h.appendChild(n);
        }
    }
    return hdoc;
}

/** Undo the C escape of -- in the comments of an xml DOM. */
function fix_comments(doc)
{
    const stack = [doc];
    while (stack.length > 0)
    {
        const n = stack.pop();
        for (let c = n.firstChild; c !== null; c = c.nextSibling)
        {
            if (c.nodeType === 8) c.data = c.data.replace(/-␐/g, "-");
            else if (c.nodeType === 1) stack.push(c);
        }
    }
    return doc;
}

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
    const cxml = run(["--trim=none", type, file, "to-xml"]);
    let cls, detail = [];
    if (cxml.status !== 0)
    {
        cls = "SKIP_C_REJECTS";
    }
    else
    {
        let doc = null;
        try
        {
            const xdoc = read_xml(cxml.stdout.replace(/^<\?xml[^>]*\?>\n?/, ""));
            doc = html ? to_html_document(xdoc) : fix_comments(xdoc);
            cls = null;
        }
        catch (e)
        {
            // xmldom wraps the error of onError in its own.
            cls = /entity not found|EntityRef/.test(e.message) ? "SKIP_ENTITY_REF" : "SKIP_C_XML_UNREADABLE";
        }
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
