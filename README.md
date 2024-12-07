# XMQ/HTMQ | Convert xml/html to a human readable/editable format and back. Also parse any content using an Invisible XML grammar (IXML).

Homepage [https://libxmq.org](https://libxmq.org)

API [https://libxmq.org/api/](https://libxmq.org/api/)

Grammar [https://libxmq.org/xmq.pdf](https://libxmq.org/xmq.pdf)

Binaries latest release [https://github.com/libxmq/xmq/releases/latest](https://github.com/libxmq/xmq/releases/latest)

[<img src="https://libxmq.org/resources/xmq_example.jpg">](https://libxmq.org/resources/xmq_example.jpg)

| System       | Status        |
| ------------ |:-------------:|
| Ubuntu | [![Build Ubuntu Status](https://github.com/libxmq/xmq/workflows/Build%20Ubuntu/badge.svg)](https://github.com/libxmq/xmq/actions)|
| MacOSX | [![Build MacOSX Status](https://github.com/libxmq/xmq/workflows/Build%20MacOSX/badge.svg)](https://github.com/libxmq/xmq/actions)|
| Windows | [![Build Windows Status](https://github.com/libxmq/xmq/workflows/Build%20Windows/badge.svg)](https://github.com/libxmq/xmq/actions)|

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
whitespace to be quoted. The xmq format also uses a shortcut where
not all values need to be quoted and this also improves readability.

This means that it is always possible to pretty print the xmq without
having to worry if you introduce visible changes to your web document!
This is very helpful when deciphering complicated html documents.

```
car {
    speed = 50km/h
    color = red
    brand = 'Le roy'
}
```

Opposite pretty printing, any xmq/htmq document (including multiple
line comments) can also be printed in a compact form on a single line.

`car{speed=50km/h color=red brand='Le roy'}`

The xmq format can exactly represent the content of the xml/html file
and can therefore be converted back to xml/html after any editing has
been done. It might not however generate exactly the same xml/html
file, byte per byte, because xmq uses its own set of choices for
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

# Use IXML to convert any input into XML/JSON/XMQ!

Write an ixml grammar for dates and store this into dates.ixml
```
date: day, -" ", month, -" ", year.
day: digit;
     digit, digit.
-digit: "0"; "1"; "2"; "3"; "4"; "5"; "6"; "7"; "8"; "9".
month: "January"; "February"; "March"; "April"; "May"; "June";
       "July"; "August"; "September"; "October"; "November"; "December".
year: digit, digit, digit, digit
```

Then you run `xmq --ixml=dates.ixml -i '22 November 2024'`
and you will get the output:
```
date {
    day   = 22
    month = November
    year  = 2024
}
```

or if you prefer json: xmq --ixml=dates.ixml -i '22 November 2024' to-json | jq .`
```
{
  "_": "date",
  "day": 22,
  "month": "November",
  "year": 2024
}
```

or if XML: xmq --ixml=dates.ixml -i '22 November 2024' to-xml` (manually pretty printed)
```
<date>
    <day>22</day>
    <month>November</month>
    <year>2024</year>
</date>
```

Check out [https://invisiblexml.org/](https://invisiblexml.org/) for more information.

# The xmq cli command

The xmq tool has several useful commands, for example:

```shell
# Render pom.xml as xmq in color on terminal.
xmq pom.xml
cat index.html | xmq
xmq < rss.xml

# Use the built in pager to scroll up and down.
xmq pom.xml page
xmq pom.xml pa
cat rss.xml | xmq pa
xmq pa < index.html
curl -s 'https://dummyjson.com/todos?limit=10' | xmq pa

# Write xmq_browsing_pom.xml.html and open this file with the default browser.
xmq pom.xml browse
xmq pom.xml br
xmq br < index.html

# View a json file as xmq in a pager or browser
xmq request.json pager
curl -s 'https://dummyjson.com/todos?limit=10' | xmq br

# View a large index.html but delete script and style tags.
xmq index.html delete //script delete //style pager

# The same but render to browse_tmp_dn.html and browse it.
xmq index.html delete //script delete //style br

# Replace entities with strings you can also --with-text-file=abc
# which inserts the content safely quoted, or as DOM --with-file=abc.xml
# where the file has be parseable.
xmq template.htmq replace-entity DATE 2024-01-11 replace-entity NAME 'Hercules' render-html --theme=lightbg > page.html

# Apply an xslq transform to some json to generate a html page.
xmq todos.json transform todos.xslq to-html > list.html

# Apply an xslq transform to generate plain text.
xmq todos.json transform todosframed.xslq to-text > list.txt

# Convert xml to json.
xmq pom.xml to-json | jq .

# Render content for tex typesetting.
xmq input.xmq render-tex > input_as_tex.tex ; xelatex input_as_tex.tex

# Compose a new DOM from xmq snippets and write the xml
xmq -z add weight=2.5ton add speed=123km/h add-root car to-xml
```

# Using xmq.h and xmq.c in your program

If you want to add support for xmq to your program, just copy paste
dist/xmq.h and dist/xmq.c in your source code directory and add
libxml2 to the build dependencies and you are done. :-)

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

Any Posix platform:
```
./configure
make
sudo make install
```

Windows cross complation from GNU/Linux:
```
(cd 3rdparty; fetch_and_build.sh)
./configure --host=x86_64-w64-mingw32 --with-libxml2=3rdparty/libxml2-winapi --with-libxslt=3rdparty/libxslt-winapi --with-zlib=3rdparty/zlib-1.3-winapi
make
```

The msi installer is found here: `./build/x86_64-w64-mingw32/windows_installer/xmq-windows-release.msi`
It will install the xmq.exe and its supporting dlls here: `C:\Program Files (x86)\libxmq\xmq`
Add this dir to your PATH: `PATH=%PATH%;"C:\Program Files (x86)\libxmq\xmq"`

GNU/Linux on aarch64-linux-gnu cross complation from GNU/Linux AMD64:
```
sudo apt install gcc make g++-aarch64-linux-gnu gcc-aarch64-linux-gnu binutils-aarch64-linux-gnu
(cd 3rdparty; fetch_and_build.sh aarch64-linux-gnu)
./configure --host=aarch64-linux-gnu --with-libxml2=3rdparty/libxml2-posix-aarch64 --with-libxslt=3rdparty/libxslt-posix-aarch64 --with-zlib=3rdparty/zlib-1.3-posix-aarch64
make
```

GNU/Linux on armv7l-unknown-linux-gnueabihf cross complation from GNU/Linux AMD64:
```
sudo apt install gcc make g++-arm-linux-gnueabi gcc-arm-linux-gnueabi binutils-arm-linux-gnueabi
(cd 3rdparty; fetch_and_build.sh armv7l-linux-gnu)
./configure --host=armv7l-unknown-linux-gnueabihf --with-libxml2=3rdparty/libxml2-posix-armv7l --with-libxslt=3rdparty/libxslt-posix-armv7l --with-zlib=3rdparty/zlib-1.3-posix-armv7l
make
```

## How to install the gnulinux binary executable

Download the xmq-gnu-linux-release.tar.gz file.
```
cd Downloads
tar xzf xmq-gnu-linux-release.tar.gz
mkdir -p ~/bin
mv xmq ~/bin
```
Restart your shell and xmq should be in your path.

## How to install the macos binary executable

Download the xmq-macos-release.tar.gz file.
```
cd Downloads
tar xzf xmq-macos-release.tar.gz
xattr -d com.apple.quarantine xmq
mkdir -p ~/bin
mv xmq ~/bin
```
Restart your shell and xmq should be in your path.

## How to install the windows binary executable

Download the xmq-windows-release.msi and install it.

You will find the xmq.exe and its supporting dlls here: `C:\Program Files (x86)\libxmq\xmq`
Add this dir to your PATH: `PATH=%PATH%;"C:\Program Files (x86)\libxmq\xmq"`

## Help
