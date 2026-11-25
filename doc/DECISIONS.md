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
