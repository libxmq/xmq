# XMQ

Convert xml to a human readable/editable format and back.

| OS           | Status           |
| ------------ |:-------------:|
|GNU/Linux & MacOSX| [![Build Status](https://travis-ci.org/weetmuts/xmq.svg?branch=master)](https://travis-ci.org/weetmuts/xmq) |

Xml can be human readable/editable if it is used for markup of longer
human language texts, ie books, articles and other documents etc. In
these cases the xml-tags represent a minor part of the whole xml-file.

However xml is often used for configuration files, the most prevalent
is the pom.xml files for maven.  In such config files the xml-tags
represent a major part of the whole xml-file. This makes the config
files hard to read and edit directly by hand.

The xmq format is simply a restructuring of the xml that (to me at
least) makes config files written in xml easier to read and edit.

The xmq format exactly represents the xml format and can therefore be
converted back to xml after any editing has been done. (Caveat
whitespace trimmings.)

Xmq can also be used as a configuration language directly without
converting to xml.

Xmq is also a surprisingly convenient way of writing html as well,
not just because the html-tags are nowadays a majority of
html pages, but because xmq makes significant whitespace explicit!

Xmq reserves six single character tokens `='(){}` and whitespace `space tab lf cr`, and
whenever a text string, be it a  key or value, contains the reserved characters, it has to be quoted
with single quotes. A double quote is the empty string. Use n+1 quotes (minimum 3) to quote strings with n quotes.

No quotes needed for numbers: `12.4` files: `/alfa/beta.txt` backslashes: `"info%d\n"` tags: `<info>`

Quotes needed for spaces: `'My Name'` code: `'x=call(1,2)'` quotes: `'''alert('Warning!'+'');'''`

A comment starts with `//` or `/*` and ends with eol or `*/`. Thus a value string that starts with the
comment starters, must be quoted.

Grammar is here: [xmq.pdf](https://github.com/weetmuts/xmq/blob/master/doc/xmq.pdf)

Check the specification tests here: [https://weetmuts.github.io/xmq](https://weetmuts.github.io/xmq)

# Usage

Type `xmq pom.xml > pom.xmq` to convert your pom.xml file into an xmq file.

Make your desired changes in the xmq file and then
do `xmq pom.xmq > pom.xml` to convert it back.

You can read form stdin like this:  `cat pom.xml | xmq -`
and there is a utility to view xml files: `xmq-less pom.xml`

You can view colorify and pretty print an xmq file without converting it to xml:
`xmq -v config.xmq`

There is an xmq major mode for emacs in xmq-mode.el.
Put it into your `.emacs.d/site-lisp` directory and
require it `(require 'xmq-mode)` in your .emacs file.

You can also bind xmq to for example ctrl-t `(global-set-key (kbd "C-t") 'xmq-buffer)`
When you have an xml file in the buffer and you press ctrl-t, you will
switch the buffer back and forth between xmq and xml.

Do `make && sudo make install` to have xmq, xmq-less, xmq-diff, xmq-git-diff and xmq-meld
installed into /usr/local/bin.

# Terminal example

Xml to the left. Colored XMQ (using xmq-less) to the right.

![XML vs XMQ](/doc/xml_vs_xmq.png)

Multiline content is quoted:

![Multiline](/doc/multiline.png)

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
