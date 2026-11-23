#!/bin/sh
# libxmq - Copyright 2024-2025 Fredrik Öhrström (spdx: MIT)

PROG=$1
OUTPUT=$2
TEST_FILE=$3
TEST_NAME=$(basename $3 2> /dev/null)
TEST_NAME=${TEST_NAME%.*}

if [ -z "$OUTPUT" ] || [ -z "$TEST_FILE" ] || [ -z "$PROG" ]
then
    echo "Usage: tests/test_ixml_not.sh [XQ_BINARY] [OUTPUT_DIR] tests/99_foo.test"
    exit 1
fi

if [ ! -f "$TEST_FILE" ]
then
    echo "No such test file $TEST_FILE"
    exit 1
fi

mkdir -p $OUTPUT

ARGS=$(grep ^ARGS $TEST_FILE | cut -b 6- | tr -d '\n')

sed -n '/^START$/,/^INPUT$/p' $TEST_FILE | tail -n +2 | sed '$d' > $OUTPUT/${TEST_NAME}.ixml
sed -n '/^INPUT$/,/^OUTPUT$/p' $TEST_FILE | tail -n +2 | sed '$d' > $OUTPUT/${TEST_NAME}.input
if [ "1" = "$(( $(wc -l < $OUTPUT/${TEST_NAME}.input) ))" ]
then
    cat $OUTPUT/${TEST_NAME}.input | tr -d '\n' > $OUTPUT/tempo
    mv $OUTPUT/tempo $OUTPUT/${TEST_NAME}.input
fi
sed -n '/^OUTPUT$/,/^END$/p' $TEST_FILE | tail -n +2 | sed '$d' > $OUTPUT/${TEST_NAME}.expected

$PROG --ixml=$OUTPUT/${TEST_NAME}.ixml $ARGS $OUTPUT/${TEST_NAME}.input > $OUTPUT/${TEST_NAME}.output 2>&1 || true

# With an external ixml engine (XMQ_IXML_ENGINE) the result is compared by meaning, eg a
# failure must be at the same position but the message is the engine's own.
if diff -q $OUTPUT/${TEST_NAME}.expected $OUTPUT/${TEST_NAME}.output > /dev/null
then
    echo "OK: test ${TEST_NAME}"
elif [ -n "$XMQ_IXML_ENGINE" ] && REASON=$(python3 tests/compare_ixml_engine.py $PROG $OUTPUT/${TEST_NAME}.expected $OUTPUT/${TEST_NAME}.output)
then
    echo "OK: test ${TEST_NAME} (same meaning with $XMQ_IXML_ENGINE)"
else
    diff $OUTPUT/${TEST_NAME}.expected $OUTPUT/${TEST_NAME}.output
    echo ERR: $TEST_NAME
    [ -n "$XMQ_IXML_ENGINE" ] && echo "$XMQ_IXML_ENGINE: $REASON"
    echo "......................................"
    cat $OUTPUT/${TEST_NAME}.ixml
    echo "......................................"
    cat $OUTPUT/${TEST_NAME}.output | sed "s|${OUTPUT}||"
    exit 1
fi

exit 0
