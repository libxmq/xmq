#!/bin/sh
# libxmq - Copyright 2023 Fredrik Öhrström (spdx: MIT)

PROG=$1
OUTPUT=$2
TEST_FILE=$3
TEST_NAME=$(basename $3 2> /dev/null)
TEST_NAME=${TEST_NAME%.*}

if [ -z "$OUTPUT" ] || [ -z "$TEST_FILE" ] || [ -z "$PROG" ]
then
    echo "Usage: tests/test_parse_json.sh [XQ_BINARY] [OUTPUT_DIR] tests/parse_json_999_foo.test"
    exit 1
fi

if [ ! -f "$TEST_FILE" ]
then
    echo "No such test file $TEST_FILE"
    exit 1
fi

mkdir -p $OUTPUT

sed -n '/^INPUT.*$/,/^XMQ$/p' $TEST_FILE | tail -n +2 | sed '$d' > $OUTPUT/${TEST_NAME}.input
sed -n '/^XMQ$/,/^JSON$/p' $TEST_FILE | tail -n +2 | sed '$d' > $OUTPUT/${TEST_NAME}.expected_xmq
sed -n '/^JSON$/,/^END$/p' $TEST_FILE | tail -n +2 | sed '$d' > $OUTPUT/${TEST_NAME}.expected_json

ARGS=$(grep ARGS $TEST_FILE | cut -b 5- | tr -d '\n')
CMDS=$(grep CMDS $TEST_FILE | cut -b 5- | tr -d '\n')

$PROG $ARGS $OUTPUT/${TEST_NAME}.input to-xmq > $OUTPUT/${TEST_NAME}.xmq_output

if diff $OUTPUT/${TEST_NAME}.expected_xmq $OUTPUT/${TEST_NAME}.xmq_output > /dev/null
then
    echo OK: xmq $TEST_NAME
else
    echo ERR: xmq $TEST_NAME
    echo "Formatting differ:"
    if [ -n "$USE_MELD" ]
    then
        meld $OUTPUT/${TEST_NAME}.expected_xmq $OUTPUT/${TEST_NAME}.xmq_output
    else
        diff $OUTPUT/${TEST_NAME}.expected_xmq $OUTPUT/${TEST_NAME}.xmq_output
    fi
    exit 1
fi

$PROG $ARGS $OUTPUT/${TEST_NAME}.xmq_output to-json > $OUTPUT/${TEST_NAME}.json_output

if ! jq . $OUTPUT/${TEST_NAME}.json_output >/dev/null 2>&1
then
    jq . $OUTPUT/${TEST_NAME}.json_output
    echo ERROR: $TEST_NAME
    exit 1
fi

if diff $OUTPUT/${TEST_NAME}.expected_json $OUTPUT/${TEST_NAME}.json_output > /dev/null
then
    echo OK: json $TEST_NAME
else
    echo ERR: json $TEST_NAME
    echo "Formatting differ:"
    if [ -n "$USE_MELD" ]
    then
        meld $OUTPUT/${TEST_NAME}.expected_json $OUTPUT/${TEST_NAME}.json_output
    else
        diff $OUTPUT/${TEST_NAME}.expected_json $OUTPUT/${TEST_NAME}.json_output
    fi
    exit 1
fi

$PROG $ARGS $OUTPUT/${TEST_NAME}.json_output > $OUTPUT/${TEST_NAME}.xmq_back

if diff $OUTPUT/${TEST_NAME}.expected_xmq $OUTPUT/${TEST_NAME}.xmq_back > /dev/null
then
    echo OK: xmq back from json $TEST_NAME
else
    echo ERR: xmq back from json $TEST_NAME
    echo "Formatting differ when returning from json to xmq:"
    if [ -n "$USE_MELD" ]
    then
        meld $OUTPUT/${TEST_NAME}.expected_xmq $OUTPUT/${TEST_NAME}.xmq_back
    else
        diff $OUTPUT/${TEST_NAME}.expected_xmq $OUTPUT/${TEST_NAME}.xmq_back
    fi
    exit 1
fi
