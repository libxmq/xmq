#!/bin/bash
./xmq --trace=ixml. $1 $2 $3 2> >(grep '^(ixml' | tee temp_render | XMQ_THEME=mono ./xmq.working grammars/xmq-utils/parse.ixml transform grammars/xmq-utils/parse.xslq to-html br >/dev/null 2>&1)
