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

if [ "$f" = "0" ]
then
    echo "OK: cli compare xmq xmqj ($n cases)"
else
    echo "ERR: cli compare xmq xmqj, $f of $n cases differ"
    exit 1
fi
