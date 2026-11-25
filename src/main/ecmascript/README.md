# libxmq for javascript

A small part of [libxmq](https://libxmq.org) in javascript, with typescript types, for
the browser and node:

- parse xmq into a W3C DOM document,
- print a DOM as xmq, also a DOM that the browser loaded from html (not yet),
- convert a DOM to json, as a javascript value or as text (not yet).

It is a port of the Java implementation of libxmq and gives the same result as the
`xmq` command line, also the same error messages. It has no runtime dependencies.

```js
import { parse, parseFragment, XMQParseError } from "libxmq";

// In a browser the DOM of the browser is used.
const doc = parse("car { model = Volvo }");

// Node has no DOM of its own, pass a DOMImplementation, eg from @xmldom/xmldom.
import { DOMImplementation } from "@xmldom/xmldom";
const doc2 = parse("car { model = Volvo }", { dom: new DOMImplementation() });

// A Document has one root element, a fragment can hold any xmq content.
const frag = parseFragment("a = 1 b = 2 'some text'", { document: doc });

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

`tokenize(xmq, "debugtokens" | "debugcontent")` gives the same output as `xmq
tokenize --type=debugtokens` and `--type=debugcontent`.

## Differences from the C and Java libxmq

- A W3C DOM cannot store entity references. A named entity such as `&nbsp;` is kept as
  a text node of its own whose text is the entity, and the library remembers these nodes
  (`entityName(node)`), so printing writes them back as entities. Likewise the attribute
  values that contain entities or have no value (`attrParts(attr)`) and the full doctype
  (`doctypeText(doctype)`). Cloning the nodes or calling `normalize()` loses this, the
  nodes are then plain text.
- A W3C Document can only have one root element and no text outside of it, `parse`
  throws an `XMQDomError` for such xmq, use `parseFragment`.
- A namespace prefix that is never declared, eg `p:a` without `xmlns:p`, gives an element
  named `p:a` without a namespace.
- The doctype is not checked (C and Java report "doctype could not be parsed").
- Embedded xml (`<b>bold</b>` in xmq) is parsed with a DOMParser: the one of the browser,
  or the `xmlParser` option. Without one it is an error.
- The `--root` option of the command line (an implicit root element) is not supported.
