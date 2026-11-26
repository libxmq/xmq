# TODO

Known bugs, limitations and deferred work, each with a minimal repro.

## xmq --xmq fails silently on input that looks like xml or json

`printf '<b>x</b>\n' | xmq --xmq - to-xml` exits with 1 and prints nothing. It should
parse the input as xmq (an embedded xml element) or print why it cannot. The same
happens with the START section of `tests/format_049_json.test`. On that input Java
(`xmqj --xmq`) prints the xmq error `3:16: error: unexpected character ":"`. The javascript
test `tests/libxmq_js/test_003_parse.mjs` skips 44 such inputs (SKIP_C_NO_MESSAGE), since
it has no C result to compare with.

## The implicit root is lost after a comment or a quote

With `--root=NAME` (and the `root` option of libxmq for javascript), the implicit root
element is created only if the first node of the xmq is an element with another name. The
parser checks `element_last == NULL` in `create_node`, and a comment or a quote before
the first element sets it. So a leading comment, eg a license header, turns the implicit
root off for the whole file. If the first element is named NAME, the elements after it
stay at the top level. Xmq with no element at all gets no root. This is the same in C and
in the Java parser:

```sh
printf "a = 1 b = 2\n" | xmq --root=list --xmq - to-xml            # <list><a>1</a><b>2</b></list>
printf "// c\na = 1 b = 2\n" | xmq --root=list --xmq - to-xml      # <!--c--> <a>1</a> <b>2</b>
printf "'x' a = 1 b = 2\n" | xmq --root=list --xmq - to-xml       # x <a>1</a> <b>2</b>
printf "list { x = 1 } b = 2\n" | xmq --root=list --xmq - to-xml  # two root elements
printf "'hello'\n" | xmq --root=list --xmq - to-xml               # hello, no root element
```

In javascript, `parse(xmq, { root })` then throws `XMQDomError`, because a W3C Document
cannot hold text at the top level or two root elements. This affects 36 of the inputs of
`tests/libxmq_js/test_003_parse.mjs` (ROOT_NOT_ENOUGH), mostly files with only quotes or
entities, eg `fuzz_testcases/xmq/004_quote.xmq`. Should comments, pis and a doctype before
the first element stay outside the implicit root, while all other content (quotes,
entities, the later elements) goes inside it? Then C, Java and javascript have to change
together. Also, `xmqj --root=NAME` prints `the global option --root=list is not implemented in
xmqj yet`.

## A duplicate attribute in xmq gives xml that is not well-formed

`printf 'a(x=1 x=2)\n' | xmq --xmq - to-xml` prints `<a x="1" x="2"/>`, an xml parser
rejects it. Java (`xmqj`) does the same. The xmq parser should reject the duplicate
attribute or decide which one is kept. In javascript the W3C DOM keeps only the last one,
`a(x = 2)`. `web/resources/syntactic_sugar.xmq` has one
(`element(value = 123 value = 123)`); it is an expected difference of
`tests/libxmq_js/test_005_print.mjs`.

## Investigate an iterative xmq parser and printer in C

The C xmq parser recurses once per nested body (`parse_xmq_element_internal` in
`src/main/c/parts/xmq_parser.c`, limited by `XMQ_MAX_NESTING_DEPTH`, 10000). The xmq
printer recurses through `print_nodes` and `print_element_with_children`
(`src/main/c/parts/xmq_printer.c`), as does the json printer
(`json_print_element_with_children` in `src/main/c/parts/json.c`). On a normal 8 MB stack
this works for 10000 levels. A smaller stack does not: a thread, an embedded system, or
the wasm build. `build/xmqjs` runs in a worker thread with a 64 MB stack for this reason.
The Java CLI runs in a thread with a 1 GB stack (`Main.java`).

libxmq for javascript has no recursion: the parse of a body is a loop with a counter of
open bodies (`close_body` in `src/main/ecmascript/src/parser.ts`), and `print_nodes` in
`printer.ts` keeps its own stack of the elements being printed. Investigate whether C
(and then Java, they must stay identical) should do the same:

- how much stack C uses per level today, for parse and print: measure with
  `ulimit -s` and 10000 nested `a{`,
- whether the same loops fit the C code without changing the output or the error
  messages (`make test`, `make testcj`),
- whether the wasm build could then use the normal stack and drop the worker thread.

`tests/test_max_depth_parsers.sh` and `tests/libxmq_js/test_004_depth.mjs` test the depth
limit.

## C aligns the = of names with non-ascii characters wrongly

