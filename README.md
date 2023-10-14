# libxmq

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
