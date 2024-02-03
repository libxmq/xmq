#!/bin/bash
# libxmq - Copyright 2023 Fredrik Öhrström (spdx: MIT)

BUILD=$1
OUTPUT=$2

SPEC=$(dirname $1)/spec.inc
if [ ! -f "$SPEC" ]
then
    echo "Cannot find spec.inc file: $SPEC"
    echo "Please run configure."
    exit 1
fi

. $SPEC

PROG=$1/xmq
LIB=$1/libxmq.so

if [ -z "$OUTPUT" ] || [ -z "$PROG" ]
then
    echo "Usage: test.sh build test_output"
    exit 1
fi

rm -rf "$OUTPUT"
mkdir -p "$OUTPUT"

for i in tests/[0-9][0-9][0-9]_*.test
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

for i in tests/cmd_???_*.test
do
    tests/test_cmd.sh "$PROG" "$OUTPUT" "$i"
done

for i in tests/json_???_*.test
do
    tests/test_json.sh "$PROG" "$OUTPUT" "$i"
done

for i in tests/parse_json_???_*.test
do
    tests/test_parse_json.sh "$PROG" "$OUTPUT" "$i"
done

for i in tests/backforth_???_*.test
do
    tests/test_backforth.sh "$PROG" "$OUTPUT" "$i"
done

for i in tests/xslt_???_*.test
do
    tests/test_xslt.sh "$PROG" "$OUTPUT" "$i"
done

for i in tests/statistics_???_*.test
do
    tests/test_statistics.sh "$PROG" "$OUTPUT" "$i"
done

#for i in tests/test_special_???_*.sh
#do
#    $i "$PROG" "$OUTPUT"
#done

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

if [ "$CONF_MNEMONIC" = "linux64" ]
then
    tests/test_dist.sh $SPEC
fi
