#!/bin/bash

TEST=$(basename "$0" | sed 's/.sh//')
echo $TEST
TOHR="$1"
TOXML="$2"
OUT="$3/$TEST"

rm -rf $OUT
mkdir -p $OUT

$TOHR tests/${TEST}.xml > $OUT/out.xmq
cat > $OUT/expected.xmq <<EOF
a {
    // comment2
    b {
        // comment3
        c = CCC
        // comment4
        d
        // comment5
        e
        // comment6
    }
    // comment7
}
EOF

diff $OUT/out.xmq $OUT/expected.xmq
if [ "$?" != "0" ]; then exit 1; fi

$TOXML $OUT/out.xmq > $OUT/back.xml
cat > $OUT/expectedback.xml <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<a>
	<!--comment2-->
	<b>
		<!--comment3-->
		<c>CCC</c>
		<!--comment4-->
		<d/>
		<!--comment5-->
		<e/>
		<!--comment6-->
	</b>
	<!--comment7-->
</a>

EOF

diff $OUT/back.xml $OUT/expectedback.xml
if [ "$?" != "0" ]; then exit 1; fi
