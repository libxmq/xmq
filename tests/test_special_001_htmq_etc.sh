#!/bin/sh
# libxmq - Copyright 2023-2024 Fredrik Öhrström (spdx: MIT)

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
$PROG $OUTPUT/input.xmq render-html --theme=darkbg > $OUTPUT/output.rendered.html

$PROG $OUTPUT/input.xmq render-html --theme=darkbg | $PROG to-xmq --compact | $PROG to-html > $OUTPUT/output.rerendered.html

$PROG $OUTPUT/output.rendered.html > $OUTPUT/output.rendered.htmq
$PROG $OUTPUT/output.rerendered.html > $OUTPUT/output.rerendered.htmq

if diff $OUTPUT/output.rendered.htmq $OUTPUT/output.rerendered.htmq
then
    echo "OK: test special 001 xmq -> html -> compact xmq -> html"
else
    echo "ERROR: test special 001 xmq -> html -> compact xmq -> html"
    echo "Formatting differ:"
    if [ -n "$USE_MELD" ]
    then
        meld $OUTPUT/output.rendered.htmq $OUTPUT/output.rerendered.htmq
    else
        diff $OUTPUT/output.rendered.htmq $OUTPUT/output.rerendered.htmq
    fi
    exit 1
fi
