# TODO

Known bugs, limitations and deferred work, each with a minimal repro.

## The Java xmq parser differs from C

The Java parser (`src/main/java/org/libxmq/imp/XMQParser.java`) accepts some invalid xmq
that C rejects, and reports some errors at other positions. `make testj` does not run the
`tests/error_*.test` files, so this is not caught. libxmq for javascript (a port of the
Java parser) has the C behaviour, see `src/main/ecmascript/src/parser.ts`. Each item
below lists the error test that shows it. The Java parser needs the same fixes. Then
`make testj` should run the error tests.

- **Error positions.** C reports the start of the quote, entity or comment, Java reports
  where the parser stopped.
  `tests/error_003_too_many_closing_quotes.test`: C `2:17`, Java `2:21`.
  `tests/error_004_entity_not_closed.test` (`hello = &nbsp`): C `1:9`, Java `1:14`.
- **Unexpected closing brace.** `printf 'a = 1 }\n'`: C gives `error: unexpected
  closing brace`, Java accepts it (`<a>1</a>`). See
  `tests/error_010_too_many_closing_braces.test`.
- **Expected content after equals.** A `key =` followed by a new element: C gives `3:8:
  error: expected content after equals`, Java `4:8: error: unexpected character "{"`.
  See `tests/error_008_expected_content_after_equals.test`.
- **The more quotes warning.** `tale = 'There's a candle, a room and a spirit.'`: C first
  prints `1:14: warning: perhaps you need more quotes to quote this quote`, and then the
  error. Java prints only `1:25: error: unexpected character ","`. See
  `tests/error_013_more_quotes_needed.test`.
- **When `!` starts a doctype.** `printf '!x = 1\n'`: C gives `error: unexpected
  character "!"`, Java prints `<!x>1</!x>`. C treats `!DOCTYPE` as a doctype only when
  `=` or whitespace follows it (`is_xmq_doctype_start`), and `?` as a pi only when a name
  follows it (`is_xmq_pi_start`).

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
- **Not implemented yet:** an `expandEntities` like the one of C. It is planned, the marks
  of the entity text nodes and the attribute parts are what it needs.
