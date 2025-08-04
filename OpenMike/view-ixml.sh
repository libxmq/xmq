#!/bin/bash
if [ ! -f $1 ]
then
    echo "No such file: $1"
    exit 1
fi
xmq --debug=ixml. $1 $2 $3 2> temp_log
cat temp_log | grep '^(ixml' | grep -v " none/" | grep -v "^(ixml....info)" > temp_render
cat temp_render | XMQ_THEME=mono xmq parse-trace.ixml > temp_xmq
cat temp_xmq | xmq transform render-trace.xslq --stringparam=level=view to-html br >/dev/null 2>&1
