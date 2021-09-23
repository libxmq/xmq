#!/bin/bash

BUILDDIR="$1"

PROG="$BUILDDIR/xmq"
TESTDIR=test_output

TESTS=$(ls tests/test*.sh)

#for i in $TESTS; do
#    $i $PROG $TESTDIR
#    if [ $? == "0" ]; then echo OK; fi
#done

./tests/test_010_sort_attributes.sh $PROG $TESTDIR
