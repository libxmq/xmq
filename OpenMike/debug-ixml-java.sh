#!/bin/bash
# Like debug-ixml.sh but parses with the Java Earley parser (org.libxmq.imp.ixml.earley),
# its trace has the same format as xmq --debug=ixml. Needs make build/xmqj.
# Usage: ./debug-ixml-java.sh grammar.ixml input.txt
#        ./debug-ixml-java.sh grammar.ixml -i 'input'
if [ ! -f $1 ]
then
    echo "No such file: $1"
    exit 1
fi
CLASSES=$(cd "$(dirname "$0")/.." && pwd)/build/classes
java -cp "$CLASSES" org.libxmq.imp.ixml.earley.Earley --trace $1 $2 "$3" 2> temp_log
cat temp_log | grep '^(ixml' | grep -v "^(ixml....info)" > temp_render
cat temp_render | XMQ_THEME=mono xmq parse-trace.ixml - > temp_xmq
cat temp_xmq | xmq transform --stringparam=level=debug render-trace.xslq to-html br >/dev/null 2>&1
