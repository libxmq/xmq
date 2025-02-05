#!/bin/bash

PROG=$1
TEMP=$(mktemp)

for i in */*-tests
do
    if [ -n $FILTER ] && [[ ! "$i" =~ $FILTER ]]; then continue; fi

    GRAMMAR="${i%%-tests}.ixml"
    if [ -f "$GRAMMAR" ]
    then
        for j in $i/*_???_*.input
        do
            TEST="$(basename $j)"
            TEST="${TEST%%.input}"
            INPUT="$j"
            OUTPUT="${j%%.input}.output"
            if [ -f "$INPUT" ] && [ -f "$OUTPUT" ]
            then
                $PROG --ixml=$GRAMMAR $INPUT > $TEMP 2>&1
                if diff $OUTPUT $TEMP
                then
                    echo "OK: $TEST"
                else
                    echo "ERR: $TEST"
                fi
            fi
        done
    fi
done

rm -f $TEMP
