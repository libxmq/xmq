#!/bin/bash
# libxmq - Copyright 2023-2024 Fredrik Öhrström (spdx: MIT)

#set -e
BUILD=$1
OUTPUT=$2
FILTER=$3

SPEC=$(dirname $1)/spec.inc
if [ ! -f "$SPEC" ]
then
    SPEC=$1/spec.inc
    if [ ! -f "$SPEC" ]
    then
        echo "Cannot find spec.inc file: $SPEC"
        echo "Please run configure."
        exit 1
    fi
fi

. $SPEC

PROG=$1/xmq
LIB=$1/libxmq.so

echo PROG=$PROG
echo OUTPUT=$OUTPUT

if [ -z "$OUTPUT" ] || [ -z "$PROG" ]
then
    echo "Usage: PRUTT test.sh build test_output"
    exit 1
fi

rm -rf "$OUTPUT"
mkdir -p "$OUTPUT"

for i in tests/[0-9][0-9][0-9]_*.test
do
    if [ -n $FILTER ] && [[ ! "$i" =~ $FILTER ]]; then continue; fi
    tests/test_single.sh "$PROG" "$OUTPUT" "$i"
done

for i in tests/ixml_not_[0-9][0-9][0-9]_*.test
do
    if [ -n $FILTER ] && [[ ! "$i" =~ $FILTER ]]; then continue; fi
    tests/test_ixml_not.sh "$PROG" "$OUTPUT" "$i"
    if [ "$?" != 0 ]; then echo "Testing aborted"; exit 1 ; fi
done


for i in tests/xmqixml_[0-9][0-9][0-9]_*.test
do
    if [ -n $FILTER ] && [[ ! "$i" =~ $FILTER ]]; then continue; fi
    tests/test_xmqixml_format.sh "$PROG" "$OUTPUT" "$i"
    if [ "$?" != 0 ]; then echo "Testing aborted"; exit 1 ; fi
done

for i in tests/ixml_grammar_[0-9][0-9][0-9]_*.test
do
    if [ -n $FILTER ] && [[ ! "$i" =~ $FILTER ]]; then continue; fi
    tests/test_ixml_grammar.sh "$PROG" "$OUTPUT" "$i"
    if [ "$?" != 0 ]; then echo "Testing aborted"; exit 1 ; fi
done

for i in tests/ixml_error_[0-9][0-9][0-9]_*.test
do
    if [ -n $FILTER ] && [[ ! "$i" =~ $FILTER ]]; then continue; fi
    tests/test_ixml_error.sh "$PROG" "$OUTPUT" "$i"
    if [ "$?" != 0 ]; then echo "Testing aborted"; exit 1 ; fi
done

for i in tests/ixml_parse_[0-9][0-9][0-9]_*.test
do
    if [ -n $FILTER ] && [[ ! "$i" =~ $FILTER ]]; then continue; fi
    tests/test_ixml_parse.sh "$PROG" "$OUTPUT" "$i"
    if [ "$?" != 0 ]; then echo "Testing aborted"; exit 1 ; fi
done

tests/test_grammars.sh "$PROG" "$OUTPUT"
if [ "$?" != 0 ]; then echo "Testing aborted"; exit 1 ; fi

