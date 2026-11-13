#!/bin/sh
# libxmq - Copyright 2026 Fredrik Öhrström (spdx: MIT)
# Error messages for command lines that the grammar xmqcli.ixml rejects.

PROG=$1
OUTPUT=$2

if [ -z "$OUTPUT" ] || [ -z "$PROG" ]
then
    echo "Usage: tests/test_cmd_....sh [XMQ_BINARY] [OUTPUT_DIR]"
    exit 1
fi

mkdir -p $OUTPUT

check()
{
    expected="$1"
    shift
    got=$($PROG "$@" 2>&1)
    rc=$?
    if [ "$rc" = "0" ] || [ "$got" != "$expected" ]
    then
        echo "ERROR: test cmd 005 bad command line: xmq $*"
        echo "rc=$rc"
        echo "Expected:"; echo "$expected"
        echo "Got:"; echo "$got"
        exit 1
    fi
}

check "xmq: bad command line, expected 'c' in name1, found 'k'
xmq pom.xml selekt //x
                ^
Try 'xmq --help' for more information" pom.xml selekt //x

check "xmq: bad command line, expected whitespace in s, found end of input
xmq pom.xml select
                  ^
Try 'xmq --help' for more information" pom.xml select

check "xmq: option \"--x\" not available for command \"select\"" pom.xml select -x //a

check "xmq: unrecognized global option: '--bogus'
Try 'xmq --help' for more information" --bogus pom.xml

echo "OK: test cmd 005 bad command line"
