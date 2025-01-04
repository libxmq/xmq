# XMQ | A tool and language to work with xml/html/json and parse any other format using ixml.

Homepage [https://libxmq.org](https://libxmq.org)

API [https://libxmq.org/api/](https://libxmq.org/api/)

Grammar [https://libxmq.org/xmq.pdf](https://libxmq.org/xmq.pdf)

Binaries latest release [https://github.com/libxmq/xmq/releases/latest](https://github.com/libxmq/xmq/releases/latest)

You can also install with `brew install xmq`

[<img src="https://libxmq.org/resources/xmq_example.jpg">](https://libxmq.org/resources/xmq_example.jpg)

| System       | Status        |
| ------------ |:-------------:|
| Ubuntu | [![Build Ubuntu Status](https://github.com/libxmq/xmq/workflows/Build%20Ubuntu/badge.svg)](https://github.com/libxmq/xmq/actions)|
| MacOSX | [![Build MacOSX Status](https://github.com/libxmq/xmq/workflows/Build%20MacOSX/badge.svg)](https://github.com/libxmq/xmq/actions)|
| Windows | [![Build Windows Status](https://github.com/libxmq/xmq/workflows/Build%20Windows/badge.svg)](https://github.com/libxmq/xmq/actions)|

## XMQ another way to work with XML/HTML/JSON

The xmq format can exactly represent the content of the xml/html/json file
and can therefore be converted back to xml/html/json after any editing has
been done.

For html you can use the suffix htmq to indicate that you have written
html using the xmq format.

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

The xmq tool has several useful commands, for example:

```shell
# Render pom.xml as xmq in color on terminal.
xmq pom.xml
cat rss.xml | xmq
xmq rest.json
xmq work.xmq

# Parse any input with a suitable ixml grammar. Output xmq/xml/json etc.
xmq --ixml=csv.ixml input.txt

# Use the built in pager (pa) to scroll up and down.
xmq pom.xml pa
cat rss.xml | xmq pa
curl -s 'https://dummyjson.com/todos?limit=10' | xmq pa

# Browse (br) the content as a web page
xmq pom.xml br
xmq rest.json br

# View a large index.html but delete script and style tags.
xmq index.html delete //script delete //style pa

# The same but render to browse_tmp_dn.html and browse it.
xmq index.html delete //script delete //style br

# List all hyperlinks in a web page.
xmq index.html select //a

# Compose a new web page with all the images from the source html.
# And immediately view it using your default browser.
xmq page.html select //img add-root body add-root html to-html br

# List all href attributes in a web page.
xmq page.html select //a/@href

# Replace entities with strings you can also --with-text-file=abc
# which inserts the content safely quoted, or as DOM --with-file=abc.xml
# where the file has be parseable.
xmq template.htmq replace-entity DATE 2024-01-11 replace-entity NAME 'Hercules' render-html --theme=lightbg > page.html

# Apply an xslq transform to some json to generate a html page.
xmq todos.json transform todos.xslq to-html > list.html

# Apply an xslq transform to generate plain text, ie keep only the text nodes.
xmq todos.json transform todosframed.xslq to-text > list.txt

# Convert xml to json.
xmq pom.xml to-json | jq .

# Convert to xml.
xmq rest.json to-xml

# Print xmq compact form on a single line.
xmq pom.xml to-xmq --compact

# Print DOM as clines (eg. /html/body="Hello")
xmq pom.xml to-clines

# Render content for tex typesetting.
xmq input.xmq render-tex > input_as_tex.tex ; xelatex input_as_tex.tex

# Compose a new DOM from xmq snippets and write the xml
xmq -z add weight=2.5ton add speed=123km/h add-root car to-xml
```

## Use IXML to convert any input into XML/JSON/XMQ

Write an ixml grammar for dates and store this into dates.ixml
```
date   = day, -' ', month, -' ', year.
day    = digit | digit, digit.
-digit = '0' | '1' | '2' | '3' | '4' | '5' | '6' | '7' | '8' | '9'.
month  = 'January' | 'February' | 'March' | 'April' | 'May' | 'June' |
         'July' | 'August' | 'September' | 'October' | 'November' | 'December'.
year   = digit, digit, digit, digit.
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

or if you prefer JSON: xmq --ixml=dates.ixml -i '22 November 2024' to-json | jq .`
```
{
  "_": "date",
  "day": 22,
  "month": "November",
  "year": 2024
}
```

or if you prefer XML: xmq --ixml=dates.ixml -i '22 November 2024' to-xml` (manually pretty printed)
```
<date>
    <day>22</day>
    <month>November</month>
    <year>2024</year>
</date>
```

Check out [https://invisiblexml.org/](https://invisiblexml.org/) for more information.

Here is an ixml grammar to convert a file with lines of tab separated values:

```
lines = line+.
line  = col++-#9, nl.
col   = ~[ #9; #a; #d ]+.
-nl   = -#d?, -#a.
```

Given the tabbed input file:
```
2024-01-01<TAB>red<TAB>8<NL>
```

Generates xmq such as:
```
lines {
    line {
        col = 2024-01-01
        col = red
        col = 8
    }
}
```

That can be converted to xml or json. But if you know the column names, you can create
a more tailored grammar:
```
cars  = car+.
car   = date, -#9, color, -#9, count, nl.
date  = ~[#9;#a;#d]+.
color = ~[#9;#a;#d]+.
count = ~[#9;#a;#d]+.
-nl   = -#d?, -#a.
```

And the xmq/xml/json output will have matched the columns to keys directly.
```
cars {
    car {
        date  = 2024-01-01
        color = red
        count = 8
    }
}
```

## Using xmq.h and xmq.c in your program

If you want to add support for xmq to your program, just copy paste
dist/xmq.h and dist/xmq.c in your source code directory and add
libxml2 to the build dependencies and you are done. :-)

## Building

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

Build with address sanitizer :
```
make asan
make testa
```

(To run asan tests you might have to disable address space randomization first:
`make disable_address_randomization`)

## Cross complation to Windows and ARM

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

## How to install the MacOS binary executable

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
