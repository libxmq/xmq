#!/bin/bash
# libxmq - Copyright 2026 Fredrik Öhrström (spdx: MIT)
#
# Build the C examples in examples/c against a built libxmq.so and the Java examples in
# examples/java against the built jar, run them and compare their output with
# tests/examples/<name>.expected. The C and the Java version of an example print the
# same. The examples are copied to the output dir and built there, so the source tree
# is left clean. The timing line printed by parse_with_ixml is removed before the
# compare. The Java examples are skipped if the jar has not been built (make xmqj).
#
# Usage: tests/test_examples.sh [LIBDIR] [OUTPUT_DIR] [JAR]
# The defaults are build/default/release, build/test_examples and build/xmq-<version>.jar.

LIBDIR=$(realpath "${1:-build/default/release}")
OUT=${2:-build/test_examples}
VERSION=$(sed -n 's/^VERSION=//p' jproject.settings)
JAR=${3:-build/xmq-$VERSION.jar}
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

# check name dir output_file: compare the output of an example with the expected output.
check()
{
    local name=$1 lang=$2 output=$3
    local expected="tests/examples/$name.expected"
    if [ ! -f "$expected" ]
    then
        echo "ERROR: no $expected"
        FAILED=1
    elif ! diff -u "$expected" "$output"
    then
        echo "ERROR: $lang example $name printed the wrong output"
        FAILED=1
    fi
}

for d in "$OUT"/c/*/
do
    name=$(basename "$d")
    # A binary copied from the source tree could look up to date, remove it.
    make -s -C "$d" clean > /dev/null
    if ! make -s -C "$d" XMQ_ROOT="$ROOT" LIBDIR="$LIBDIR" CFLAGS="-g -O0 -Wall -Werror" > "$OUT/$name.c.build" 2>&1
    then
        echo "ERROR: C example $name failed to build"
        cat "$OUT/$name.c.build"
        FAILED=1
        continue
    fi
    (cd "$d" && "./$name") 2>&1 | grep -v ' took [0-9]* us$' > "$OUT/$name.c.output"
    if [ "${PIPESTATUS[0]}" != "0" ]
    then
        echo "ERROR: C example $name failed to run"
        FAILED=1
    else
        check "$name" C "$OUT/$name.c.output"
    fi
done

if [ ! -f "$JAR" ]
then
    echo "Skipping the Java examples, $JAR not found, run make xmqj."
else
    JAR=$(realpath "$JAR")
    cp -r examples/java "$OUT/java"
    for d in "$OUT"/java/*/
    do
        name=$(basename "$d")
        class=$(sed -n 's/^PROG = //p' "$d/Makefile")
        make -s -C "$d" clean > /dev/null
        if ! make -s -C "$d" JAR="$JAR" > "$OUT/$name.java.build" 2>&1 || [ -s "$OUT/$name.java.build" ]
        then
            echo "ERROR: Java example $name failed to build or has warnings"
            cat "$OUT/$name.java.build"
            FAILED=1
            continue
        fi
        (cd "$d" && java -cp "$JAR:." "$class") 2>&1 | grep -v ' took [0-9]* us$' > "$OUT/$name.java.output"
        if [ "${PIPESTATUS[0]}" != "0" ]
        then
            echo "ERROR: Java example $name failed to run"
            FAILED=1
        else
            check "$name" Java "$OUT/$name.java.output"
        fi
    done
fi

if [ "$FAILED" = "0" ]
then
    echo "OK: examples"
fi
exit $FAILED
