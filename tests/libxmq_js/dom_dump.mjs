// libxmq - Copyright (C) 2026 Fredrik Öhrström (spdx: MIT)
// A structural dump of a W3C DOM, to compare the DOM of libxmq for javascript with the DOM
// of the C xmq (its to-xml read by xmldom): names, namespaces, attributes in order, text
// with the entities as &name;, comments, pis, doctype name and ids, one line per node.
//   const lines = []; dump(doc, 0, lines, opts);
// The options make the two sides comparable where the xml of C, read by xmldom, cannot
// show what the DOM of javascript has:
//   opts.text(node)   the text of a text node, eg the character of a predefined entity
//                     that javascript keeps as the entity text &lt; (xmldom gives <)
//   opts.attr(attr)   the value of an attribute, likewise
//   opts.normalize    the ids of the doctype without the quotes that xmldom keeps, a CR
//                     in a comment or pi as a newline (xml line end normalization), and
//                     the data of a pi without leading whitespace (xml cannot show it).
//   opts.ns(element)  the namespace of an element to show, instead of its namespaceURI.
//   opts.fffd         the characters that xml 1.0 does not allow (eg U+0007) as U+FFFD, like
//                     the to-xml of C writes them (&#xFFFD;).

/** A structural dump of a node, adjacent text (and entity text) is merged. */
export function dump(n, indent, out, opts = {})
{
    const pad = "  ".repeat(indent);
    const fffd = (s) => opts.fffd ? s.replace(/[\u0000-\u0008\u000b\u000c\u000e-\u001f]/g, "\ufffd") : s;
    const norm = (s) => fffd(opts.normalize ? s.replace(/\r\n?/g, "\n") : s);
    const id = (s) => opts.normalize && /^(["']).*\1$/.test(s) ? s.substring(1, s.length - 1) : s;
    if (n.nodeType === 9 || n.nodeType === 11)
    {
        dump_children(n, indent, out, true, opts);
        return;
    }
    if (n.nodeType === 1)
    {
        const ns = opts.ns ? opts.ns(n) : n.namespaceURI;
        let line = pad + "E " + n.nodeName + (ns ? " {" + ns + "}" : "");
        for (let i = 0; i < n.attributes.length; i++)
        {
            const a = n.attributes[i];
            const v = fffd(opts.attr ? opts.attr(a) : a.value);
            line += " " + a.name + (a.namespaceURI && !a.name.startsWith("xmlns") ? "{" + a.namespaceURI + "}" : "") + "=" + JSON.stringify(v);
        }
        out.push(line);
        dump_children(n, indent + 1, out, false, opts);
        return;
    }
    // An xml comment cannot contain --, C prints -␐- (xmq_replace_transform_markers),
    // the DOM has --.
    if (n.nodeType === 8) out.push(pad + "C " + JSON.stringify(norm(n.data.replace(/-␐/g, "-"))));
    else if (n.nodeType === 7) out.push(pad + "P " + n.target + " " + JSON.stringify(norm(opts.normalize ? n.data.replace(/^[ \t\r\n]+/, "") : n.data)));
    else if (n.nodeType === 10) out.push(pad + "D " + n.name + " " + JSON.stringify(id(n.publicId || "")) + " " + JSON.stringify(id(n.systemId || "")));
    else out.push(pad + "? " + n.nodeType);
}

export function dump_children(n, indent, out, top, opts = {})
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
            let t = opts.text ? opts.text(c) : c.data;
            if (opts.fffd) t = t.replace(/[\u0000-\u0008\u000b\u000c\u000e-\u001f]/g, "\ufffd");
            text = (text ?? "") + t;
            continue;
        }
        flush();
        dump(c, indent, out, opts);
    }
    flush();
}
