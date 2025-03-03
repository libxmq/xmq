#!/bin/sh
# libxmq - Copyright 2025 Fredrik Öhrström (spdx: MIT)

PROG=$1
OUTPUT=$2
TEST_NAME=$(basename $1 2> /dev/null)
TEST_NAME=${TEST_NAME%.*}

if [ -z "$OUTPUT" ] || [ -z "$PROG" ]
then
    echo "Usage: tests/test_special....sh [XMQ_BINARY] [OUTPUT_DIR]"
    exit 1
fi

mkdir -p $OUTPUT

cat > $OUTPUT/input.xml <<EOF
<root>
  <child>Hello Homebrew!</child>
</root>
EOF

cat > $OUTPUT/expected.xmq <<EOF
child = 'Hello Homebrew!'
EOF

$PROG $OUTPUT/input.xml select //child > $OUTPUT/output.xmq
RC="$?"

if [ "$?" = "0" ]
then
    echo "OK: test special 002 exit code"
else
    echo "ERROR: test special 002 exit code, expected 0 as exit code but got $RC"
    exit 1
fi

if diff $OUTPUT/output.xmq $OUTPUT/expected.xmq
then
    echo "OK: test special 002 xmq output"
else
    echo "ERROR: test special 002 xmq"
    echo "Formatting differ:"
    if [ -n "$USE_MELD" ]
    then
        meld $OUTPUT/expected.xmq $OUTPUT/output.xmq
    else
        diff $OUTPUT/expected.xmq $OUTPUT/output.xmq
    fi
    exit 1
fi
