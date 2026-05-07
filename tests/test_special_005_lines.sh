#!/bin/sh
# libxmq - Copyright 2026 Fredrik Öhrström (spdx: MIT)

PROG=$1
OUTPUT=$2
TEST_NAME=$(basename $1 2> /dev/null)
TEST_NAME=${TEST_NAME%.*}

if [ -z "$OUTPUT" ] || [ -z "$PROG" ]
then
    echo "Usage: tests/test_special....sh [XMQ_BINARY] [OUTPUT_DIR]"
    exit 1
fi

mkdir -p $OUTPUT

echo "BESETTER" > $OUTPUT/input.xmq
echo "BBBBBBBB" >> $OUTPUT/input.xmq
echo "BEVESSELED" >> $OUTPUT/input.xmq


cat <<EOF > $OUTPUT/grammar.ixml
-ok = rule_H.
-vowel = ["AEIOU"].
-consonant = ["BCDGHLMNPRSTVWXY"].

rule_H:
"E"?,
(consonant; "N", ["CS"]; ["ST"],["ST"])++"E",
"E"?.
EOF

$PROG --lines --ixml-fail-silent $OUTPUT/grammar.ixml $OUTPUT/input.xmq > $OUTPUT/output.txt

cat << EOF > $OUTPUT/expected.txt
rule_H = BESETTER
rule_H = BEVESSELED
EOF

if diff $OUTPUT/output.txt $OUTPUT/expected.txt
then
    echo "OK: test special 005 lines"
else
    echo "ERROR: test special 005 lines"
    echo "Formatting differ:"
    if [ -n "$USE_MELD" ]
    then
        meld $OUTPUT/expected.txt $OUTPUT/output.txt
    else
        diff $OUTPUT/expected.txt $OUTPUT/output.txt
    fi
    exit 1
fi
