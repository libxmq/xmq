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
    # Same sanitizers as the asan build of libxmq (configure: CFLAGS_asan/LDFLAGS_asan).
    CFLAGS="-fprofile-arcs -ftest-coverage -fsanitize=address,undefined -fno-omit-frame-pointer"
    LDFLAGS="--coverage -fprofile-arcs -ftest-coverage -fsanitize=address,undefined"
    LDLIBS=""
fi

DEBUG=$(echo $LIB | grep -o "/debug/")

if [ "$DEBUG" = "/debug/" ]
then
    CFLAGS="-g -O0"
    LDFLAGS=""
    LDLIBS=""
fi

# Compile and link with the compiler that built libxmq, CC in the spec.mk of the build
# (build/<host>/spec.mk for build/<host>/release/libxmq.a), eg filcc, otherwise gcc.
SPEC="$(dirname "$(dirname "$LIB")")/spec.mk"
CC=$(sed -n 's/^CC:=//p' "$SPEC" 2>/dev/null)
CC=${CC:-gcc}

$CC $CFLAGS -c -I src/main/c -o $OUTPUT/test.o $TEST_FILE
$CC $LDFLAGS -o $OUTPUT/test $OUTPUT/test.o $LDLIBS $LIB -lm

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
