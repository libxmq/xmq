# xmqjs

The [xmq](https://libxmq.org) command line compiled to WebAssembly and run with node.
It reads and writes xmq, xml, html, json, yaml and clines, runs ixml grammars and
xslt/xslq transforms, and validates with xsd.

```
npm install -g xmqjs
xmqjs pom.xml
xmqjs data.json to-xml
xmqjs ixml:data/csv people.csv to-json
```

The grammar library (`ixml:...`, `xsl:...`) is included in the package. The commands
are the same as for the native `xmq`, see `xmqjs --help`.

## Differences from the native xmq

- It cannot start other programs, so commands that do (eg fetching a missing library
  file with curl, `shell=`, or running xmq again) fail.
- It does not ask the terminal for its background color, it assumes a dark background.
  Set `XMQ_BG=light` or use `--bg=light` for a light one.
- The terminal is not put in raw mode, so the interactive commands that read single key
  presses do not work.
