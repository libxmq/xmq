#!/bin/sh
# libxmq - Copyright 2026 Fredrik Öhrström (spdx: MIT)

PROG=$1
OUTPUT=$2
TEST_NAME="Test special 007 --no-final-nl for json."

if [ -z "$OUTPUT" ] || [ -z "$PROG" ]
then
    echo "Usage: tests/test_special....sh [XMQ_BINARY] [OUTPUT_DIR]"
    exit 1
fi

mkdir -p $OUTPUT

echo ' { "howdy"        :123 }' | $PROG - to-json -c -n > $OUTPUT/output.json

if [ -s $OUTPUT/output.json ] && [ "$(tail -c 1 $OUTPUT/output.json | od -An -tu1)" != "  10" ]; then
    echo OK: $TEST_NAME
else
    echo ERR: $TEST_NAME
    echo "No final newline found..."
    exit 1
fi

TEST_NAME="Test special 007 --no-final-nl for xmq."

echo ' { "howdy"        :123 }' | $PROG - to-xmq -c -n > $OUTPUT/output.xmq

if [ -s $OUTPUT/output.xmq ] && [ "$(tail -c 1 $OUTPUT/output.xmq | od -An -tu1)" != "  10" ]; then
    echo OK: $TEST_NAME
else
    echo ERR: $TEST_NAME
    echo "No final newline found..."
    exit 1
fi
