#!/bin/bash

TEST=$(basename "$0" | sed 's/.sh//')
XMQ="$1"
OUT="$2/$TEST"

rm -rf $OUT
mkdir -p $OUT

$XMQ --sort-attributes tests/${TEST}.xml > $OUT/out.xmq
cat > $OUT/expected.xmq <<EOF
alfa(aa       = 1
     aaa      = 2
     def      = 3
     deffffff = 4
     xyz      = 5
     z        = 6)
EOF

diff $OUT/out.xmq $OUT/expected.xmq
if [ "$?" != "0" ]
then
    echo ERROR $TEST
    exit 1
fi

$XMQ $OUT/out.xmq > $OUT/back.xml
if [ "$?" != "0" ]
then
    echo ERROR $TEST
    exit 1
fi

echo OK $TEST

# We expect a difference since we sorted the attributes.
# So lets not compare.
