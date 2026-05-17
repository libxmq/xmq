#!/bin/sh
# libxmq - Copyright 2026 Fredrik Öhrström (spdx: MIT)

PROG=$1
OUTPUT=$2

if [ -z "$OUTPUT" ] || [ -z "$PROG" ]
then
    echo "Usage: tests/test_ixml_engine.sh [XMQ_BINARY] [OUTPUT_DIR]"
    exit 1
fi

mkdir -p $OUTPUT

cat > $OUTPUT/natlang.ixml <<EOF
S  = NP, VP.
NP = det?, adj*, noun.
VP = verb, (PF | NP | PP | NP, PP | ).
PF = adj | NP.
PP = prep, NP.

det  = 'a',s | 'an',s | 'the',s.
adj  = 'red',s | 'blue',s | 'green',s | 'good',s | 'evil',s | 'beautiful',s | 'dark',s.
noun = 'man',s | 'woman',s | 'ball',s | 'computer',s | 'rock',s | 'house',s | 'forest',s.
verb = 'is',s | 'gives',s | 'takes',s | 'looks',s | 'sings',s | 'goes',s | 'stands',s | 'runs',s.
prep = 'to',s | 'at',s | 'from',s | 'in',s | 'into',s | 'inside',s | 'on',s | 'beside',s.

-s = -' '+.
EOF

cat > $OUTPUT/natlang.exp <<EOF
S {
    NP {
        det  = a
        noun = man
    }
    VP {
        verb = runs
    }
}
EOF

# Note the final space which is required.
echo -n "a man runs " > $OUTPUT/natlang.inp

$PROG $OUTPUT/natlang.ixml $OUTPUT/natlang.inp > $OUTPUT/natlang.out

if ! diff $OUTPUT/natlang.out $OUTPUT/natlang.exp
then
    echo "ERROR: Ouch xmq output differs...."
fi

if command -v kaffepot > /dev/null 2>&1
then
    XMQ_IXML_ENGINE=kaffepot $PROG $OUTPUT/natlang.ixml $OUTPUT/natlang.inp > $OUTPUT/natlang.out
    if ! diff $OUTPUT/natlang.out $OUTPUT/natlang.exp
    then
        echo "ERROR: kaffepot output differs...."
    else
        echo "OK: coffepot generates same output"
    fi
fi

if command -v markup-blitz > /dev/null 2>&1
then
    XMQ_IXML_ENGINE=markup-blitz $PROG $OUTPUT/natlang.ixml $OUTPUT/natlang.inp > $OUTPUT/natlang.out
    if ! diff $OUTPUT/natlang.out $OUTPUT/natlang.exp
    then
        echo "ERROR: markup-blitz output differs...."
        OK=false
    else
        echo "OK: markup-blitz generates same output"
    fi
fi
