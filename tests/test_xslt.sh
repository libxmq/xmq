#!/bin/sh
# libxmq - Copyright 2023 Fredrik Öhrström (spdx: MIT)

PROG=$1
OUTPUT=$2
TEST_FILE=$3
TEST_NAME=$(basename $3 2> /dev/null)
TEST_NAME=${TEST_NAME%.*}

if [ -z "$OUTPUT" ] || [ -z "$TEST_FILE" ] || [ -z "$PROG" ]
then
    echo "Usage: tests/test_xslt.sh [XQ_BINARY] [OUTPUT_DIR] tests/xslt_999_foo.test"
    exit 1
fi

if [ ! -f "$TEST_FILE" ]
then
    echo "No such test file $TEST_FILE"
    exit 1
fi

mkdir -p $OUTPUT

sed -n '/^XML.*$/,/^XSLT$/p' $TEST_FILE | tail -n +2 | sed '$d' > $OUTPUT/${TEST_NAME}.xml
sed -n '/^XSLT$/,/^EXPECTED$/p' $TEST_FILE | tail -n +2 | sed '$d' > $OUTPUT/${TEST_NAME}.xslt
sed -n '/^EXPECTED$/,/^END$/p' $TEST_FILE | tail -n +2 | sed '$d' > $OUTPUT/${TEST_NAME}.expected
CMDS=$(grep CMDS $TEST_FILE | cut -b 5- | tr -d '\n')

$PROG $ARGS $OUTPUT/${TEST_NAME}.xml transform $OUTPUT/${TEST_NAME}.xslt $CMDS > $OUTPUT/${TEST_NAME}.output

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
