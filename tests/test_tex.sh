#!/bin/bash

PROG="$1"
OUTPUT="$2"

mkdir -p $OUTPUT

xmq render-tex > $OUTPUT/tjo.tex <<EOF
work { data(i=2) { foo bar = '123' } }
EOF

(cd $OUTPUT; xelatex -interaction=batchmode -halt-on-error tjo.tex > texx.log 2>&1)

if [ "$?" != "0" ]
then
    cat $OUTPUT/texx.log
    echo "ERR: test tex"
    exit 1
else
    echo "OK: test tex"
fi
