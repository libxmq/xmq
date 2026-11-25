# libxmq for javascript

A small part of [libxmq](https://libxmq.org) in javascript, with typescript types, for
the browser and node:

- parse xmq into a W3C DOM document,
- print a DOM as xmq, also the DOM of the page that a browser loaded from html (not yet
  the compact form),
- convert a DOM to a javascript value with the json conventions of xmq.

It is a port of the Java implementation of libxmq and gives the same result as the
`xmq` command line, also the same error messages. It has no runtime dependencies.

```js
import { parse, print, toJson, XMQParseError } from "libxmq";

// In a browser the DOM of the browser is used.
const doc = parse("car { model = Volvo }");

// Node has no DOM of its own, pass a DOMImplementation, eg from @xmldom/xmldom.
import { DOMImplementation } from "@xmldom/xmldom";
const doc2 = parse("car { model = Volvo }", { dom: new DOMImplementation() });

// A Document has one root element: several elements need an implicit root element,
// like xmq --root=list: <list><a>1</a><b>2</b></list>
const doc3 = parse("a = 1 b = 2", { root: "list" });

// car {
//     model = Volvo
// }
console.log(print(doc));
// The to-xmq options of xmq: indent, escapeNewlines, escapeNon7bit, escapeTabs,
// preferDoubleQuotes.
console.log(print(doc, { indent: 2 }));

// In a browser: the page as xmq, trimmed like xmq page.html.
console.log(print(document));

// { _: "car", model: "Volvo" }, the value of the json of xmq to-json.
const car = toJson(doc);

try
{
    parse("a = (x)", { source: "my.xmq" });
}
catch (e)
{
    // my.xmq:1:6: error: compound may only contain quotes and entities
    // a = (x)
    //      ^
    if (e instanceof XMQParseError) console.log(e.message);
}
```

## Api

- `parse(xmq, options?)`: a new Document. Options: `dom` (the DOM implementation, the
  browser's by default), `source` (the name in error messages), `xmlParser` (for
  embedded xml), `root` (the implicit root element). Throws `XMQParseError` (with `code`,
  `line`, `column`, `source` and the message of the C xmq) or `XMQDomError`.
- `print(node, options?)`: the node as xmq, without a final newline. Options: `indent`,
  `escapeNewlines`, `escapeNon7bit`, `escapeTabs`, `preferDoubleQuotes`, `trim`.
- `toJson(node, options?)`: the node as a javascript value. Options: `trim`, `onWarning`.
- `xmqToJson(xmq, options?)`: `toJson(parse(xmq, options), options)`.
- `tokenize(xmq, "debugtokens" | "debugcontent", source?)`: the same text as `xmq
  tokenize --type=debugtokens` and `--type=debugcontent`.
- `entityName(node)`, `attrParts(attr)`, `doctypeText(doctype)`: what parse remembered
  that a W3C DOM cannot hold, see below.
- `ParseErrorCode`, `parseErrorToString(code)`: the error codes and texts of C.

The typescript types come with the package (es module and commonjs). They use the DOM
types of typescript (`"lib": ["dom"]` in tsconfig.json). Another DOM implementation,
eg @xmldom/xmldom, has types of its own: `dom` takes any object with `createDocument` and
`createDocumentType`, `parse` returns the document type of that implementation, and
`print` and `toJson` take any node (`DOMNodeLike`).

Printing trims the whitespace like the xmq command line does when it reads xml or html
(the default `--trim`): the text nodes with only whitespace are skipped and the incidental
indentation is removed from the text and the comments. The DOM is not changed. A document
that `parse` made is printed untrimmed, as C never trims xmq; the option `trim: true` or
`trim: false` decides it yourself.

`toJson` builds the javascript objects and arrays directly, with the conventions of `xmq
to-json`: the root element name is `_`, an attribute `x` is `_x`, repeated names get `[0]`,
`[1]`..., text between elements is `|`, a comment `//`, an array is an element with the
attribute `A`. Numbers, `true`, `false` and `null` become javascript values unless the
element has the attribute `S`. Where xmq prints something that is not one json value, the
value is: a comment, text or entity at the top or in an array is an object of its own,
eg `{ "//": "comment" }`, and several values at the top (eg a processing instruction
before the root element) are an array. As javascript objects: a key that looks like an
array index (eg `"12"`, possible through the `_` attribute) comes first in the object,
two equal keys (an element `_x` and an attribute `x`) keep the last value, and a large
integer loses precision like any javascript number.

## Differences from the C and Java libxmq

- A W3C DOM cannot store entity references. A named entity such as `&nbsp;` is kept as
  a text node of its own whose text is the entity, and the library remembers these nodes
  (`entityName(node)`), so printing writes them back as entities. Likewise the attribute
  values that contain entities or have no value (`attrParts(attr)`) and the full doctype
  (`doctypeText(doctype)`). Cloning the nodes or calling `normalize()` loses this, the
  nodes are then plain text.
- A W3C Document can only have one root element and no text outside of it, `parse`
  throws an `XMQDomError` for such xmq. Give the `root` option, an implicit root element
  like `--root` of the command line. As in C, the implicit root is only created when the
  first node is an element: after a leading comment or quote, or for xmq that is only a
  quote, it still throws `XMQDomError`.
- A namespace prefix that is never declared, eg `p:a` without `xmlns:p`, gives an element
  named `p:a` without a namespace.
- The doctype is not checked (C and Java report "doctype could not be parsed").
- Embedded xml (`<b>bold</b>` in xmq) is parsed with a DOMParser: the one of the browser,
  or the `xmlParser` option. Without one it is an error.
- An element can only have an attribute once in a W3C DOM, the last one wins:
  `a(x=1 x=2)` prints as `a(x = 2)`, C keeps and prints both.
- A W3C DOM has no internal subset of the doctype. The printer prints the declarations
  of the doctype text that parse read, each on its own line like C, but as they were
  written, not in the normalized form of libxml2.
- A browser builds another DOM from html than the html parser of C: it adds `head`,
  `body` and `tbody`, it gives `<input disabled>` an empty value (printed `disabled`, C
  prints `disabled = disabled`), and C adds a doctype when there is none. The printer
  prints the DOM it gets, the same as C prints the same DOM.
