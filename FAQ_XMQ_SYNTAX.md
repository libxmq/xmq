# FAQ: the XMQ syntax

XMQ is XML (and HTMQ is HTML) written with braces instead of closing tags, parentheses
around the attributes and every intended whitespace quoted. It maps one to one to the XML
DOM, so any XML/HTML can be converted to XMQ and back. The `xmq` tool converts between
XMQ, HTMQ, XML, HTML, JSON and YAML. This FAQ is written for humans and for LLM agents
that read or generate XMQ. The formal specification is in `doc/xmq.tex`, the guided tour
in `web/index.htmq` and the exact behaviour in the tests (`tests/*.test`).

## The syntax in short

```
!DOCTYPE = html                      // a doctype, before the first element
?xml-stylesheet = 'type="text/css" href="s.css"'   // a processing instruction ?target = data
// an element with attributes, hidden has no value, then its body
config(id = 7 hidden)
{
    name   = Restore                 // key = value is <name>Restore</name>
    cron   = '30 08 10 06 *'         // a value with whitespace must be quoted
    empty  = ''                      // two single quotes is the empty string
    quote  = "It's"                  // double quotes mirror single quotes
    code   = '''say 'hi' to all'''   // n+1 quotes quote a text with n consecutive quotes
    lines  = ( 'one' &#10; 'two' )   // a compound value: quotes and entities, no whitespace kept
    para {
        'Hello '                     // standalone text must always be quoted
        b = World
        '!'
    }
    // a multiline quote, the incidental indentation is removed
    address = 'The Vasa Museum
               115 21 Stockholm'
    br                               // an empty element <br/>
    /* a multiline
       comment */
    x:item(xmlns:x = urn:x) = 1      // namespaces work as in xml
}
```

* `name = value` is short for `name { 'value' }`. Content is either a value after `=`, or a body in `{ }`.
* Tokens are separated by spaces and newlines only, never by tabs, commas or semicolons.
* Text that is not safe must be quoted, entities (`&#10;` `&amp;` `&nbsp;`) are written outside of quotes.
* Comments are `// to end of line` and `/* ... */`, they become xml comments.
* The file is UTF-8, CRLF and CR are read as LF.

## Values and quoting

### When must a value be quoted?

A value can be written without quotes if it is safe: it contains no whitespace (also no
non-breaking space or other unicode space), none of `' " ( ) { }`, and it does not start with
`=`, `&`, `//` or `/*`. So these need no quotes:

```
id    = 889923
url   = https://a.b.c/api?x=2
path  = /drivers/r.sh
tag   = <car>          // becomes &lt;car&gt; in xml
corp  = R&D            // & is fine inside, only not first
regex = x/*y*/         // /* and // are only special first
```

These must be quoted: `'hello world'`, `'f(x)'`, `'=x'`, `'/*not a comment*/'`, `'&amp;'` (if you
mean the five characters and not the entity).

### What happens if I forget the quotes around text with a space?

The value ends at the space and the next word becomes a new element:

```
a = hello world        // is <a>hello</a><world/>
a = 'hello world'      // is <a>hello world</a>
```

### How do I write the empty string?

`''` (or `""`). A quote of only spaces and one newline is also the empty string.

### How do I quote a text that contains single quotes?

Use double quotes, `"It's"`, or n+1 single quotes for a text with at most n consecutive
single quotes: `'''It's'''`, `''''a''b''''`. Two quotes are reserved for the empty string, so
the opening is one, three or more quotes. Doubling a quote inside the quote, as in SQL, is an
error: `'it''s'` fails with "quote closed with too many quotes".

### How do I write a value that starts or ends with spaces, quotes or newlines?

```
spaces = '  alfa  '                    // leading and ending spaces are kept on one line
spaces = ( &#32;&#32; 'alfa' &#32;&#32; )
apos   = "'alfa'"                      // or ( &#39; 'alfa' &#39; )
nl     = ( &#10; 'alfa' &#10; )        // a newline first and last
```

### Can I put entities inside a quote?

No, quotes are literal. `'x&#10;y'` is the text `x&#10;y` (seven characters). Write the entity
outside of the quotes: `( 'x' &#10; 'y' )` in a value, or `{ 'x' &#10; 'y' }` in a body.

