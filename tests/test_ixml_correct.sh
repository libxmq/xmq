#!/bin/sh
# libxmq - Copyright 2024 Fredrik Öhrström (spdx: MIT)

PROG=$1
OUTPUT=$2
TEST_FILE=$3
TEST_DIR=$(dirname $3)
TEST_NAME=$(basename $3 2> /dev/null)
TEST_NAME=${TEST_NAME%%.*}

if [ -z "$OUTPUT" ] || [ -z "$TEST_FILE" ] || [ -z "$PROG" ]
then
    echo "Usage: tests/test_ixml_correct.sh [XQ_BINARY] [OUTPUT_DIR] tests/ixml/correct/aaa.output.xmq"
    exit 1
fi

if [ ! -f "$TEST_FILE" ]
then
    echo "No such test file $TEST_FILE"
    exit 1
fi

mkdir -p $OUTPUT

$PROG --ixml=$TEST_DIR/${TEST_NAME}.ixml $TEST_DIR/${TEST_NAME}.inp > $OUTPUT/${TEST_NAME}.output 2>/dev/null

if diff $TEST_DIR/${TEST_NAME}.output.xmq  $OUTPUT/${TEST_NAME}.output
then
    echo "OK: test ixml xmq correct ${TEST_NAME}"
else
    echo ERR: $TEST_NAME
    echo "......................................"
    cat $TEST_DIR/${TEST_NAME}.ixml
    echo "......................................"
    cat $OUTPUT/${TEST_NAME}.output | sed "s|${OUTPUT}||"
    exit 1
fi

$PROG $TEST_DIR/${TEST_NAME}.output.xmq to-xml -o > $OUTPUT/${TEST_NAME}.output.xml 2>/dev/null
$PROG --trim=none $TEST_DIR/${TEST_NAME}.output.xml to-xml -o > $OUTPUT/${TEST_NAME}.output2.xml 2>/dev/null

if diff $OUTPUT/${TEST_NAME}.output.xml  $OUTPUT/${TEST_NAME}.output2.xml
then
    echo "OK: test ixml xml correct ${TEST_NAME}"
else
    echo ERR: $TEST_NAME
    echo "......................................"
    cat $TEST_DIR/${TEST_NAME}.output.xml
    echo "......................................"
    cat $OUTPUT/${TEST_NAME}.output2.xml | sed "s|${OUTPUT}||"
    exit 1
fi
