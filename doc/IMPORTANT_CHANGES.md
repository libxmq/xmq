# Important changes

Changes that are important to know about, eg a performance problem or a change of
behaviour. Each has a number IC-n, the newest last.

## IC-1 2026-11-25 The xml parser namespace lookup is no longer quadratic in the depth

**Problem:** For every element and every prefixed attribute the xml parser looked up the
namespace of the prefix by walking all the ancestors of the element. Parsing a deeply
nested document was therefore quadratic in the depth: the 50000 deep document in
`test_xml_parser` (`parts/testinternals`) took 9.5 s. It has been like this since the
parser was forked from libxml2 (952f5a08, 2026-11-14), in both C and Java.

**Solution:** The parser only visits the open elements that declare namespaces. The C
parser stores in each open element (`XmlOpen.ns_open`) the nearest enclosing open element
that declares namespaces, the Java parser keeps a stack of them (`ns_open_`). A lookup
(`xml_lookup_ns` in `xml_parser.c`, `lookup_ns` in `XmlParser.java`) checks the element
itself, then that chain from the innermost, then the ancestors below the open elements of
the chunk being parsed (the node that an entity replacement text is parsed into). The
result is exactly the same as before, only faster: `test_xml_parser` takes 0.015 s.
