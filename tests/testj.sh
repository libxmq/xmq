#!/bin/bash
# libxmq - Copyright 2023-2025 Fredrik Öhrström (spdx: MIT)

#set -e
PROG=$1
BUILD=$2
OUTPUT=$3
FILTER=$4

if [ -z "$PROG" ]
then
    . $BUILD/java/spec.sh

    PROG=${BUILD}/$EXECUTABLE
    LIB=${BUILD}/${ARTIFACTID}-${VERSION}.jar
fi

if [ -z "$OUTPUT" ] || [ -z "$PROG" ]
then
    echo "Usage: testj.sh build test_output"
    exit 1
fi

rm -rf "$OUTPUT"
mkdir -p "$OUTPUT"

if [ -n "$LIB" ]
then
    for i in tests/java/*.java
    do
        if [ -n $FILTER ] && [[ ! "$i" =~ $FILTER ]]; then continue; fi
        tests/test_java.sh $LIB "$OUTPUT" "$i"
    done
fi

for i in tests/[0-9][0-9][0-9]_*.test
do
    if [ -n $FILTER ] && [[ ! "$i" =~ $FILTER ]]; then continue; fi
    tests/test_single.sh "$PROG" "$OUTPUT" "$i"
done

for i in tests/json_???_*.test
do
    if [ -n $FILTER ] && [[ ! "$i" =~ $FILTER ]]; then continue; fi
    tests/test_json.sh "$PROG" "$OUTPUT" "$i"
    if [ "$?" != 0 ]; then echo "Testing aborted"; exit 1 ; fi
done

for i in tests/parse_json_???_*.test
do
    if [ -n $FILTER ] && [[ ! "$i" =~ $FILTER ]]; then continue; fi
    tests/test_parse_json.sh "$PROG" "$OUTPUT" "$i"
    if [ "$?" != 0 ]; then echo "Testing aborted"; exit 1 ; fi
done

for i in tests/yaml_???_*.test
do
    if [ -n $FILTER ] && [[ ! "$i" =~ $FILTER ]]; then continue; fi
    tests/test_yaml.sh "$PROG" "$OUTPUT" "$i"
    if [ "$?" != 0 ]; then echo "Testing aborted"; exit 1 ; fi
done

if [ -z "$FILTER" ] || [[ "yaml" =~ $FILTER ]]
then
    tests/test_special_010_yaml_source_name.sh "$PROG" "$OUTPUT"
    if [ "$?" != 0 ]; then echo "Testing aborted"; exit 1 ; fi
fi

if [ -z "$FILTER" ] || [[ "tokenize" =~ $FILTER ]]
then
    tests/test_special_011_tokenize.sh "$PROG" "$OUTPUT"
    if [ "$?" != 0 ]; then echo "Testing aborted"; exit 1 ; fi
fi

for i in tests/backforth_???_*.test
do
    if [ -n $FILTER ] && [[ ! "$i" =~ $FILTER ]]; then continue; fi
    tests/test_backforth.sh "$PROG" "$OUTPUT" "$i"
    if [ "$?" != 0 ]; then echo "Testing aborted"; exit 1 ; fi
done

# The Java and the C command lines must behave the same, when the C xmq is built.
XMQ_C=$(ls build/*/release/xmq 2>/dev/null | head -1)
if [ -n "$XMQ_C" ] && [ -z "$FILTER" ]
then
    tests/test_cli_compare.sh "$XMQ_C" "$PROG" "$OUTPUT/cli_compare"
    if [ "$?" != 0 ]; then echo "Testing aborted"; exit 1 ; fi
fi

for i in tests/theme_???_*.test
do
    if [ -n $FILTER ] && [[ ! "$i" =~ $FILTER ]]; then continue; fi
    tests/test_theme.sh "$PROG" "$OUTPUT" "$i"
    if [ "$?" != 0 ]; then echo "Testing aborted"; exit 1 ; fi
done

for i in tests/format_???_*.test
do
    if [ -n $FILTER ] && [[ ! "$i" =~ $FILTER ]]; then continue; fi
    tests/test_formatting.sh "$PROG" "$OUTPUT" "$i"
    if [ "$?" != 0 ]; then echo "Testing aborted"; exit 1 ; fi
done
