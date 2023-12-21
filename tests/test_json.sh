#!/bin/sh
# libxmq - Copyright 2023 Fredrik Öhrström (spdx: MIT)

PROG=$1
OUTPUT=$2
TEST_FILE=$3
TEST_NAME=$(basename $3 2> /dev/null)
TEST_NAME=${TEST_NAME%.*}

if [ -z "$OUTPUT" ] || [ -z "$TEST_FILE" ] || [ -z "$PROG" ]
then
    echo "Usage: tests/test_jsonh [XQ_BINARY] [OUTPUT_DIR] tests/json_999_foo.test"
    exit 1
fi

if [ ! -f "$TEST_FILE" ]
then
    echo "No such test file $TEST_FILE"
    exit 1
fi

mkdir -p $OUTPUT

sed -n '/^INPUT.*$/,/^OUTPUT$/p' $TEST_FILE | tail -n +2 | sed '$d' > $OUTPUT/${TEST_NAME}.input
sed -n '/^OUTPUT$/,/^COMPACT$/p' $TEST_FILE | tail -n +2 | sed '$d' > $OUTPUT/${TEST_NAME}.expected
sed -n '/^COMPACT$/,/^END$/p' $TEST_FILE | tail -n +2 | sed '$d' > $OUTPUT/${TEST_NAME}.expected_compact

ARGS=$(grep ARGS $TEST_FILE | cut -b 5- | tr -d '\n')
CMDS=$(grep CMDS $TEST_FILE | cut -b 5- | tr -d '\n')

if [ "$CMDS" = "" ]
then
    CMDS="to-json"
fi

$PROG $ARGS $OUTPUT/${TEST_NAME}.input $CMDS > $OUTPUT/${TEST_NAME}.output
if ! jq . $OUTPUT/${TEST_NAME}.output >/dev/null 2>&1
then
    jq . $OUTPUT/${TEST_NAME}.output
    echo ERROR: $TEST_NAME
    exit 1
fi

if [ "$(cat $OUTPUT/${TEST_NAME}.expected_compact)" != "IGNORE" ]
then
    $PROG $ARGS $OUTPUT/${TEST_NAME}.input $CMDS --compact > $OUTPUT/${TEST_NAME}.output_compact
    if ! jq . $OUTPUT/${TEST_NAME}.output_compact >/dev/null 2>&1
    then
        jq . $OUTPUT/${TEST_NAME}.output_compact
        echo ERROR: compact $TEST_NAME
        exit 1
    fi
else
    cp $OUTPUT/${TEST_NAME}.expected_compact $OUTPUT/${TEST_NAME}.output_compact
fi

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

if diff $OUTPUT/${TEST_NAME}.expected_compact $OUTPUT/${TEST_NAME}.output_compact > /dev/null
then
    echo OK: $TEST_NAME compact
else
    echo ERR: $TEST_NAME compact
    echo "Formatting differ:"
    if [ -n "$USE_MELD" ]
    then
        meld $OUTPUT/${TEST_NAME}.expected_compact $OUTPUT/${TEST_NAME}.output_compact
    else
        diff $OUTPUT/${TEST_NAME}.expected_compact $OUTPUT/${TEST_NAME}.output_compact
    fi
    exit 1
fi

# XMQ output was as expected.
# XMQ compact output was as expected.

# Now expand the compact output to normal xmq output and compare with the previous output.
# Now compact the previous output and compare with the previous compact output.

exit 0

$PROG $ARGS $OUTPUT/${TEST_NAME}.output_compact > $OUTPUT/${TEST_NAME}.output_expanded
$PROG $ARGS $OUTPUT/${TEST_NAME}.output to-xmq --compact > $OUTPUT/${TEST_NAME}.output_compressed

if diff $OUTPUT/${TEST_NAME}.output $OUTPUT/${TEST_NAME}.output_expanded > /dev/null
then
    echo OK: $TEST_NAME
else
    echo ERR: $TEST_NAME
    echo "Formatting differ:"
    if [ -n "$USE_MELD" ]
    then
        meld $OUTPUT/${TEST_NAME}.output $OUTPUT/${TEST_NAME}.output_expanded
    else
        diff $OUTPUT/${TEST_NAME}.output $OUTPUT/${TEST_NAME}.output_expanded
    fi
    exit 1
fi

if diff $OUTPUT/${TEST_NAME}.output_compact $OUTPUT/${TEST_NAME}.output_compressed > /dev/null
then
    echo OK: $TEST_NAME
else
    echo ERR: $TEST_NAME
    echo "Formatting differ:"
    if [ -n "$USE_MELD" ]
    then
        meld $OUTPUT/${TEST_NAME}.output_compact $OUTPUT/${TEST_NAME}.output_compressed
    else
        diff $OUTPUT/${TEST_NAME}.output_compact $OUTPUT/${TEST_NAME}.output_compressed
    fi
    exit 1
fi
