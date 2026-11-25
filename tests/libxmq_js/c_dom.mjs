// libxmq - Copyright (C) 2026 Fredrik Öhrström (spdx: MIT)
// The DOM of the C xmq for an html or xml file, for the libxmq for javascript tests that
// compare a DOM that was not parsed from xmq: the untrimmed DOM of
// xmq --trim=none --html|--xml FILE to-xml is read with the xmldom DOMParser, and for html
// rebuilt as an html document like a browser builds it (the elements in the xhtml
// namespace with the nodeName in upper case, the attributes, also xmlns, without a
// namespace).
import { spawnSync } from "node:child_process";
import { createRequire } from "node:module";

const require = createRequire(import.meta.url);
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

/**
 * The C DOM of the html (.html, .htm) or xml file: [null, document], or [reason, null]:
 *   SKIP_C_REJECTS         C cannot read the file.
 *   SKIP_C_XML_UNREADABLE  xmldom cannot read the xml that C prints, eg an undeclared prefix.
 *   SKIP_ENTITY_REF        the xml has entity references that xmldom cannot resolve.
 */
export function c_dom(xmq, file)
{
    const html = /\.html?$/.test(file);
    const c = spawnSync(xmq, ["--trim=none", html ? "--html" : "--xml", file, "to-xml"], { encoding: "utf8", timeout: 60000 });
    if (c.status !== 0) return ["SKIP_C_REJECTS", null];
    try
    {
        const xdoc = read_xml(c.stdout.replace(/^<\?xml[^>]*\?>\n?/, ""));
        return [null, html ? to_html_document(xdoc) : fix_comments(xdoc)];
    }
    catch (e)
    {
        // xmldom wraps the error of onError in its own.
        return [/entity not found|EntityRef/.test(e.message) ? "SKIP_ENTITY_REF" : "SKIP_C_XML_UNREADABLE", null];
    }
}
