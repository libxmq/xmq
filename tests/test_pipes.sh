#!/bin/sh
# libxmq - Copyright 2024 Fredrik Öhrström (spdx: MIT)

PROG=$1
OUTPUT=$2
TEST_FILE=$3
TEST_NAME=$(basename $3 2> /dev/null)
TEST_NAME=${TEST_NAME%.*}

if [ -z "$OUTPUT" ] || [ -z "$TEST_FILE" ] || [ -z "$PROG" ]
then
    echo "Usage: tests/test_pipes.sh [XQ_BINARY] [OUTPUT_DIR] tests/pipe_001_foo.test"
    exit 1
fi

if [ ! -f "$TEST_FILE" ]
then
    echo "No such test file $TEST_FILE"
    exit 1
fi

mkdir -p $OUTPUT

sed -n '/^START$/,/^DEBUG$/p' $TEST_FILE | tail -n +2 | sed '$d' > $OUTPUT/${TEST_NAME}.input
sed -n '/^DEBUG$/,/^CONTENT$/p' $TEST_FILE | tail -n +2 | sed '$d' > $OUTPUT/${TEST_NAME}.expected_tokens
sed -n '/^CONTENT$/,/^END$/p' $TEST_FILE | tail -n +2 | sed '$d' > $OUTPUT/${TEST_NAME}.expected_content


cat $OUTPUT/${TEST_NAME}.input | $PROG tokenize --type=debugtokens > $OUTPUT/${TEST_NAME}.tokens
cat $OUTPUT/${TEST_NAME}.input | $PROG tokenize --type=debugcontent > $OUTPUT/${TEST_NAME}.content

if diff $OUTPUT/${TEST_NAME}.expected_tokens $OUTPUT/${TEST_NAME}.tokens > /dev/null
then
    true
else
    echo ERR: $TEST_NAME
    echo "Tokens differ:"
    if [ -n "$USE_MELD" ]
    then
        meld $OUTPUT/${TEST_NAME}.expected_tokens $OUTPUT/${TEST_NAME}.tokens
    else
        diff $OUTPUT/${TEST_NAME}.expected_tokens $OUTPUT/${TEST_NAME}.tokens
    fi
    exit 1
fi

if diff $OUTPUT/${TEST_NAME}.expected_content $OUTPUT/${TEST_NAME}.content > /dev/null
then
    echo OK: $TEST_NAME
else
    echo ERR: $TEST_NAME
    echo "Values/quotes content differ:"
    if [ -n "$USE_MELD" ]
    then
        meld $OUTPUT/${TEST_NAME}.expected_content $OUTPUT/${TEST_NAME}.content
    else
        diff $OUTPUT/${TEST_NAME}.expected_content $OUTPUT/${TEST_NAME}.content
    fi
    exit 1
fi
