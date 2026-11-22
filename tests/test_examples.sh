#!/bin/bash
# libxmq - Copyright 2026 Fredrik Öhrström (spdx: MIT)
#
# Build the C examples in examples/c against a built libxmq.so, run them and compare
# their output with tests/examples/c/<name>.expected. The examples are copied to the
# output dir and built there, so the source tree is left clean. The timing line printed
# by parse_with_ixml is removed before the compare.
#
# Usage: tests/test_examples.sh [LIBDIR] [OUTPUT_DIR]
# The defaults are build/default/release and build/test_examples.

LIBDIR=$(realpath "${1:-build/default/release}")
OUT=${2:-build/test_examples}
ROOT=$(pwd)

if [ ! -f "$LIBDIR/libxmq.so" ]
then
    echo "Cannot find $LIBDIR/libxmq.so, run make first."
    exit 1
fi

rm -rf "$OUT"
mkdir -p "$OUT"
cp -r examples/c "$OUT/c"

FAILED=0
for d in "$OUT"/c/*/
do
    name=$(basename "$d")
    expected="tests/examples/c/$name.expected"
    if ! make -s -C "$d" XMQ_ROOT="$ROOT" LIBDIR="$LIBDIR" CFLAGS="-g -O0 -Wall -Werror" > "$OUT/$name.build" 2>&1
    then
        echo "ERROR: example $name failed to build"
        cat "$OUT/$name.build"
        FAILED=1
        continue
    fi
    (cd "$d" && "./$name") 2>&1 | grep -v ' took [0-9]* us$' > "$OUT/$name.output"
    if [ "${PIPESTATUS[0]}" != "0" ]
    then
        echo "ERROR: example $name failed to run"
        FAILED=1
    elif [ ! -f "$expected" ]
    then
        echo "ERROR: no $expected"
        FAILED=1
    elif ! diff -u "$expected" "$OUT/$name.output"
    then
        echo "ERROR: example $name printed the wrong output"
        FAILED=1
    fi
done

if [ "$FAILED" = "0" ]
then
    echo "OK: examples"
fi
exit $FAILED
