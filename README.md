# XMQ

Convert xml/html to a human readable/editable format and back.

| OS           | Status           |
| ------------ |:-------------:|
| Ubuntu | [![Build Ubuntu Status](https://github.com/weetmuts/xmq/workflows/Build%20Ubuntu/badge.svg)](https://github.com/weetmuts/xmq/actions)|
| MacOSX | [![Build MacOSX Status](https://github.com/weetmuts/xmq/workflows/Build%20MacOSX/badge.svg)](https://github.com/weetmuts/xmq/actions)|

# Rationale

Xml can be human readable/editable if it is used for markup of longer
human language texts, ie books, articles and other documents etc. In
these cases the xml-tags represent a minor part of the whole xml-file.

However xml is often used for configuration files, such as the
pom.xml.  In such config files the xml-tags represent a major part of
the whole xml-file.  This makes the config files hard to read and edit
directly by hand.  Today, the tags are a major part of html files as
well, which is one reason why html files are hard to read and edit.

The other reason why xml/html are hard to edit by hand is because they
have a complicated approach to dealing with significant whitespace.
The truth is that you cannot always pretty print the xml/html code as
you would like since it might introduce significant white space.  In
fact proper pretty printing even requires a full understanding of the
DTD/XSD/CSS!  This is indeed a daunting task just for pretty printing inside
an editor!

The xmq format solves the first problem primarily by using braces to
open and close tags and the second problem by forcing significant
whitespace to be quoted.

This means that it is always possible to pretty print the xmq without
having to worry if you introduce visible changes to your web document!
This is very helpful when deciphering complicated html documents.

The xmq format can exactly represent the content of the xml/html file
and can therefore be converted back to xml/html after any editing has
been done.  It might not however generate exactly the same xml/html
file, byte per byte, because xmq uses its own set of choices for:
pretty printing, quotes and escaping. For html you can use the suffix
htmq to indicate that you have written html using the xmq format.

Xmq cannot represent broken html and will give up on broken html since
the tags must always balance or be self-closing. Use a tool like
[tidy](http://manpages.ubuntu.com/manpages/bionic/man1/tidy.1.html)
to make sure the html is fully compliant before converting to xmq.

Xmq can of course also be used as a configuration language directly.

# Rules

Xmq reserves six single character tokens `='(){}` and whitespace
`space tab lf cr` + unicode whitespace, and whenever a text string,
contains the reserved characters, it has to be quoted with single
quotes. Two consecutive single quotes are an empty string. Use n+1
quotes (minimum 3) to quote strings with n quotes.

Only value strings have to be quoted. The keys (aka the tag names are
limited to the standard xml tag rules, which prohibits the reserved
characters anyway.

No quotes needed for numbers: `12.4` files with slashes: `/alfa/beta.txt` files with backslashes: `C:\Foo\bar.png`
strings with backslashes: `"info%d\n"` tags: `<info>`

Quotes are needed for whitespace: `'My Name'` source code with reserved chars: `'x=call(1,2)'`
single quotes: `'''alert('Warning!'+'');'''` the empty string must be just two single quotes: `''`

A comment starts with `//` or `/*` and ends with eol or `*/`. Thus a
value string that starts with the comment starters, must also be quoted.

Grammar is here: [xmq.pdf](https://github.com/weetmuts/xmq/blob/master/doc/xmq.pdf)

Check the specification tests here: [https://weetmuts.github.io/xmq](https://weetmuts.github.io/xmq)

# Examples

![Example1](/doc/ex1.png)

![Example1](/doc/ex2.png)

![Example1](/doc/ex3.png)

# Usage

Type `xmq pom.xml > pom.xmq` to convert your pom.xml file into an xmq file.

Make your desired changes in the xmq file and then
do `xmq pom.xmq > pom.xml` to convert it back.

You can read form stdin like this:  `cat pom.xml | xmq -`
and there is a utility to view xml files: `xmq-less pom.xml`

You can view colorify and pretty print an xmq file without converting it to xml:
`xmq -v config.xmq` or `xmq-less -v config.xmq`

There is an xmq major mode for emacs in xmq-mode.el.
Put it into your `.emacs.d/site-lisp` directory and
require it `(require 'xmq-mode)` in your .emacs file.

You can also bind xmq to for example ctrl-t `(global-set-key (kbd "C-t") 'xmq-buffer)`
When you have an xml file in the buffer and you press ctrl-t, you will
switch the buffer back and forth between xmq and xml.

Do `make && sudo make install` to have xmq, xmq-less, xmq-diff, xmq-git-diff and xmq-meld
installed into /usr/local/bin.

# Command line options

```
--color force coloring.
--mono  prevent coloring.
--compress find common prefixes in tag names.
--exclude  exlude tags.
--html  assume that data is html, even though it does not start with an html tag.
--nodec do not add the xml/html5 declaration/doctype.
--nopp  do not pretty print xml/html.
--output=html produce output suitable inclusion between <pre>...</pre> tags.
--output=terminal write on terminal, use ansi colors if necessary.
--output=tex  produce output suitable for inclusion in tex documents.
--output=plain produce plain utf8 text.
-p   preserve whitespace when converting from xml to xmq.
--pp pretty print.
--root=<foo> if the xmq does not already have a single root node foo, then add it.
-v   view only, do not convert between xmq and xml/html.
```

# Emacs example

Assuming xmq-buffer is bound to ctrl-t, pressing ctrl-t
will flip between xml and xmq as can be seen below.

![XML vs XMQ](/doc/emacs_xml_xmq.png)

# Diffing

You can diff two xml files: `xmq-diff old.xml new.xml`

You can diff an xml file against its git repo: `xmq-git-diff file.xml` or using `xmq-git-meld file.xml`

You can meld two xml files: `xmq-meld old.xml new.xml`

# Excluding attributes

You can exclude the attribute `foo` in every node: `xmq-less -x @foo foo.xml`

# Compressing

If the node names are very long and have consistent prefixes
you can compress the output: `xmq-less -c foo.xml` `xmq-git-diff -c -x @name foo.xml`

This will replace a long prefix `abc.def.ghi` with for example `0:`
depending on how many prefixes are found. The prefix is printed
at the top of the xmq file.