for i in tests/ixml/correct/*.output.xmq
do
    if [ -n $FILTER ] && [[ ! "$i" =~ $FILTER ]]; then continue; fi
    tests/test_ixml_correct.sh "$PROG" "$OUTPUT" "$i"
    if [ "$?" != 0 ]; then echo "Testing aborted"; exit 1 ; fi
done

for i in tests/pipe_???_*.test
do
    if [ -n $FILTER ] && [[ ! "$i" =~ $FILTER ]]; then continue; fi
    tests/test_pipes.sh "$PROG" "$OUTPUT" "$i"
    if [ "$?" != 0 ]; then echo "Testing aborted"; exit 1 ; fi
done

for i in tests/error_???_*.test
do
    if [ -n $FILTER ] && [[ ! "$i" =~ $FILTER ]]; then continue; fi
    tests/test_error.sh "$PROG" "$OUTPUT" "$i"
#    if [ "$?" != 0 ]; then echo "Testing aborted"; exit 1 ; fi
done

for i in tests/warning_???_*.test
do
    if [ -n $FILTER ] && [[ ! "$i" =~ $FILTER ]]; then continue; fi
    tests/test_warning.sh "$PROG" "$OUTPUT" "$i"
    if [ "$?" != 0 ]; then echo "Testing aborted"; exit 1 ; fi
done

for i in tests/format_???_*.test
do
    if [ -n $FILTER ] && [[ ! "$i" =~ $FILTER ]]; then continue; fi
    tests/test_formatting.sh "$PROG" "$OUTPUT" "$i"
    if [ "$?" != 0 ]; then echo "Testing aborted"; exit 1 ; fi
done

for i in tests/noinput_???_*.test
do
    if [ -n $FILTER ] && [[ ! "$i" =~ $FILTER ]]; then continue; fi
    tests/test_noinput.sh "$PROG" "$OUTPUT" "$i"
    if [ "$?" != 0 ]; then echo "Testing aborted"; exit 1 ; fi
done

for i in tests/cmd_???_*.test
do
    if [ -n $FILTER ] && [[ ! "$i" =~ $FILTER ]]; then continue; fi
    tests/test_cmd.sh "$PROG" "$OUTPUT" "$i"
    if [ "$?" != 0 ]; then echo "Testing aborted"; exit 1 ; fi
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

for i in tests/backforth_???_*.test
do
    if [ -n $FILTER ] && [[ ! "$i" =~ $FILTER ]]; then continue; fi
    tests/test_backforth.sh "$PROG" "$OUTPUT" "$i"
    if [ "$?" != 0 ]; then echo "Testing aborted"; exit 1 ; fi
done

for i in tests/select_???_*.test
do
    if [ -n $FILTER ] && [[ ! "$i" =~ $FILTER ]]; then continue; fi
    tests/test_select.sh "$PROG" "$OUTPUT" "$i"
    if [ "$?" != 0 ]; then echo "Testing aborted"; exit 1 ; fi
done

for i in tests/xslt_???_*.test
do
    if [ -n $FILTER ] && [[ ! "$i" =~ $FILTER ]]; then continue; fi
    tests/test_xslt.sh "$PROG" "$OUTPUT" "$i"
    if [ "$?" != 0 ]; then echo "Testing aborted"; exit 1 ; fi
done

for i in tests/statistics_???_*.test
do
    if [ -n $FILTER ] && [[ ! "$i" =~ $FILTER ]]; then continue; fi
    tests/test_statistics.sh "$PROG" "$OUTPUT" "$i"
    if [ "$?" != 0 ]; then echo "Testing aborted"; exit 1 ; fi
done

for i in tests/xsd_???_*.test
do
    if [ -n $FILTER ] && [[ ! "$i" =~ $FILTER ]]; then continue; fi
    tests/test_xsd.sh "$PROG" "$OUTPUT" "$i"
    if [ "$?" != 0 ]; then echo "Testing aborted"; exit 1 ; fi
done

for i in tests/test_special_???_*.sh
do
    if [ -n $FILTER ] && [[ ! "$i" =~ $FILTER ]]; then continue; fi
    $i "$PROG" "$OUTPUT"
    if [ "$?" != 0 ]; then echo "Testing aborted"; exit 1 ; fi
done

for i in tests/test_cmd_???_*.sh
do
    if [ -n $FILTER ] && [[ ! "$i" =~ $FILTER ]]; then continue; fi
    $i "$PROG" "$OUTPUT"
    if [ "$?" != 0 ]; then echo "Testing aborted"; exit 1 ; fi
done

for i in tests/test_???_*.c
do
    if [ -n $FILTER ] && [[ ! "$i" =~ $FILTER ]]; then continue; fi
    tests/test_program.sh "$LIB" "$OUTPUT" "$i"
    if [ "$?" != 0 ]; then echo "Testing aborted"; exit 1 ; fi
done

if [ -f build/XMQ-1.0-SNAPSHOT.jar ]
then
    for i in tests/java/*.java
    do
        if [ -n $FILTER ] && [[ ! "$i" =~ $FILTER ]]; then continue; fi
        tests/test_java.sh build/XMQ-1.0-SNAPSHOT.jar "$OUTPUT" "$i"
    done
fi

if [ -n $FILTER ]; then exit 0; fi

if [ "$CONF_MNEMONIC" = "linux64" ]
then
    tests/test_dist.sh $SPEC
fi

if command -v xelatex > /dev/null
then
    tests/test_tex.sh "$PROG" "$OUTPUT"
fi
