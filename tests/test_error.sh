#!/bin/sh
# libxmq - Copyright 2023 Fredrik Öhrström (spdx: MIT)

PROG=$1
OUTPUT=$2
TEST_FILE=$3
TEST_NAME=$(basename $3 2> /dev/null)
TEST_NAME=${TEST_NAME%.*}

if [ -z "$OUTPUT" ] || [ -z "$TEST_FILE" ] || [ -z "$PROG" ]
then
    echo "Usage: tests/test_single.sh [XQ_BINARY] [OUTPUT_DIR] tests/99_foo.test"
    exit 1
fi

if [ ! -f "$TEST_FILE" ]
then
    echo "No such test file $TEST_FILE"
    exit 1
fi

mkdir -p $OUTPUT

sed -n '/^START$/,/^ERROR$/p' $TEST_FILE | tail -n +2 | sed '$d' > $OUTPUT/${TEST_NAME}.input
sed -n '/^ERROR$/,/^END$/p' $TEST_FILE | tail -n +2 | sed '$d' > $OUTPUT/${TEST_NAME}.expected_error

$PROG $OUTPUT/${TEST_NAME}.input tokenize --type=debugtokens > $OUTPUT/${TEST_NAME}.tokens 2> $OUTPUT/${TEST_NAME}.err

cat $OUTPUT/${TEST_NAME}.err | sed 's|'${OUTPUT}'|...|'  > $OUTPUT/${TEST_NAME}.error

mv $OUTPUT/${TEST_NAME}.error $OUTPUT/${TEST_NAME}.err

if diff $OUTPUT/${TEST_NAME}.expected_error $OUTPUT/${TEST_NAME}.err > /dev/null
then
    echo "OK: test ${TEST_NAME}"
else
    echo ERR: $TEST_NAME
    echo "Error differ:"
    if [ -n "$USE_MELD" ]
    then
        meld $OUTPUT/${TEST_NAME}.expected_error $OUTPUT/${TEST_NAME}.err
    else
        diff $OUTPUT/${TEST_NAME}.expected_error $OUTPUT/${TEST_NAME}.err
    fi
    exit 1
fi
