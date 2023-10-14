#!/bin/sh

LIB=$1
OUTPUT=$2
TEST_FILE=$3
TEST_NAME=$(basename $3)
TEST_NAME=${TEST_NAME%.*}

if [ -z "$OUTPUT" ] || [ -z "$TEST_FILE" ] || [ -z "$LIB" ]
then
    echo "Usage: tests/test_program.sh build/libxmq.o test_output tests/99_foo.test"
    exit 1
fi

if [ ! -f "$TEST_FILE" ]
then
    echo "No such test file $TEST_FILE"
    exit 1
fi

mkdir -p $OUTPUT

ASAN=$(echo $LIB | grep -o "/asan/")

if [ "$ASAN" = "/asan/" ]
then
    CFLAGS="-fprofile-arcs -ftest-coverage"
    LDFLAGS="--coverage -fprofile-arcs -ftest-coverage"
    LDLIBS="-lasan"
fi

DEBUG=$(echo $LIB | grep -o "/debug/")

if [ "$DEBUG" = "/debug/" ]
then
    CFLAGS="-g -O0"
    LDFLAGS=""
    LDLIBS=""
fi

gcc $CFLAGS -c -I src/main/c -o $OUTPUT/test.o $TEST_FILE
gcc $LDFLAGS -o $OUTPUT/test $OUTPUT/test.o $LDLIBS  $LIB -lxml2

cp tests/${TEST_NAME}.* ${OUTPUT}
rm -f ${OUTPUT}/test.gcda

if (cd $OUTPUT; ./test ${TEST_NAME}.xmq)
then
    echo "OK: $TEST_NAME"
    exit 0
else
    echo "ERROR: $TEST_NAME"
    exit 1
fi
