#!/bin/sh
# libxmq - Copyright 2023 Fredrik Öhrström (spdx: MIT)

JAR=$1
OUTPUT=$2
TEST_FILE=$3
TEST_NAME=$(basename $3 2> /dev/null)
TEST_NAME=${TEST_NAME%.*}

if [ -z "$OUTPUT" ] || [ -z "$TEST_FILE" ] || [ -z "$JAR" ]
then
    echo "Usage: tests/test_java.sh [XMQ_JAR] [OUTPUT_DIR] tests/java/Test1.java"
    exit 1
fi

if [ ! -f "$TEST_FILE" ]
then
    echo "No such test file $TEST_FILE"
    exit 1
fi

mkdir -p $OUTPUT

javac -d $OUTPUT -cp $OUTPUT:$JAR $TEST_FILE

java -cp $OUTPUT:$JAR $TEST_NAME > /dev/null

if [ "$?" = "0" ]
then
    echo "OK: java $TEST_NAME"
else
    echo "ERROR: java $TEST_NAME"
fi
