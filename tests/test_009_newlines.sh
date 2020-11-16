#!/bin/bash

TEST=$(basename "$0" | sed 's/.sh//')
echo $TEST
XMQ="$1"
OUT="$2/$TEST"

rm -rf $OUT
mkdir -p $OUT

$XMQ tests/${TEST}.xml > $OUT/out.xmq
cat > $OUT/expected.xmq <<EOF
feature(id            = org.eventb.texteditor.feature
        label         = TextEditor
        provider-name = 'University AB')
{
    description =
    'Release History:
     ----------------------
     3.3.0 - Compatibility with core 3.3
     3.1.2 - Fix some more bugs regarding theory parsing, fix saving
     changes to seen contexts.
     3.1.1 - Fix parsing of theory operators with more than one operand'
    copyright   =
    'Copyright (c) 1971-1973 University AB
     All rights reserved.'
    url {
        update(label = 'University AB'
               url   = http://www.uniab.cde/)
        discovery(label = 'Update Site'
                  url   = file:/home/repository/)
    }
}
EOF

diff $OUT/out.xmq $OUT/expected.xmq
if [ "$?" != "0" ]; then exit 1; fi

$XMQ $OUT/out.xmq > $OUT/back.xml
diff $OUT/back.xml tests/${TEST}.xml
if [ "$?" != "0" ]; then exit 1; fi
