# libxmq

Homepage [https://libxmq.org](https://libxmq.org)

API [https://libxmq.org](https://libxmq.org/gtkdoc/index.html)

If you want to add support for xmq to your program,
just place xmq.h and xmq.c in your source code directory.
Add libxml2 to the dependencies and you are done. :-)

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
