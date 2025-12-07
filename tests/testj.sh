#!/bin/bash
# libxmq - Copyright 2023-2025 Fredrik Öhrström (spdx: MIT)

#set -e
BUILD=$1
OUTPUT=$2
FILTER=$3

. $BUILD/java/spec.sh

PROG=${BUILD}/$EXECUTABLE
LIB=${BUILD}/${ARTIFACTID}-${VERSION}.jar

if [ -z "$OUTPUT" ] || [ -z "$PROG" ]
then
    echo "Usage: testj.sh build test_output"
    exit 1
fi

rm -rf "$OUTPUT"
mkdir -p "$OUTPUT"

for i in tests/java/*.java
do
    if [ -n $FILTER ] && [[ ! "$i" =~ $FILTER ]]; then continue; fi
    tests/test_java.sh $LIB "$OUTPUT" "$i"
done

for i in tests/[0-9][0-9][0-9]_*.test
do
    if [ -n $FILTER ] && [[ ! "$i" =~ $FILTER ]]; then continue; fi
    tests/test_single.sh "$PROG" "$OUTPUT" "$i"
done
