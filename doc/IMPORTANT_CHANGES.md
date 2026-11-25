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

## IC-2 2026-11-25 Freeing and moving the nodes of a dom is no longer recursive

**Problem:** Freeing a dom and moving nodes to another document (which sets the document
of every moved node) walked the tree recursively, in C `xml_node_free_contents` and
`xml_node_set_dom_recursive` in `xml_dom.c`, in Java `XmlNode.setDocRecursive`. A deeply
nested tree overflowed the stack: Fil-C stopped `parts/testinternals` with a stack overflow
when the 50000 deep document of `test_xml_parser` was freed, moving a 1000000 deep tree
crashed the C version and moving a 200000 deep tree threw a `StackOverflowError` in Java.

**Solution:** The walks are iterative and use no call stack per level. Freeing keeps the
nodes still to free in a list linked with `next`, each node first moves its own children,
attributes and notations onto the list, then it is freed. Setting the document keeps the
first nodes of the sibling lists still to visit on a stack on the heap in C, and in an
`ArrayDeque` in Java (a leaf, eg an appended text, takes a fast path). Java now also sets
the document of the notations of a dtd, like C. `test_deep_dom` in `parts/testinternals.c`
and `TestInternals.java` moves and frees such deep trees.
