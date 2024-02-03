#!/bin/sh
# libxmq - Copyright 2023 Fredrik Öhrström (spdx: MIT)

PROG=$1
OUTPUT=$2
TEST_FILE=$3
TEST_NAME=$(basename $3 2> /dev/null)
TEST_NAME=${TEST_NAME%.*}

if [ -z "$OUTPUT" ] || [ -z "$TEST_FILE" ] || [ -z "$PROG" ]
then
    echo "Usage: tests/test_statistics.sh [XQ_BINARY] [OUTPUT_DIR] tests/statistics_99_foo.test"
    exit 1
fi

if [ ! -f "$TEST_FILE" ]
then
    echo "No such test file $TEST_FILE"
    exit 1
fi

mkdir -p $OUTPUT

sed -n '/^START.*$/,/^STATISTICS$/p' $TEST_FILE | tail -n +2 | sed '$d' > $OUTPUT/${TEST_NAME}.input
sed -n '/^STATISTICS$/,/^END$/p' $TEST_FILE | tail -n +2 | sed '$d' > $OUTPUT/${TEST_NAME}.expected

ARGS=$(grep ARGS $TEST_FILE | cut -b 5- | tr -d '\n')
CMDS=$(grep CMDS $TEST_FILE | cut -b 5- | tr -d '\n')

if [ "$CMDS" = "" ]
then
    CMDS="statistics"
fi

$PROG $ARGS $OUTPUT/${TEST_NAME}.input $CMDS > $OUTPUT/${TEST_NAME}.output

if diff $OUTPUT/${TEST_NAME}.expected $OUTPUT/${TEST_NAME}.output > /dev/null
then
    echo OK: $TEST_NAME
else
    echo ERR: $TEST_NAME
    echo "Statistics differ:"
    if [ -n "$USE_MELD" ]
    then
        meld $OUTPUT/${TEST_NAME}.expected $OUTPUT/${TEST_NAME}.output
    else
        diff $OUTPUT/${TEST_NAME}.expected $OUTPUT/${TEST_NAME}.output
    fi
    exit 1
fi
