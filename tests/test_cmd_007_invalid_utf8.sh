#!/bin/bash
# libxmq - Copyright 2026 Fredrik Öhrström (spdx: MIT)
#
# xmq, json and yaml input that is not valid UTF-8 is rejected with the position and the bad bytes.
# Run by both the C (make test) and the Java (make testj) test suites.

PROG=$1
OUTPUT=$2/test_cmd_007

if [ -z "$2" ] || [ -z "$PROG" ]
then
    echo "Usage: tests/test_cmd_007_invalid_utf8.sh [XMQ_BINARY] [OUTPUT_DIR]"
    exit 1
fi

mkdir -p $OUTPUT

FAILED=0

# check FILE INPUT_PRINTF EXPECTED_ERROR [COMMANDS...], FILE without extension is .xmq,
# global options before the file in OPTS.
check()
{
    NAME=$1
    case "$NAME" in *.*) FILE=$NAME ;; *) FILE=$NAME.xmq ;; esac
    printf "$2" > $OUTPUT/$FILE
    EXPECTED=$3
    shift 3
    $PROG $OPTS $OUTPUT/$FILE "$@" > $OUTPUT/$NAME.out 2>&1
    RC=$?
    GOT=$(sed "s|$OUTPUT/||" $OUTPUT/$NAME.out)
    if [ "$RC" != "1" ] || [ "$GOT" != "$EXPECTED" ]
    then
        echo "ERROR: test cmd 007 invalid utf8 $NAME (rc=$RC expected 1)"
        diff <(echo "$EXPECTED") <(echo "$GOT")
        FAILED=1
    fi
}

check stray 'a = 1\nbb = x\xa7\x94y\n' "stray.xmq:2:7: error: invalid UTF-8: 0xA7 0x94
bb = x
      ^"
check overlong 'å = "x\xc0\xafy"\n' "overlong.xmq:1:7: error: invalid UTF-8: 0xC0 0xAF
å = \"x
       ^"
check surrogate 'a = 1\n\xed\xa0\x80\n' "surrogate.xmq:2:1: error: invalid UTF-8: 0xED 0xA0 0x80

 ^"
check truncated 'a = \xe2\x82' "truncated.xmq:1:5: error: invalid UTF-8: 0xE2 0x82
a = 
    ^"
check tokenize 'a = \xff' "tokenize.xmq:1:5: error: invalid UTF-8: 0xFF
a = 
    ^" tokenize

check json.json '{"a":1,\n "b":"x\xa7y"}\n' "json.json:2:8: error: invalid UTF-8: 0xA7
 \"b\":\"x
       ^"
check yaml.yaml 'a: 1\nb: x\xc0\xafy\n' "yaml.yaml:2:5: error: invalid UTF-8: 0xC0 0xAF
b: x
    ^"
OPTS=--yaml check yaml_forced.txt 'a: \xf8\n' "yaml_forced.txt:1:4: error: invalid UTF-8: 0xF8
a: 
   ^"

if [ "$FAILED" = "0" ]
then
    echo "OK: test cmd 007 invalid utf8"
else
    exit 1
fi
