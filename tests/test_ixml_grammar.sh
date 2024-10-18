#!/bin/sh
# libxmq - Copyright 2024 Fredrik Öhrström (spdx: MIT)

PROG=$1
OUTPUT=$2
TEST_FILE=$3
TEST_NAME=$(basename $3 2> /dev/null)
TEST_NAME=${TEST_NAME%.*}

if [ -z "$OUTPUT" ] || [ -z "$TEST_FILE" ] || [ -z "$PROG" ]
then
    echo "Usage: tests/test_ixml_grammar.sh [XQ_BINARY] [OUTPUT_DIR] tests/99_foo.test"
    exit 1
fi

if [ ! -f "$TEST_FILE" ]
then
    echo "No such test file $TEST_FILE"
    exit 1
fi

mkdir -p $OUTPUT

sed -n '/^START$/,/^END$/p' $TEST_FILE | tail -n +2 | sed '$d' > $OUTPUT/${TEST_NAME}.ixml


if $PROG --ixml=$OUTPUT/${TEST_NAME}.ixml > $OUTPUT/${TEST_NAME}.output 2>&1
then
    echo "OK: test ${TEST_NAME}"
else
    echo ERR: $TEST_NAME
    echo "......................................"
    cat $OUTPUT/${TEST_NAME}.ixml
    echo "......................................"
    cat $OUTPUT/${TEST_NAME}.output | sed "s|${OUTPUT}||"
    exit 1
fi
