#!/bin/sh
# libxmq - Copyright 2024 Fredrik Öhrström (spdx: MIT)

PROG=$1
OUTPUT=$2
TEST_FILE=$3
TEST_NAME=$(basename $3 2> /dev/null)
TEST_NAME=${TEST_NAME%.*}

if [ -z "$OUTPUT" ] || [ -z "$TEST_FILE" ] || [ -z "$PROG" ]
then
    echo "Usage: tests/test_ixml_parse.sh [XQ_BINARY] [OUTPUT_DIR] tests/99_foo.test"
    exit 1
fi

if [ ! -f "$TEST_FILE" ]
then
    echo "No such test file $TEST_FILE"
    exit 1
fi

mkdir -p $OUTPUT

ARGS=$(grep ^ARGS $TEST_FILE | cut -b 6- | tr -d '\n')
ONLY_INTERNAL_ENGINE=$(grep ^ONLY_INTERNAL_ENGINE $TEST_FILE)
# Optional commands after the input, eg CMDS=to-xml, and the expected exit code, eg EXIT=1.
CMDS=$(grep ^CMDS= $TEST_FILE | cut -b 6- | tr -d '\n')
EXIT=$(grep ^EXIT= $TEST_FILE | cut -b 6- | tr -d '\n')
EXIT=${EXIT:-any}

if [ -n "$XMQ_IXML_ENGINE" ] && [ "$ONLY_INTERNAL_ENGINE" = "ONLY_INTERNAL_ENGINE=true" ]
then
    # This test can only be run with xmq internal ixml parser.
    # An external IXML engine has been set, so skip this test.
    echo "SKIP: test ${TEST_NAME} (only the internal ixml engine)"
    exit 0
fi
sed -n '/^START$/,/^INPUT$/p' $TEST_FILE | tail -n +2 | sed '$d' > $OUTPUT/${TEST_NAME}.ixml
sed -n '/^INPUT$/,/^OUTPUT$/p' $TEST_FILE | tail -n +2 | sed '$d' | tr -d '\n' > $OUTPUT/${TEST_NAME}.input
sed -n '/^OUTPUT$/,/^END$/p' $TEST_FILE | tail -n +2 | sed '$d' > $OUTPUT/${TEST_NAME}.expected

$PROG --ixml=$OUTPUT/${TEST_NAME}.ixml $ARGS $OUTPUT/${TEST_NAME}.input $CMDS > $OUTPUT/${TEST_NAME}.output 2>/dev/null
RC=$?

# With an external ixml engine the exit code depends on how the engine reports a failure
# (eg as text instead of a failure document), the compare by meaning below decides.
if [ "$EXIT" != "any" ] && [ "$RC" != "$EXIT" ] && [ -z "$XMQ_IXML_ENGINE" ]
then
    echo "ERR: $TEST_NAME exit code $RC expected $EXIT"
    exit 1
fi

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
