#!/bin/sh
# libxmq - Copyright 2025 Fredrik Öhrström (spdx: MIT)

PROG=$1
OUTPUT=$2
TEST_NAME="Test pretty json print is the same for jq and xmq."

if [ -z "$OUTPUT" ] || [ -z "$PROG" ]
then
    echo "Usage: tests/test_special....sh [XMQ_BINARY] [OUTPUT_DIR]"
    exit 1
fi

mkdir -p $OUTPUT

$PROG pom.xml to-json -p > $OUTPUT/output.json
jq . $OUTPUT/output.json > $OUTPUT/jqoutput.json

if diff $OUTPUT/output.json $OUTPUT/jqoutput.json > /dev/null
then
    echo OK: $TEST_NAME
else
    echo ERR: $TEST_NAME
    echo "Formatting differ:"
    if [ -n "$USE_MELD" ]
    then
        meld $OUTPUT/output.json $OUTPUT/jqoutput.json
    else
        diff $OUTPUT/output.json $OUTPUT/jqoutput.json
    fi
    exit 1
fi
