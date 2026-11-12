# Nix configuration grammar

From the repository root, after `./configure && make`:

```sh
./build/default/release/xmq library/conf/nix.ixml configuration.nix transform library/conf/nix.xslq
python3 library/conf/test-nix/test.py
```

`configuration.nix` in this directory is the supplied regression fixture.
The grammar parses it without ixml ambiguity. The transform produces a `nix`
root containing module parameters and nested sets of `option(name=...)` nodes.
Values retain their types (`string`, `number`, `boolean`, `null`, `path`,
`reference`, `list`, `set`). `with` retains its scope and expression. Dotted
option names stay dotted; nested attribute sets stay nested. No Nix evaluation,
import loading, option validation, or merging is performed.

Comments remain `comment` elements, in source order, including comments inside
lists. This safely preserves comment text containing `--`, which cannot be
stored directly in XML comments. Formatting whitespace and delimiters are
omitted. String escapes remain explicit `escape` children with their original
spelling; ordinary string text is combined by the transform. Quoted attribute
names are supported and stored as option names rather than XML element names.
The dotted display name does not distinguish a quoted dot from a path separator;
the untransformed `attrpath/attribute` nodes preserve segment boundaries.

## Scope

This is a configuration-oriented subset, not a complete Nix language grammar.
It supports plain sets and module functions with simple named parameters,
optional trailing commas and ellipsis; assignments; dotted/quoted attribute
names; nested sets and lists; ordinary double-quoted strings and escapes;
integers and decimals; booleans/null; dotted references; simple absolute and
relative paths; `with` a reference scope; parentheses; line and block comments.
Line comments currently require a terminating newline (LF, CRLF, or CR).

Not implemented: parameter defaults or `@` patterns, general function
application/lambdas, `let`, `inherit`, `rec`, conditionals, operators, indented
strings, dynamic attributes, search paths, URIs, or interpolation expressions.
`${...}` inside a string is retained as literal text if it contains no unescaped
quotes; it is not parsed as an expression. This parser is not a Nix validator:
reserved names and some broader expression placements are not checked.

Regression checks verify important values and structure in the supplied file,
comment retention, empty containers, numeric and escaped-string values, paths,
parentheses, unambiguous parsing, and rejection of missing delimiters/values.
