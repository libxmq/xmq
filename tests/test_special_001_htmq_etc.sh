#!/bin/sh
# libxmq - Copyright 2023 Fredrik Öhrström (spdx: MIT)

PROG=$1
OUTPUT=$2
TEST_NAME=$(basename $1 2> /dev/null)
TEST_NAME=${TEST_NAME%.*}

if [ -z "$OUTPUT" ] || [ -z "$PROG" ]
then
    echo "Usage: tests/test_special....sh [XMQ_BINARY] [OUTPUT_DIR]"
    exit 1
fi

mkdir -p $OUTPUT

echo "c = Howdy" > $OUTPUT/input.xmq
$PROG $OUTPUT/input.xmq render_html --darkbg > $OUTPUT/output.1.html

$PROG $OUTPUT/input.xmq render_html --darkbg \
    | $PROG --trim=none to_xmq --compact \
    | $PROG to_html > $OUTPUT/output.2.html

if diff $OUTPUT/output.1.html $OUTPUT/output.2.html
then
    echo "OK: xmq -> html -> compact xmq -> html"
else
    echo "ERROR: xmq -> html -> compact xmq -> html"
    exit 1
fi
