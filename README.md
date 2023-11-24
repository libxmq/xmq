# XMQ/HTMQ | Convert xml/html to a human readable/editable format and back.

Homepage [https://libxmq.org](https://libxmq.org)

API [https://libxmq.org/api/](https://libxmq.org/api/)

Grammar [https://libxmq.org/xmq.pdf](https://libxmq.org/xmq.pdf)

| System       | Status        |
| ------------ |:-------------:|
| Ubuntu | [![Build Ubuntu Status](https://github.com/libxmq/xmq/workflows/Build%20Ubuntu/badge.svg)](https://github.com/libxmq/xmq/actions)|
| MacOSX | [![Build MacOSX Status](https://github.com/libxmq/xmq/workflows/Build%20MacOSX/badge.svg)](https://github.com/libxmq/xmq/actions)|

# Rationale

Xml can be human readable/editable if it is used for markup of longer
human language texts, ie books, articles and other documents etc. In
these cases the xml-tags represent a minor part of the whole xml-file.

However xml is often used for configuration files, such as the
pom.xml. In such config files the xml-tags represent a major part of
the whole xml-file. This makes the config files hard to read and edit
directly by hand. Today, the tags are a major part of html files as
well, which is one reason why html files are hard to read and edit.

The other reason why xml/html are hard to edit by hand is because they
have a complicated approach to dealing with significant
whitespace. The truth is that you cannot always pretty print the
xml/html code as you would like since it might introduce significant
white space. In fact proper pretty printing even requires a full
understanding of the DTD/XSD/CSS! This is indeed a daunting task just
for pretty printing inside an editor!

The xmq format solves the first problem primarily by using braces to
open and close tags and the second problem by forcing significant
whitespace to be quoted. The xmq format also uses a trick where
not all values need to be quoted this makes xmq even more readable.

This means that it is always possible to pretty print the xmq without
having to worry if you introduce visible changes to your web document!
This is very helpful when deciphering complicated html documents.

The xmq format can exactly represent the content of the xml/html file
and can therefore be converted back to xml/html after any editing has
been done. It might not however generate exactly the same xml/html
file, byte per byte, because xmq uses its own set of choices for:
pretty printing, quotes and escaping.

For html you can use the suffix htmq to indicate that you have written
html using the xmq format. Xmq cannot represent broken html and will
give up on broken html since the tags must always balance or be
self-closing. Use a tool like tidy to make sure the html is fully
compliant before converting to xmq.

Xmq can of course also be used as a configuration language directly
for your application! You can just copy&paste dist/xmq.h and
dist/xmq.c into your source code directory to have xmq support.

In xmq you can omit the root node (if you supply an implicit root) this
can make your xmq config files for your application even simpler to read.
```
speed = 50km/h
count = 123
url   = https://foo/bar?x=123
msg   = 'Welcome to the app!'
```

# The xmq cli command

View the input file (xmq/xml/html/htmq/json) as xmq.

```
xmq-less pom.xml
xmq-less index.html
xmq-less request.json
```

# Using xmq.h and xmq.c in your program

If you want to add support for xmq to your program, just copy paste
dist/xmq.h and dist/xmq.c in your source code directory and add
libxml2 to the build dependencies and you are done. :-)

If you do not need output, ie you are only loading data, for example config files,
then uncomment: `// #define XMQ_NO_XMQ_PRINTING` in xmq.h.

If you do not need full xml/html support, then uncomment:
`// #define XMQ_NO_LIBXML` in xmq.h and xmq.c will not require libxml2.
This means that you cannot load/print xml/html files.
This will limit the xpath support to a small subset suitable for normal config files.

If you do not need json support, similarily uncomment:
`// #define XMQ_NO_JSON` in xmq.h.

# JSON

Xmq can work with json/jsonl files as if they were xmq files.

The workflow is intended to be: 1) convert json to xmq 2) work on the data as xmq 3) possibly write back to json.

Some clean xml files (like pom.files) can be converted to clean json, worked on in json and then converted back to xmq.

But if you convert generic xml/html with standalone text nodes and attributes to json, then the result is probably rather confusing.

# Building

```
./configure
make
make test
sudo make install
```

View an xml file: `xmq-less file.xml`

View an html page: `xmq-less page.html`

View the same page but remove all script and style nodes and smaller indentation.
```
xmq-less page.html delete //script delete //style render_terminal --color --indent=2
```

Convert any input to xmq: `xmq input to_xmq > output.xmq`

Convert any input to xml: `xmq input to_xml > output.xml`

Convert any (suitable) input to html: `xmq input to_html > output.html`

```
xmq 'book { alfa = &ALFA; }' replace entity:ALFA "Howdy'''Dowdy" to_xmq
```

Render any input as a web page: `xmq input.xmq render_html > input_as_web_page.html`

Render any input as a tex: `xmq input.xmq render_tex > input_as_tex.tex ; xelatex input_as_tex.tex`

Build with debug symbols:
```
make debug
make testd
```

You can find the built debug binary here:
`gdb --args ./build/x86_64-pc-linux-gnu/debug/xmq input.xml`

Build with address sanitizer :
```
make asan
make testa
```

You can find the built asan binary here:
`./build/x86_64-pc-linux-gnu/asan/xmq input.xml`

## How to compile:

Windows cross complation from GNU/Linux:
```
(cd 3rdparty; fetch_and_build.sh)
./configure --host=x86_64-w64-mingw32 --with-libxml2=3rdparty/libxml2
```