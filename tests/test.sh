#!/bin/bash
# libxmq - Copyright 2023 Fredrik Öhrström (spdx: MIT)

BUILD=$1
OUTPUT=$2

PROG=$1/xmq
LIB=$1/libxmq.so

if [ -z "$OUTPUT" ] || [ -z "$PROG" ]
then
    echo "Usage: test.sh build test_output"
    exit 1
fi

rm -rf "$OUTPUT"
mkdir -p "$OUTPUT"

for i in tests/???_*.test
do
    tests/test_single.sh "$PROG" "$OUTPUT" "$i"
done

for i in tests/error_???_*.test
do
    tests/test_error.sh "$PROG" "$OUTPUT" "$i"
done

for i in tests/format_???_*.test
do
    tests/test_formatting.sh "$PROG" "$OUTPUT" "$i"
done

for i in tests/json_???_*.test
do
    tests/test_json.sh "$PROG" "$OUTPUT" "$i"
done

for i in tests/backforth_???_*.test
do
    tests/test_backforth.sh "$PROG" "$OUTPUT" "$i"
done

for i in tests/test_special_???_*.sh
do
    $i "$PROG" "$OUTPUT"
done

for i in tests/test_???_*.c
do
    tests/test_program.sh "$LIB" "$OUTPUT" "$i"
done

if [ -f build/XMQ-1.0-SNAPSHOT.jar ]
then
    for i in tests/java/*.java
    do
        tests/test_java.sh build/XMQ-1.0-SNAPSHOT.jar "$OUTPUT" "$i"
    done
fi
