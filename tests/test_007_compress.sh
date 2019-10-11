#!/bin/bash

TEST=$(basename "$0" | sed 's/.sh//')
echo $TEST
XMQ="$1"
OUT="$2/$TEST"

rm -rf $OUT
mkdir -p $OUT

$XMQ -c tests/${TEST}.xml > $OUT/out.xmq
cat > $OUT/expected.xmq <<EOF
# 1=org.eventb.
# 0=org.eventb.core.
0:contextFile(0:configuration               = org.eventb.core.fwd
              0:generated                   = false
              1:texttools.text_lastmodified = 1566054895570
              version                       = 3)
{
    0:carrierSet(0:comment    = Comment
                 0:generated  = false
                 0:identifier = VIF_STATES)
    0:constant(0:generated  = false
               0:identifier = DecodeVIF)
    0:axiom(0:generated = false
            0:label     = axm1
            0:predicate = 'partition(VIF_STATES, {DecodeVIF}, {DecodeCombinable}, {DecodeCombinableExtension}, {DoneVIB} )')
    0:constant(0:generated  = false
               0:identifier = DecodeCombinable)
    0:constant(0:generated  = false
               0:identifier = DecodeCombinableExtension)
    0:constant(0:generated  = false
               0:identifier = DoneVIB)
}
EOF

diff $OUT/out.xmq $OUT/expected.xmq
if [ "$?" != "0" ]; then exit 1; fi
