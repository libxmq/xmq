# Decisions

Design decisions that have been made, so that they are not reopened.

## The javascript/typescript api (libxmq for javascript)

A small subset of libxmq in javascript: parse xmq into a W3C DOM, pretty print a DOM as
xmq (not the compact form yet) and convert a DOM to json.

- **Typescript source.** The library is written in typescript and compiled with tsc into
  javascript (esm and commonjs) and .d.ts files, so the types always match the code.
- **The caller passes the DOM.** Node has no DOM: parse takes an optional
  DOMImplementation (eg from @xmldom/xmldom or jsdom). In a browser
  document.implementation is the default. The library has no runtime dependencies; the
  tests use @xmldom/xmldom.
- **Entities are text nodes.** xmq preserves entities when a file is loaded, but a W3C
  DOM cannot store entity references. Like C, numeric character references become
  characters, and every named entity (also the predefined ones such as `&amp;`) is kept:
  as a text node of its own whose data is the entity, eg `&nbsp;`. The library remembers
  these nodes in a WeakSet, so the printer writes them back as entities and the json
  conversion resolves them like C, while a quote `'&nbsp;'` stays a quote. The mark is
  lost when the caller clones the nodes or calls normalize(); they are then plain text.
- **Ported from Java.** The parser, printer and json conversion are ported from the Java
  implementation (closest in structure, and identical in output to C), with a comment
  naming the Java method on every function. C stays the reference for the differential
  tests.
- **An implicit root for several root elements.** A W3C Document has one root element
  and no text outside of it. For xmq with several elements at the top level, the caller
  gives the implicit root element (the `root` option of parse, like `--root` of the
  command line), and it is used as the root. Without it such xmq throws `XMQDomError`.
  There is no parse into a DocumentFragment.
- **No recursion over the nesting levels.** The parser (`parse_xmq` in `parser.ts`), the
  DOM building (`to_w3c` in `dom.ts`) and the printer (`print_nodes` in `printer.ts`)
  loop instead of recursing once per nested element as Java and C do. The reason: the
  mechanical port overflowed the stack of node at about 2500 levels, with a
  `RangeError` instead of an xmq error. A browser's stack cannot be changed and differs
  between engines, so a recursive library would fail at an engine dependent depth. The
  iterative code is kept small: the parser needs only a counter of open bodies (after a
  body Java does nothing but parse the `}`, now `close_body`), the printer a stack of
  frames, the DOM building a worklist. So the javascript port accepts the same 10000
  levels as C and Java, with the same error at 10001. The cost: these three places do
  not read line for line like Java. Each has a comment naming the Java code it replaces;
  when porting a later change of that Java or C code, code that runs after the recursive
  call belongs in `close_body` or where the printer pops a frame. `test_004_depth.mjs`
  and the parse and print comparisons with C guard it.
- **A DOM that was not parsed from xmq is printed trimmed.** C trims the xml and html it
  reads (whitespace only text removed, the incidental indentation of text and comments
  removed), never xmq. print does the same: it trims unless the document was made by
  parse, and the option trim overrides it. A browser page then prints like `xmq
  page.html`. The DOM is never changed, the printer skips and trims while printing.
- **The DOM is printed as it is.** The printer does not emulate the html parser of C, eg
  a browser's `disabled=""` prints as `disabled`, not as C's `disabled = disabled`. Such
  differences come from the parsers and are listed as known differences.
