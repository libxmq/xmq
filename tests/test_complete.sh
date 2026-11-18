#!/bin/bash
# libxmq - Copyright 2026 Fredrik Öhrström (spdx: MIT)
#
# Test xmq --complete: what could follow the input parsed with an ixml grammar,
# one line per string, char, charset or end, used for tab completion.
# Usage: tests/test_complete.sh <xmq binary> <output dir>

PROG=$1
OUTPUT=$2/complete
mkdir -p "$OUTPUT"

fail()
{
    echo "ERR: complete $1"
    exit 1
}

cat > "$OUTPUT/natlang.ixml" <<'GRAMMAR'
S  = NP, VP.
NP = det?, noun.
VP = verb, NP?.
det  = 'a',s | 'the',s.
noun = 'man',s | 'woman',s | 'ball',s.
verb = 'is',s | 'gives',s | 'takes',s.
name = ['A'-'Z'], ['a'-'z']*.
-s = -' '+.
GRAMMAR

# check "input" "expected lines" (tabs written as |)
check()
{
    local out
    out=$("$PROG" --complete "$OUTPUT/natlang.ixml" -i "$1" | tr '\t' '|')
    [ "$out" = "$2" ] || { echo "input: '$1'"; echo "got:"; echo "$out"; echo "expected:"; echo "$2"; fail "wrong completions"; }
}

# The words of the rules that can come first.
check "" "string|det|a
string|det|the
string|noun|ball
string|noun|man
string|noun|woman"

# The rest of a word.
check "a man gi" "string|verb|ves"

# A complete word is followed by a space (the rule s).
check "a man" "string|s| "

# The input is a sentence, it could also end here.
check "a man is " "string|det|a
string|det|the
string|noun|ball
string|noun|man
string|noun|woman
string|s| 
end||"

# An input that can not be read to its end gives nothing and exit 1.
"$PROG" --complete "$OUTPUT/natlang.ixml" -i "x" > "$OUTPUT/x.txt" && fail "a bad input exits 0"
[ -s "$OUTPUT/x.txt" ] && fail "a bad input gives completions"

# A character set and a control character.
printf 'line = word, #a.\nword = ["a"-"z"]+.\n' > "$OUTPUT/line.ixml"
out=$("$PROG" --complete "$OUTPUT/line.ixml" -i "ab" | tr '\t' '|')
[ "$out" = "char|line|#a
charset|word|['a'-'z']" ] || { echo "$out"; fail "wrong charset or char completion"; }

echo "OK: complete"
