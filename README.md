# libxmq

```
./configure
make
make test
sudo make install
```

View an html page: `xmq-less page.html`

Convert any input to xmq: `xmq input.xml to_xmq > output.xmq`

Convert any input to xml: `xmq input.xmq to_xml > output.xml`

Convert any input to html: `xmq input.htmq to_html > output.html`

Render any input as a web page: `xmq input.xmq render_html > input_as_web_page.html`

Render any input as a tex: `xmq input.xmq render_tex > input_as_tex.tex ; xelatex input_as_tex.tex`