```sh
printf '<r><PSČ>1</PSČ><abcde>2</abcde></r>\n' > a.xml
xmq a.xml to-xmq     # PSČ  = 1 and abcde = 2, the = signs are not aligned
xmqj a.xml to-xmq    # PSČ   = 1, aligned
```

`print_utf8_internal` in `src/main/c/parts/utf8.c` finds the end of a utf8 character with
`while (j < stop && (*j & 0xc0) == 0x80) j++;`. A name is printed with `stop` NULL (a NUL
terminated string), then `j < stop` is false, every byte counts as a character and
`current_indent` grows by the number of bytes. `find_element_key_max_width` counts the
characters, so the padding is too small by the number of extra utf8 bytes. Java and
libxmq for javascript count characters. `tests/libxml2/test/valid/objednavka.xml` shows it,
it is an expected difference of `tests/libxmq_js/test_007_print_dom.mjs`.

## A default namespace is also used by the following siblings

`xmlns=uri` on an element puts the element and its children in the namespace, but the
parser also keeps it for the elements that follow the element. C, Java and javascript
alike: `default_namespace` of the parse state is never reset when the element ends.

```sh
printf 'r { a(xmlns=urn:x) = 1 b = 2 }\n' > ns.xmq
xmq ns.xmq select '//*[namespace-uri()="urn:x"]' to-xml   # selects a and b
xmq ns.xmq to-xml      # <r><a xmlns="urn:x">1</a><b>2</b></r>, b is shown without it
```

The xml that C prints loses the namespace of `b` (it is not declared where `b` is), so the
DOM and its xml disagree. `make fuzz_parse_print` counts these as KNOWN_DEFAULT_NS_LEAK.

## A processing instruction with a compound value is parsed wrongly

`?pi = ( 'a' )` gives the parts of the compound to whatever node was created last, not
to the pi, and the pi gets the value of the next key = value:

```sh
printf "r { &apos; ?pi = ( 'a' ) x = 1 }\n" | xmq --xmq - to-xml
# <r>&apos;<x/><?pi 1?></r>, the 'a' is a child of the entity reference &apos;
```

The same in Java and javascript (Java threw a ClassCastException when it printed such an
entity reference, fixed). The printer writes `?pi = ( ... )` for a pi whose text needs a
compound, so such a pi does not survive print and parse again.

## Printing a comment that ends with a newline is not stable

A comment whose text ends with a newline is printed as `/* text */`, which loses the
newline, and the next print gives another comment:

```sh
printf '/* a\n\n*/\na = 1\n' > c.xmq   # the comment text is "a\n"
xmq --xmq c.xmq to-xmq                  # /* a */
xmq --xmq c.xmq to-xmq | xmq --xmq - to-xmq   # // a
```

Most of the ROUNDTRIP findings of `make fuzz_parse_print` (print, parse and print again
gives another text) are comments with leading or ending whitespace and newlines; the
printer of Java and javascript does the same as C.

## libxmq for javascript: limitations

- **The doctype is not checked.** C and Java report `doctype could not be parsed`, the
  javascript library takes the doctype text as it is.
- **Embedded xml needs a DOMParser.** Embedded xml (`<b>bold</b>`) is parsed with the
  browser's DOMParser, or with the `xmlParser` option. Without one it is an error.
- **Undeclared namespace prefixes.** `p:a` without an `xmlns:p` declaration gives an
  element named `p:a` without a namespace, since a W3C DOM cannot store a namespace
  without a uri.
- **The entity marks are fragile.** Entities are kept as text nodes, and the library
  remembers them. Cloning the nodes, or `normalize()`, loses the marks, and the nodes
  become plain text.
- **The internal subset of a doctype is printed as written.** A W3C DOM has no internal
  subset, the printer takes the declarations from the doctype text that parse read, one
  per line like C, but not normalized like libxml2 does (`<!ENTITY  a   "b">` prints as
  it is, C prints `<!ENTITY a "b">`).
- **A DOM from a browser's html parser differs from the one of C.** Printing a browser page
  gives what `xmq page.html` gives for the same DOM, but the html parsers differ: a browser
  adds `head`, `body` and `tbody`, C adds a doctype (HTML 4.0 Transitional) when there is
  none, lowercases the svg names (`foreignobject`), and gives a boolean attribute its name
  as value (`<input disabled>` is `disabled = disabled` in C, `disabled` from a browser).
- **Not implemented yet:** an `expandEntities` like the one of C. It is planned, the marks
  of the entity text nodes and the attribute parts are what it needs.