### What is a compound value `( ... )`?

A sequence of quotes and entities after `=`, concatenated without any whitespace between them,
for example `( 'line 1' &#10; 'line 2' )`. `()` is an empty value. It is required for an
attribute value that needs entities, since an attribute value cannot have a body:
`a(title = ( 'x' &#10; 'y' ))`.

### Are `a = 1` and `a=1` the same?

Yes, the spaces around `=` are separating whitespace. `xmq --compact` prints `a=1 b='x y'`.

### Does a comma or semicolon separate values?

No. `a { b = 1, c = 2 }` gives `b` the value `1,`. Separate with spaces or newlines.

## Multiline quotes and whitespace

### How is the indentation of a multiline quote handled?

The incidental indentation is removed, as in Java text blocks: the smallest indentation of the
lines (the first line and empty lines are not counted, the line with the closing quote is) is
removed from every line, and the first line is assumed to have exactly that indentation.

```
address = 'The Vasa Museum
           Galärvarvsvägen 14
           Sweden'
```

is `The Vasa Museum\nGalärvarvsvägen 14\nSweden`. Relative indentation is kept:

```
a = '
    line one
      indented
    line three
    '
```

is `line one\n  indented\nline three`.

### Why did my multiline quote get an extra leading space?

The line with the closing quote counts for the indentation. If the closing quote is indented
less than the text, the difference stays in the content:

```
a = '
    alfa
   '            // the content is ' alfa', align the quote with the text to get 'alfa'
```

### What happens to newlines first and last in a multiline quote?

Leading and ending whitespace is trimmed to the newlines only, minus one: `'\n\n    alfa\n\n    '`
(with the quote aligned) is `\nalfa\n`. Spaces before a newline are always removed, so trailing
spaces of a line must be written as entities, `( 'x' &#32;&#32; &#10; 'y' )`, which is also how
xmq prints them.

### Is whitespace between elements kept?

No. Whitespace outside of quotes only separates tokens, so XMQ can always be pretty printed or
printed compact (`--compact`) without changing the content. All content whitespace is quoted.
When xml/html is read, xmq trims the whitespace with a heuristic (incidental indentation and
leading/ending whitespace with newlines), use `--trim=none` to keep all of it.

### Can I use tabs?

Not as separators or indentation, a tab outside of a quote is an error. A tab inside a quote is
content, or write `&#9;`.

## Elements, attributes and text

### What is the difference between a key and an element name?

None in the document, `price = 7` and `price { '7' }` are the same element. The colors of the
syntax highlighting call it a key when `=` follows the name directly and a name otherwise.

### How do I mix text and elements (markup)?

Text in a body must be quoted, a word that is not quoted is an element:

```
p { 'Hello ' b = World '!' }     // <p>Hello <b>World</b>!</p>
p { Hello }                      // <p><Hello/></p>
```

Adjacent quotes are concatenated without whitespace: `a { 'one' 'two' }` is `onetwo`.

### Is `a = 'x' 'y'` a value of two quotes?

No, the value of `a` is `x` and `'y'` is a text sibling after `a`. Use `a = ( 'x' 'y' )`.

### How are attributes written?

In parentheses after the element name, before the value or the body:
`a(href = 'https://x.y' id = 3 hidden) = Click`. An attribute without `= value` gets the empty
value. Attribute values follow the value rules, but braces cannot be used, `a(b = { c })` is an
error.

### How do I write an empty element?

Just the name: `br`, `hr(class = x)` or `e {}`.

### What names are allowed?

As in xml: a name starts with a letter or underscore and contains letters, digits, `-`, `_` and
`.`, with an optional `prefix:` (eg `a.b-c_d`, `xsl:template`). Names starting with `xml` are
reserved by xml. Names cannot be quoted. The exceptions are `!DOCTYPE`, `?target` for processing
instructions and `&...;` entities.

### How do namespaces work?

As in xml, the declarations are attributes: `x:root(xmlns:x = urn:x) { x:a = 1 }` and
`root(xmlns = urn:d) { a = 1 }`.

