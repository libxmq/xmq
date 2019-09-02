#!/bin/bash

TEST=$(basename "$0" | sed 's/.sh//')
echo $TEST
XMQ="$1"
OUT="$2/$TEST"

rm -rf $OUT
mkdir -p $OUT

$XMQ tests/${TEST}.xml > $OUT/out.xmq
cat > $OUT/expected.xmq <<EOF
a {
    b {
        c = CCC
        d
        e
    }
}
EOF

diff $OUT/out.xmq $OUT/expected.xmq
if [ "$?" != "0" ]; then exit 1; fi

$XMQ $OUT/out.xmq > $OUT/back.xml
cat > $OUT/expectedback.xml <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<a>
	<b>
		<c>CCC</c>
		<d/>
		<e/>
	</b>
</a>

EOF

diff $OUT/back.xml $OUT/expectedback.xml
if [ "$?" != "0" ]; then exit 1; fi
