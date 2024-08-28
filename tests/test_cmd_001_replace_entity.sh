#!/bin/sh
# libxmq - Copyright 2024 Fredrik Öhrström (spdx: MIT)

PROG=$1
OUTPUT=$2
TEST_NAME=$(basename $1 2> /dev/null)
TEST_NAME=${TEST_NAME%.*}

if [ -z "$OUTPUT" ] || [ -z "$PROG" ]
then
    echo "Usage: tests/test_cmd_....sh [XMQ_BINARY] [OUTPUT_DIR]"
    exit 1
fi

mkdir -p $OUTPUT

echo "div(id=&id; style=('color:'&color;)){'>'&welcome;'<'}" > $OUTPUT/input.xmq

$PROG $OUTPUT/input.xmq replace-entity id 4711 replace-entity color red replace-entity welcome "Welcome stranger!" to-xmq --compact > $OUTPUT/output.xmq

echo "div(id=4711 style=color:red)='>Welcome stranger!<'" > $OUTPUT/expected_output.xmq

if diff $OUTPUT/expected_output.xmq $OUTPUT/output.xmq
then
    echo "OK: test cmd 001 replace-entity"
else
    echo "ERROR: test cmd 001 replace-entity"
    echo "Formatting differ:"
    if [ -n "$USE_MELD" ]
    then
        meld $OUTPUT/expected_output.xnq $OUTPUT/output.xmq
    else
        diff $OUTPUT/expected_output.xmq $OUTPUT/output.xmq
    fi
    exit 1
fi
