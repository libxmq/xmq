# libxmq for javascript

A small part of [libxmq](https://libxmq.org) in javascript, with typescript types, for
the browser and node:

- parse xmq into a W3C DOM document,
- print a DOM as xmq, also a DOM that the browser loaded from html,
- convert a DOM to json, as a javascript value or as text.

It is a port of the Java implementation of libxmq and gives the same output as the
`xmq` command line. It has no runtime dependencies. In node, which has no DOM of its own,
pass a DOMImplementation, eg from `@xmldom/xmldom` or `jsdom`.

This is work in progress: so far only the parse errors are exported.
