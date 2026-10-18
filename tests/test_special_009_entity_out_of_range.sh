#!/bin/sh
# libxmq - Copyright 2026 Fredrik Öhrström (spdx: MIT)
#
# Regression test: a numeric character entity with a value outside the valid
# Unicode range (0 .. 0x10FFFF), ie &#2000000;, must produce a normal parse
# error. Before the fix the out-of-range value reached encode_utf8() which
# aborted on the assert "uc <= 0x10ffff" (SIGABRT). See src/main/c/xmq.c,
# create_entity().

PROG=$1
OUTPUT=$2

if [ -z "$OUTPUT" ] || [ -z "$PROG" ]
then
    echo "Usage: tests/test_special....sh [XMQ_BINARY] [OUTPUT_DIR]"
    exit 1
fi

mkdir -p $OUTPUT

# Expect a clean parse error: non-zero exit code that is NOT an abort/crash,
# and an "error" message on stderr/stdout. An abort (SIGABRT) would exit 134
# and print an "Assertion" message; a segfault would exit 139.
testerr() {
    IN="$1"
    OUT=$("$PROG" -i "$IN" 2>&1)
    RC=$?
    if [ $RC -eq 0 ]
    then
        echo "ERROR: expected a parse error for '$IN' but it succeeded"
        exit 1
    fi
    if [ $RC -ge 128 ]
    then
        echo "ERROR: '$IN' aborted/crashed (exit $RC) instead of a clean parse error"
        exit 1
    fi
    case "$OUT" in
        *Assertion*|*Segmentation*|*segfault*|*core\ dumped*)
            echo "ERROR: '$IN' crashed/asserted instead of a clean parse error"
            exit 1 ;;
    esac
    case "$OUT" in
        *error*)
            return 0 ;;
        *)
            echo "ERROR: expected an 'error' message for '$IN' but got: $OUT"
            exit 1 ;;
    esac
}

# A valid character entity must succeed (exit 0).
testok() {
    IN="$1"
    "$PROG" -i "$IN" >/dev/null 2>&1
    RC=$?
    if [ $RC -ne 0 ]
    then
        echo "ERROR: expected the valid entity '$IN' to succeed but got exit $RC"
        exit 1
    fi
    return 0
}

# Out of range (above 0x10FFFF) -- decimal, hex and overflowing values.
testerr '&#2000000;'
testerr '&#1114112;'
testerr '&#x110000;'
testerr '&#x1000000;'
testerr '&#x7fffffff;'
testerr '&#99999999999999999999;'

# In range (including the exact maximum, 0x10FFFF) must succeed.
testok '&#65;'
testok '&#233;'
testok '&#1114111;'
testok '&#x10ffff;'
testok '&nbsp;'

echo "OK: test special 009 entity out of range"
