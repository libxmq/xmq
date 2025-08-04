#!/bin/sh
# libxmq - Copyright 2024 Fredrik Öhrström (spdx: MIT)

PROG=$1
OUTPUT=$2

if [ -z "$OUTPUT" ] || [ -z "$PROG" ]
then
    echo "Usage: tests/test_library.sh [XMQ_BINARY] [OUTPUT_DIR]"
    exit 1
fi

mkdir -p $OUTPUT

$PROG --ixml=library/core/ixml.ixml library/core/ixml.ixml > $OUTPUT/ixml.output 2>/dev/null
$PROG library/core/ixml.xml > $OUTPUT/ixml.expected 2>/dev/null
if diff $OUTPUT/ixml.output $OUTPUT/ixml.expected
then
    echo "OK: test ixml.ixml"
else
    echo "ERR: Failed to parse ixml.ixml"
    exit 1
fi

for i in library/data/*.ixml
do
    g="$i"
    n="$(basename $g)"
    n="${n%.ixml}-test"
    testdir="$(dirname $g)/$n"

    if [ -d "$testdir" ]
    then
        for t in $testdir/*.inp
        do
            o="${t%.inp}.out"
            $PROG $i $t > $OUTPUT/tmp
            if diff $OUTPUT/tmp $o
            then
                echo OK: $t
            else
                echo ERROR: $t
                exit 1
            fi
        done
    fi
done

for i in library/data/*.xslq
do
    g="$i"
    n="$(basename $g)"
    n="${n%.xslq}-test"
    testdir="$(dirname $g)/$n"

    if [ -d "$testdir" ]
    then
        for t in $testdir/*.inp
        do
            o="${t%.inp}.out"
            $PROG $i transform $t > $OUTPUT/tmp
            if diff $OUTPUT/tmp $o
            then
                echo OK: $t
            else
                echo ERROR: $t
                exit 1
            fi
        done
    fi
done
