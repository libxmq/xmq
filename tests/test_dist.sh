#!/bin/bash

ROOT=$(pwd)

rm -f $ROOT/dist/xmq.c $ROOT/dist/xmq.h
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
