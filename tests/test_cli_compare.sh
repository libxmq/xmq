#!/bin/bash
# libxmq - Copyright 2026 Fredrik Öhrström (spdx: MIT)
#
# Compare the Java command line (xmqj) with the C command line (xmq): both parse
# the command line with library/xmq-utils/xmqcli.ixml and must give the same
# output, errors and exit codes.
#
# Usage: tests/test_cli_compare.sh XMQ_BINARY XMQJ_BINARY OUTPUT_DIR

X=$1
J=$2
O=$3
if [ ! -x "$X" ] || [ ! -x "$J" ] || [ -z "$O" ]
then
    echo "Usage: tests/test_cli_compare.sh XMQ_BINARY XMQJ_BINARY OUTPUT_DIR"
    exit 1
fi
mkdir -p "$O"

printf '<!DOCTYPE html><html><body><script>x</script>howdy<script>y</script></body></html>' > "$O/d.html"
printf 'rad1\n\nrad2\n' > "$O/t.txt"
printf '"a\\tb\\n"\n' > "$O/q.txt"
echo 'a{b=1 c(x=2)=3 d=&e;}' > "$O/a.xmq"
echo 'b=bold' > "$O/dom.xmq"
printf '<!DOCTYPE goo [<!ENTITY copy "&#169;">\n<!ENTITY prutt "PRUTT&copy;"> ]>\n<goo x="&prutt;alfa">X&copy;Y</goo>\n' > "$O/entcycle.xml"

n=0
f=0
cmp1()
{
    n=$((n+1))
    a=$("$X" "$@" 2>&1; echo "rc=$?")
    b=$("$J" "$@" 2>&1; echo "rc=$?")
    if [ "$a" != "$b" ]
    then
        f=$((f+1))
        echo "DIFF: xmq $*"
        diff <(echo "$a") <(echo "$b") | head -8
    fi
}

cmp1 "$O/d.html" delete //script to-xmq --compact
cmp1 "$O/d.html" select //script
cmp1 "$O/d.html" select 'count(//script)'
cmp1 "$O/d.html" select 'count(//script)>1'
cmp1 "$O/a.xmq" select //@x
cmp1 "$O/a.xmq" select //b
cmp1 -i 'a{b=1}' add-root top
cmp1 -i 'a{b=1}' add 'c=2'
cmp1 -i 'a=&x;' replace-entity x hello
cmp1 "$O/a.xmq" replace-entity e hello to-xml
cmp1 "$O/a.xmq" replace-entity --with-file e "$O/dom.xmq"
cmp1 "$O/a.xmq" delete-entity e
cmp1 "$O/a.xmq" delete //@x
cmp1 --text "$O/t.txt" quote-c --add-nl to-text
cmp1 --text "$O/q.txt" unquote-c to-text
cmp1 "$O/a.xmq" to-xmq --indent=2
cmp1 "$O/a.xmq" to-xmq --indent=0
cmp1 "$O/a.xmq" to-xml -o
cmp1 "$O/a.xmq" to-xmq -c -n
cmp1 "$O/a.xmq" to-htmq
cmp1 "$O/a.xmq" no-output
cmp1 "$O/a.xmq" select //b to-xml
cmp1 --trim=none "$O/d.html"
cmp1 "$O/d.html" selekt //x
cmp1 "$O/a.xmq" select
cmp1 --bogus "$O/a.xmq"
cmp1 "$O/a.xmq" select -x //b
cmp1 -z add 'x=1'
echo '{"a":[1,"2",true,null,{"_":"x","b":"c d"}],"_e":"f","//":"note","bad key":3,"n[1]":0}' > "$O/j.json"
cmp1 "$O/j.json"
cmp1 "$O/j.json" to-json
cmp1 "$O/j.json" to-json -p
cmp1 "$O/j.json" to-xml
cmp1 "$O/a.xmq" to-json
cmp1 "$O/d.html" to-json --pretty
cmp1 --json "$O/j.json" select //b
cmp1 -i '[1,2' to-json
cmp1 -i '{"a":"\q"}'
cmp1 "$O/j.json" to-clines
cmp1 "$O/a.xmq" to-clines
cmp1 "$O/d.html" to-clines
printf "root {\nalfa = 'a\n\n        a'\n  /* x\n     y\n  */\n}\n" > "$O/trim.xmq"
cmp1 "$O/trim.xmq" to-xml
cmp1 "$O/trim.xmq" to-json
printf 'doc { v = 1 <address><street>foo</street></address> }\n' > "$O/emb.xmq"
cmp1 "$O/emb.xmq"
printf 'name: Alice\n' > "$O/colon.xmq"
cmp1 "$O/colon.xmq" to-xml
printf 'a: 1\nlist:\n  - x\n  - "true"\n  - k: v\n    j: w\nblock: |\n  one\n  two\nflow: [1, {b: c}]\nanchor: &id1 hej\nalias: *id1\n# a comment\n' > "$O/y.yaml"
cmp1 "$O/y.yaml"
cmp1 "$O/y.yaml" to-yaml
cmp1 "$O/y.yaml" to-json
cmp1 --yaml "$O/y.yaml" to-xml
cmp1 "$O/a.xmq" to-yaml
cmp1 "$O/j.json" to-yaml
printf 'a: 1\nb: [\n' > "$O/bad.yaml"
cmp1 "$O/bad.yaml"
cmp1 "$O/entcycle.xml" to-json

# The render commands. The terminal colors are ansi codes from the theme.
printf '?pi = 1\ndoc(a=(\x271\x27&lt;\x272\x27) b=\x27x y\x27) {\n    // c\n    t = \x27a<b & c\x27\n    m = \x27l1\n         l2\x27\n    e = &lt;\n}\n' > "$O/r.xmq"
cmp1 "$O/r.xmq" render-terminal --bg=dark
cmp1 "$O/r.xmq" render-terminal --bg=light --compact
cmp1 "$O/r.xmq" render-terminal --bg=dark --theme=EN=#ff0000_B_U:C=#00ff00
cmp1 "$O/r.xmq" render-terminal --bg=dark --use-is=. --use-es=_ --use-nl=NL
cmp1 "$O/r.xmq" render-html
cmp1 "$O/r.xmq" render-html --bg=dark --theme=dark+BG=#101010:FG=#eeeeee,light+BG=#fafafa
cmp1 "$O/r.xmq" render-html --onlystyle
cmp1 "$O/r.xmq" render-tex
cmp1 "$O/r.xmq" render-tex --bg=dark --theme=EK=#123456_U

if [ "$f" = "0" ]
then
    echo "OK: cli compare xmq xmqj ($n cases)"
else
    echo "ERR: cli compare xmq xmqj, $f of $n cases differ"
    exit 1
fi
