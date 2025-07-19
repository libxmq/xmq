#!/bin/bash
if [ ! -f $1 ]
then
    echo "No such file: $1"
    exit 1
fi

./xmq --trace=ixml. $1 $2 $3 2> temp_log
cat temp_log | grep '^(ixml' > temp_render
XMQ_THEME=mono ./xmq.working grammars/xmq-utils/parse.ixml temp_render transform grammars/xmq-utils/parse.xslq to-html br >/dev/null 2>&1
