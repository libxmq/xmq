#!/bin/sh
# libxmq - Copyright 2026 Fredrik Öhrström (spdx: MIT)

PROG=$1
OUTPUT=$2
GRAMMAR=$3

if [ -z "$OUTPUT" ] || [ -z "$PROG" ]
then
    echo "Usage: tests/test_library_grammar.sh [XMQ_BINARY] [OUTPUT_DIR] [GRAMMAR.ixml]"
    exit 1
fi

mkdir -p $OUTPUT

GDIR=$(dirname $GRAMMAR)
GNAME=$(basename $GRAMMAR)
GNAME=${GNAME%.ixml}
IXML=$GRAMMAR
XSLQ=$GDIR/${GNAME}.xslq
TDIR=$GDIR/test-${GNAME}

for i in $TDIR/*.inp
do
    EXP=$(basename $i)
    EXP=${i%.inp}.exp
    TR=""
    if test -f $XSLQ
    then
        TR="transform $XSLQ"
    fi
    OUT=$OUTPUT/$(basename $i).out
    $PROG $IXML $i $TR > $OUT 2>&1
    if diff $OUT $EXP
    then
        echo OK: $IXML $XSLQ $i
    else
        if [ "$USE_MELD" = "true" ]
        then
            meld $OUT $EXP
        fi
        echo ERROR
    fi
done
