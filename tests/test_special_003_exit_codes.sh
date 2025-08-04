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

# Valid xmq element name.
$PROG -i "howdy" > null 2>&1
testok $? "howdy"

# This is an invalid xmq element name.
$PROG -i "ho+dy" > null 2>&1
testerr $? "ho+dy"

# This is a json string.
$PROG -i '"ho+dy"' > null 2>&1
testok $? '"ho+dy"'

# This is an xmln string.
$PROG -i '<howdy/>' > null 2>&1
testok $? '<howdy/>'

# This is a broken xmln string.
$PROG -i '<howdy>' > null 2>&1
testerr $? '<howdy>'

# This is valid xml.
$PROG -i '<howdy></howdy>' > null 2>&1
testok $? '<howdy></howdy>'

# This is invalid json.
$PROG -i '{"key":"value","key":foobar}' > null 2>&1
testerr $? '{"key":"value","key":foobar}'

# This is valid json.
$PROG -i '{"key":"value","keyy":false}' > null 2>&1
testok $? '{"key":"value","keyy":false}'

# This is valid xmq.
$PROG -i "body(name=1){content=''''alfa''''}" > null 2>&1
testok $? "body(name=1){content=''''alfa''''}"

# This is valid xmq.
$PROG -i "body(name=1){content=''''alfa'''}" > null 2>&1
testerr $? "body(name=1){content=''''alfa'''}"

# This is valid xmq.
$PROG -i "body(name=1){content='''alfa'''}" select //body/content > null 2>&1
testok $? "body(name=1){content='''alfa'''} select //body/content"

# Lines
$PROG -i "alfa
beta
gamma
delta" add-root banana > null 2>&1
testok $? "body(name=1){content='''alfa'''} select //body/content"


echo "OK: test special 003 exit codes"
