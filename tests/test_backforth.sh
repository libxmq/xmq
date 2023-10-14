#!/bin/sh
# libxmq - Copyright 2023 Fredrik Öhrström (spdx: MIT)

PROG=$1
OUTPUT=$2
TEST_FILE=$3
TEST_NAME=$(basename $3 2> /dev/null)
TEST_NAME=${TEST_NAME%.*}

if [ -z "$OUTPUT" ] || [ -z "$TEST_FILE" ] || [ -z "$PROG" ]
then
    echo "Usage: tests/test_backforth.sh [XQ_BINARY] [OUTPUT_DIR] tests/99_foo.test"
    exit 1
fi

if [ ! -f "$TEST_FILE" ]
then
    echo "No such test file $TEST_FILE"
    exit 1
fi

mkdir -p $OUTPUT

sed -n '/^INPUT.*$/,/^FIRST$/p' $TEST_FILE | tail -n +2 | sed '$d' > $OUTPUT/${TEST_NAME}.input
sed -n '/^FIRST$/,/^SECOND$/p' $TEST_FILE | tail -n +2 | sed '$d' > $OUTPUT/${TEST_NAME}.first_expected
sed -n '/^SECOND$/,/^END$/p' $TEST_FILE | tail -n +2 | sed '$d' > $OUTPUT/${TEST_NAME}.second_expected

ARGS_FIRST=$(grep ARGS_FIRST $TEST_FILE | cut -b 11- | tr -d '\n')
CMDS_FIRST=$(grep CMDS_FIRST $TEST_FILE | cut -b 11- | tr -d '\n')

ARGS_SECOND=$(grep ARGS_SECOND $TEST_FILE | cut -b 12- | tr -d '\n')
CMDS_SECOND=$(grep CMDS_SECOND $TEST_FILE | cut -b 12- | tr -d '\n')

$PROG $ARGS_FIRST $OUTPUT/${TEST_NAME}.input $CMDS_FIRST \
    | tee $OUTPUT/${TEST_NAME}.first \
    | $PROG $ARGS_SECOND - $CMDS_SECOND \
            > $OUTPUT/${TEST_NAME}.second

if diff $OUTPUT/${TEST_NAME}.first_expected $OUTPUT/${TEST_NAME}.first > /dev/null
then
    echo OK: FORTH $TEST_NAME
else
    echo ERR: $TEST_NAME
    echo "Formatting first differ:"
    if [ -n "$USE_MELD" ]
    then
        meld $OUTPUT/${TEST_NAME}.first_expected $OUTPUT/${TEST_NAME}.first
    else
        diff $OUTPUT/${TEST_NAME}.first_expected $OUTPUT/${TEST_NAME}.first
    fi
    exit 1
fi

if diff $OUTPUT/${TEST_NAME}.second_expected $OUTPUT/${TEST_NAME}.second > /dev/null
then
    echo OK: BACK $TEST_NAME
else
    echo ERR: $TEST_NAME
    echo "Formatting second differ:"
    if [ -n "$USE_MELD" ]
    then
        meld $OUTPUT/${TEST_NAME}.second_expected $OUTPUT/${TEST_NAME}.second
    else
        diff $OUTPUT/${TEST_NAME}.second_expected $OUTPUT/${TEST_NAME}.second
    fi
    exit 1
fi
