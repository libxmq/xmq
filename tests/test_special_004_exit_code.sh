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

testok() {
    if [ "$1" != "0" ]
    then
        echo "ERROR: test special 003 expected succes for $2"
        exit 1
    fi
}

testerr() {
    if [ "$1" = "0" ]
    then
        echo "ERROR: test special 003 expected fail for $2"
        exit 1
    fi
}

mkdir -p $OUTPUT

cat > $OUTPUT/test.ixml <<EOF
book = line++-#a, -#a*.
line = ~[#a]+.
EOF

cat > $OUTPUT/test.inp <<EOF
Hej
Hopp
Sveksan


EOF

$PROG $OUTPUT/test.ixml $OUTPUT/test.inp > null 2>&1
testok $? "lines ok"

cat > $OUTPUT/test.inp <<EOF


Hej
Hopp
Sveksan


EOF

$PROG $OUTPUT/test.ixml $OUTPUT/test.inp > null 2>&1
testerr $? "lines fail"

echo "OK: test special 004 exit codes"
