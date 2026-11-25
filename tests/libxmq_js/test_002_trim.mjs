// libxmq - Copyright (C) 2026 Fredrik Öhrström (spdx: MIT)
// The trimming of quotes and comments (trim.ts) against the C xmq itself: random quotes
// and comments (fixed seed) plus hand picked cases are put in one xmq document, and
// xmq tokenize --type=debugcontent must print the same content as the javascript trimming.
// Also the trimming of the text that C reads from xml (xmqTrimWhitespace, xmq_un_quote
// with is_xmq false): random texts in one xml document, C reads it with the default trim
// and prints it with to-xml, every text must be what the javascript trimming gives.
//   node tests/libxmq_js/test_002_trim.mjs build/libxmq_js/package OUTPUT_DIR
// The xmq binary is $XMQ, default build/default/release/xmq.
import { execFileSync } from "node:child_process";
import fs from "node:fs";
import path from "node:path";

const pkg = path.resolve(process.argv[2]);
const output = path.resolve(process.argv[3]);
const xmq = process.env.XMQ || "build/default/release/xmq";
const { trimQuote, findQuoteStartStop, unComment, unQuote } = await import(path.join(pkg, "esm/trim.js"));
const { createRequire } = await import("node:module");
const xmldom = createRequire(import.meta.url)("@xmldom/xmldom");
const { xmq_quote_as_c } = await import(path.join(pkg, "esm/text.js"));

// A small xorshift generator, so that a failure can be reproduced.
let seed = 4711;
function rnd(n)
{
    seed ^= seed << 13; seed >>>= 0;
    seed ^= seed >>> 17;
    seed ^= seed << 5; seed >>>= 0;
    return seed % n;
}
function body(alphabet, max)
{
    let s = "";
    const n = rnd(max);
    for (let i = 0; i < n; i++) s += alphabet[rnd(alphabet.length)];
    return s;
}

const quotes = [
    "", " ", "\n", "\n\n", "a", " a ", "\na\n", "\n  a\n  ", "  a\n     b\n  ", "a\n  b\n c",
    "\n\n   x\n    y\n\n", "a  \n  b  ", "   \n   ", "a\n\n\nb", " \n a", "a\n ", "x\n  y\n    ",
];
const comments = ["", " ", "a", " a ", "\n", "x\n   y", "\n  a\n  b\n", "a  \n   b   ", "  \n  "];
const line_comments = ["", " ", "a", " a ", "a  b  ", "   x"];
for (let i = 0; i < 2000; i++) quotes.push(body("ab \n", 30));
for (let i = 0; i < 500; i++) comments.push(body("ab \n", 30));
for (let i = 0; i < 300; i++) line_comments.push(body("ab ", 20));
// Also with CR and tab, a CR is not a newline but the trimming treats it specially.
for (let i = 0; i < 1000; i++) quotes.push(body("ab \n\r\t", 30));
for (let i = 0; i < 300; i++) comments.push(body("ab \n\r\t", 30));

// The document, and the expected debugcontent output computed by the javascript code
// from the same text (the same positions as the C tokenizer sees).
let doc = "r {\n";
let expected = "";
for (const q of quotes)
{
    doc += " ".repeat(rnd(9)) + "q = ";
    const start = doc.length;
    doc += "'" + q + "'";
    const [s, e] = findQuoteStartStop(doc, start, doc.length);
    expected += "{quote " + xmq_quote_as_c(trimQuote(doc, s, e, true, false), -1, -1, true) + "}";
    doc += "\n";
}
for (const c of comments)
{
    doc += " ".repeat(rnd(9));
    const start = doc.length;
    doc += "/*" + c + "*/";
    expected += "{comment " + xmq_quote_as_c(unComment(doc, start, doc.length), -1, -1, true) + "}";
    doc += "\n";
}
for (const c of line_comments)
{
    doc += " ".repeat(rnd(9));
    const start = doc.length;
    doc += "//" + c + "\n";
    expected += "{comment " + xmq_quote_as_c(unComment(doc, start, doc.length), -1, -1, true) + "}";
}
doc += "}\n";
expected += "\n";

fs.mkdirSync(output, { recursive: true });
const file = path.join(output, "trim.xmq");
fs.writeFileSync(file, doc);
const got = execFileSync(xmq, [file, "tokenize", "--type=debugcontent"], { encoding: "utf8" });
fs.writeFileSync(path.join(output, "trim.expected"), expected);
fs.writeFileSync(path.join(output, "trim.got"), got);

// The xml texts.
const texts = ["\t\n\r x", "\n\r x", "a\n\r b", " \n x\n  y", "\n\n", "  x  "];
for (let i = 0; i < 3000; i++) texts.push(body("ab \n\r\t", 30));
const esc = (t) => t.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/\r/g, "&#13;");
const xml_file = path.join(output, "trim.xml");
fs.writeFileSync(xml_file, "<r>" + texts.map(t => "<t>" + esc(t) + "</t>").join("") + "</r>\n");
const cxml = execFileSync(xmq, [xml_file, "to-xml"], { encoding: "utf8" });
const ts = new xmldom.DOMParser().parseFromString(cxml, "text/xml").getElementsByTagName("t");
const xml_diffs = [];
for (let i = 0; i < texts.length; i++)
{
    // C removes an all whitespace text and a text that becomes empty.
    const t = texts[i];
    const js = /^[ \t\n\r]*$/.test(t) ? "" : unQuote(t, 0, t.length, false, false);
    const c = ts[i].textContent;
    if (js !== c) xml_diffs.push(JSON.stringify(t) + " C: " + JSON.stringify(c) + " JS: " + JSON.stringify(js));
}

const n = quotes.length + comments.length + line_comments.length;
if (got === expected && xml_diffs.length === 0)
{
    console.log("OK: trim (" + n + " quotes and comments, and " + texts.length + " xml texts the same as " + xmq + ")");
    process.exit(0);
}
if (got === expected)
{
    console.log("ERR: trim of " + xml_diffs.length + " of " + texts.length + " xml texts differs from " + xmq + ", eg\n" + xml_diffs.slice(0, 5).join("\n"));
    process.exit(1);
}
// Show the first difference.
const ge = expected.split("}{"), gg = got.split("}{");
let k = 0;
while (k < ge.length && ge[k] === gg[k]) k++;
console.log("ERR: trim differs from " + xmq + " at item " + k + "\nexpected: " + ge[k] + "\n     got: " + gg[k] +
            "\nsee " + file);
process.exit(1);
