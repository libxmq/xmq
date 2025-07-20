#!/bin/bash
./xmq --debug=ixml. $1 $2 $3 2> >(grep '^(ixml' | grep -v " pred/" | grep -v "^(ixml.pa.info)" > temp_render)
XMQ_THEME=mono ./xmq.working grammars/xmq-utils/parse.ixml temp_render transform grammars/xmq-utils/parse.xslq to-html br >/dev/null 2>&1
