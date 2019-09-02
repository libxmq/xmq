#!/bin/bash

BUILDDIR="$1"

PROG1="$BUILDDIR/xml2xmq"
PROG2="$BUILDDIR/xmq2xml"
TESTDIR=test_output

TESTS=$(ls tests/test*.sh)

for i in $TESTS; do
    $i $PROG1 $PROG2 $TESTDIR
    if [ $? == "0" ]; then echo OK; fi
done
