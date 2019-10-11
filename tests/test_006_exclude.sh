#!/bin/bash

TEST=$(basename "$0" | sed 's/.sh//')
echo $TEST
XMQ="$1"
OUT="$2/$TEST"

rm -rf $OUT
mkdir -p $OUT

$XMQ -x sub@name tests/${TEST}.xml > $OUT/out.xmq
cat > $OUT/expected.xmq <<EOF
info {
    sub() = Svejsan
}
EOF

diff $OUT/out.xmq $OUT/expected.xmq
if [ "$?" != "0" ]; then exit 1; fi

$XMQ $OUT/out.xmq > $OUT/back.xml

diff $OUT/back.xml tests/${TEST}.back.xml
if [ "$?" != "0" ]; then exit 1; fi
