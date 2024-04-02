#!/bin/sh
# libxmq - Copyright 2024 Fredrik Öhrström (spdx: MIT)

PROG=$1
OUTPUT=$2
TEST_FILE=$3
TEST_NAME=$(basename $3 2> /dev/null)
TEST_NAME=${TEST_NAME%.*}

if [ -z "$OUTPUT" ] || [ -z "$TEST_FILE" ] || [ -z "$PROG" ]
then
    echo "Usage: tests/test_xsd.sh [XQ_BINARY] [OUTPUT_DIR] tests/xsd_999_foo.test"
    exit 1
fi

if [ ! -f "$TEST_FILE" ]
then
    echo "No such test file $TEST_FILE"
    exit 1
fi

mkdir -p $OUTPUT

sed -n '/^XSD.*$/,/^INPUT$/p' $TEST_FILE | tail -n +2 | sed '$d' > $OUTPUT/${TEST_NAME}.xsd
sed -n '/^INPUT$/,/^OUTPUT$/p' $TEST_FILE | tail -n +2 | sed '$d' > $OUTPUT/${TEST_NAME}.input
sed -n '/^OUTPUT$/,/^END$/p' $TEST_FILE | tail -n +2 | sed '$d' > $OUTPUT/${TEST_NAME}.expected

ARGS=$(grep ARGS $TEST_FILE | cut -b 5- | tr -d '\n')
CMDS=$(grep CMDS $TEST_FILE | cut -b 5- | tr -d '\n')

$PROG $ARGS $OUTPUT/${TEST_NAME}.input validate $OUTPUT/${TEST_NAME}.xsd $CMDS | sed 's/validated against.*/validated against .../' > $OUTPUT/${TEST_NAME}.output

if diff $OUTPUT/${TEST_NAME}.expected $OUTPUT/${TEST_NAME}.output > /dev/null
then
    echo OK: $TEST_NAME
else
    echo ERR: $TEST_NAME
    echo "Formatting differ:"
    if [ -n "$USE_MELD" ]
    then
        meld $OUTPUT/${TEST_NAME}.expected $OUTPUT/${TEST_NAME}.output
    else
        diff $OUTPUT/${TEST_NAME}.expected $OUTPUT/${TEST_NAME}.output
    fi
    exit 1
fi