### Can a file have several root elements?

Yes. `a = 1 b = 2` is valid XMQ, which is convenient for config files. Give an implicit root when
it is converted or loaded: `xmq --root=conf config.xmq to-xml`, `xmq config.xmq add-root conf`, or
`xmqParseFile(doc, "config.xmq", "conf", 0)` in C.

### Can I embed xml in an xmq file?

Yes, an xml fragment can stand where a node can: `r { a = 1 <b>x</b> }`.

### How are DOCTYPE, processing instructions, CDATA and entities written?

```
!DOCTYPE = html                          // the inline DTD is the value of !DOCTYPE
?xml-stylesheet = 'type="text/css" href="s.css"'
p = &nbsp;                               // an entity reference stays an entity
```

CDATA has no syntax of its own, it is read as text and quoted.

## Comments

### How do I comment?

`// to the end of the line` and `/* ... */`. A comment that contains `*/` is written with more
slashes: `///* has /* and */ inside *///`. `#` and `<!-- -->` are not xmq comments.

### Where can a comment not be?

Inside a quote, where `//` and `/*` are just text, also on the lines of a multiline quote. And
between an element and its body: `a(x = 1) /* c */ { b = 1 }` and a `// c` line between `a` and
`{` are errors, since the comment ends the element `a`. Put the comment before the element or
inside the body.

### How is a multiline comment printed on one line?

With `*/*` for each newline: `xmq --compact` prints `/* one\n two */` as `/*one*/*two*/`.

### What about `--` inside comments?

XMQ comments may contain `--`, which xml comments cannot. When xmq writes xml it inserts the
character ␐ between the dashes, and removes it again when the xml is read.

## JSON

### How does XMQ map to JSON?

```
r(id = 3) {
    n = 5
    s(S) = 5
    t = true
    list(A) {
        _ = 1
        _ = 2
    }
    p = a
    p = b
}
```

is, with `xmq file.xmq to-json`:

```json
{"_":"r","_id":"3","n":5,"s":"5","t":true,"list":[1,2],"p[0]":"a","p[1]":"b"}
```

The element name of an object is the key `"_"`, a JSON object without a name becomes the element
`_`. `(A)` marks an array, `(S)` forces a string. Values that are JSON numbers, `true`, `false` or
`null` become JSON values. Repeated element names become `p[0]`, `p[1]` and attributes become
keys prefixed with `_`. Reading the JSON back gives the same xmq. In XPath the anonymous
objects are `_`, eg `/_/todos/_/what`.

## HTMQ, XSLQ and the file suffixes

### What are .htmq, .xslq and .xsq?

The same syntax for html (htmq, printed with `to-html`), xslt stylesheets (xslq) and xsd schemas
(xsq). The Q does not mean anything.

### Why is my xslq attribute value template not evaluated?

The braces of an attribute value template must be quoted: `out(b = '{$v}')` or `out(b = "{$v}")`.
Without braces, `out(a = $v)` is the literal text `$v`, as in xslt.

### Why does whitespace-only text disappear in my xslq?

XSLT strips whitespace-only text nodes from a stylesheet, use `xsl:text = ' '`.

## For LLM agents generating XMQ

A checklist of the mistakes that are easy to make:

1. Quote every value with whitespace or `' " ( ) { }`, or starting with `= & // /*`.
2. Quote all standalone text in a body, an unquoted word becomes an element.
3. Put entities outside of the quotes, `( 'a' &#10; 'b' )`, never `'a&#10;b'`.
4. Never write `'it''s'`, use `"it's"` or `'''it's'''`. `''` is the empty string.
5. Indent with spaces, never tabs. Do not separate with commas or semicolons.
6. Align the closing quote of a multiline quote with the text, or the difference becomes content.
7. Comments are `//` and `/* */`, not `#` or `<!-- -->`, and never between an element and its `{`.
8. Attribute values cannot have a body, use a compound value `( ... )`.
9. Check the result: `xmq file.xmq to-xml` reports the line and column of an error.
10. Let xmq print the canonical form: `xmq file.xml to-xmq`, `xmq file.json`, `xmq file.xmq --compact`.
