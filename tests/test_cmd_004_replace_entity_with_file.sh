#!/bin/sh
# libxmq - Copyright 2026 Fredrik Öhrström (spdx: MIT)
# replace-entity --with-file NAME FILE and replace-entity --with-text-file NAME FILE

PROG=$1
OUTPUT=$2

if [ -z "$OUTPUT" ] || [ -z "$PROG" ]
then
    echo "Usage: tests/test_cmd_....sh [XMQ_BINARY] [OUTPUT_DIR]"
    exit 1
fi

mkdir -p $OUTPUT

echo "div{&dom; p=&text;}" > $OUTPUT/with_input.xmq
echo "b=bold" > $OUTPUT/with_dom.xmq
printf 'a<b & c' > $OUTPUT/with_text.txt

$PROG $OUTPUT/with_input.xmq replace-entity --with-file dom $OUTPUT/with_dom.xmq \
      replace-entity --with-text-file text $OUTPUT/with_text.txt to-xmq --compact > $OUTPUT/with_output.xmq 2>&1

echo "div{b=bold p='a<b & c'}" > $OUTPUT/with_expected.xmq

if diff $OUTPUT/with_expected.xmq $OUTPUT/with_output.xmq
then
    echo "OK: test cmd 004 replace-entity --with-file"
else
    echo "ERROR: test cmd 004 replace-entity --with-file"
    exit 1
fi
