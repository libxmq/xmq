#!/bin/bash

SPEC="$1"
. $SPEC

rm -f $SRC_ROOT/dist/xmq.c $SRC_ROOT/dist/xmq.h
make dist
cd dist
make
./example

if [ "$?" = "0" ]
then
    echo "OK: dist example"
else
    echo "ERR: Failed to run example in dist"
    exit 1
fi

exit 0
