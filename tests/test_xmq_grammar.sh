#!/bin/bash
# libxmq - Copyright 2026 Fredrik Öhrström (spdx: MIT)
#
# Run every xmq example of the test suite through the xmq grammar
# library/core/xmq.ixml, both with the ixml engine and with the C parser
# generated from the grammar, and compare.
#
# The examples are the .xmq files of the tests and the input sections of the
# .test files that the xmq parser accepts as xmq (duplicates removed). The
# error_*.test files are skipped, their input is meant to be invalid xmq even
# when plain to-xml happens to accept it. Examples with quotes of more than 9
# quote chars are skipped too, a known limitation of xmq.ixml (each quote
# length needs its own rule).
# For each example the result is one of:
#   SAME       both parse it and give the same tree, and the tree transformed
#              by xmq.xslq is the same document as xmq gives (the round trip)
#   ROUNDTRIP  both give the same tree, but the round trip differs
#   DIFF       both parse it but the trees differ
#   REJECT     both reject it (the grammar does not cover it)
#   ENGINE     only the ixml engine accepts it
#   GENERATED  only the generated parser accepts it
#   AMBIGUOUS  the ixml engine reports an ambiguous parse
#
# Usage: tests/test_xmq_grammar.sh [XMQ_BINARY] [XMQJ_BINARY] [OUTPUT_DIR]
# Without XMQJ_BINARY the generated parser is skipped (the C build never needs
# Java), the ixml engine and the round trip are still checked.
# Exit code 0 only if every example is SAME.

export LC_ALL=C
PROG=${1:-./build/x86_64-pc-linux-gnu/release/xmq}
XMQJ=${2:-./build/xmqj}
OUTPUT=${3:-build/x86_64-pc-linux-gnu/release/test_output/xmq_grammar}
GRAMMAR=${GRAMMAR:-library/core/xmq.ixml}
XSLQ=${XSLQ:-${GRAMMAR%.ixml}.xslq}
LIBDIR=$(dirname "$PROG")
TIMEOUT=${TIMEOUT:-60}
JOBS=${JOBS:-$(nproc 2>/dev/null || echo 4)}

if [ ! -x "$PROG" ]
then
    echo "Build first, make"
    exit 1
fi
# The C build never needs Java: without xmqj (or libxmq.a) the generated
# parser is not built and only the ixml engine and the round trip are checked.
GENERATED=yes
if [ ! -x "$XMQJ" ] || [ ! -f "$LIBDIR/libxmq.a" ]
then
    GENERATED=no
    echo "No $XMQJ (make xmqj), skipping the generated parser of $GRAMMAR"
fi

rm -rf "$OUTPUT"
mkdir -p "$OUTPUT/examples" "$OUTPUT/candidates"

# Collect the candidates.
n=0
for f in $(find tests -name "*.xmq" -size -256k | grep -v "test_output" | sort)
do
    n=$((n+1)); cp "$f" "$OUTPUT/candidates/c$n.xmq"; echo "$f" > "$OUTPUT/candidates/c$n.from"
done
for t in tests/*.test
do
    case "$t" in tests/error_*) continue ;; esac
    sec=$(sed -n '/^\(INPUT\|START\)$/,/^[A-Z_]*$/p' "$t" | sed '1d;$d')
    [ -n "$sec" ] || continue
    n=$((n+1)); printf '%s\n' "$sec" > "$OUTPUT/candidates/c$n.xmq"; echo "$t" > "$OUTPUT/candidates/c$n.from"
done

# Keep the ones that the xmq parser accepts as xmq, without duplicates.
declare -A seen
k=0
for f in "$OUTPUT"/candidates/*.xmq
do
    "$PROG" --xmq "$f" to-xml > /dev/null 2>&1 || continue
    # A known limitation of xmq.ixml: quotes of at most 9 quote chars.
    grep -qE "'{10}|\"{10}" "$f" && continue
    sum=$(md5sum < "$f" | cut -d' ' -f1)
    [ -n "${seen[$sum]}" ] && continue
    seen[$sum]=1
    k=$((k+1))
    cp "$f" "$OUTPUT/examples/e$k.xmq"
    cp "${f%.xmq}.from" "$OUTPUT/examples/e$k.from"
done

# Generate the C parser from the grammar.
if [ "$GENERATED" = yes ]
then
    if ! "$XMQJ" generate-parser --c "$GRAMMAR" --prefix=xmqg > "$OUTPUT/xmqg.c" 2> "$OUTPUT/generate.err"
    then
        echo "ERROR: cannot generate a parser from $GRAMMAR:"
        head -20 "$OUTPUT/generate.err"
        exit 1
    fi
    if ! cc -O2 -Isrc/main/c -o "$OUTPUT/xmqg" "$OUTPUT/xmqg.c" $(./tests/xmq_link_libs.sh "$LIBDIR") 2> "$OUTPUT/compile.err"
    then
        echo "ERROR: cannot compile the generated parser:"
        head -20 "$OUTPUT/compile.err"
        exit 1
    fi
fi

# Compare, in parallel.
one()
{
    local f=$1
    local e=${f%.xmq}
    timeout "$TIMEOUT" "$PROG" "$GRAMMAR" "$f" > "$e.engine" 2>&1; local ra=$?
    local rb=$ra
    if [ "$GENERATED" = yes ]
    then
        timeout "$TIMEOUT" "$OUTPUT/xmqg" "$f" > "$e.generated" 2>&1; rb=$?
    else
        cp "$e.engine" "$e.generated"
    fi
    local result
    if grep -q "state = ambiguous\|^AMBIGUOUS" "$e.engine"; then result=AMBIGUOUS
    elif grep -q "state = failed" "$e.engine" || [ $ra != 0 ]
    then
        if [ $rb != 0 ]; then result=REJECT; else result=GENERATED; fi
    elif [ $rb != 0 ]; then result=ENGINE
    elif ! cmp -s "$e.engine" "$e.generated"; then result=DIFF
    else
        # The round trip: the parse tree transformed by xmq.xslq must be the
        # same document as the one the xmq parser builds.
        timeout "$TIMEOUT" "$PROG" "$f" > "$e.xmq_expected" 2>&1
        timeout "$TIMEOUT" "$PROG" "$GRAMMAR" "$f" transform "$XSLQ" > "$e.xmq_roundtrip" 2>&1
        if cmp -s "$e.xmq_expected" "$e.xmq_roundtrip"; then result=SAME; else result=ROUNDTRIP; fi
    fi
    echo "$result $(basename $f) $(cat $e.from)"
}
export -f one
export PROG GRAMMAR XSLQ OUTPUT TIMEOUT GENERATED
ls "$OUTPUT"/examples/*.xmq | xargs -P "$JOBS" -I{} bash -c 'one {}' | sort > "$OUTPUT/results.txt"

echo "xmq grammar $GRAMMAR on $k xmq examples of the test suite:"
for r in SAME ROUNDTRIP DIFF REJECT ENGINE GENERATED AMBIGUOUS
do
    c=$(grep -c "^$r " "$OUTPUT/results.txt")
    [ "$c" != "0" ] && printf '  %-10s %4d\n' "$r" "$c"
done
echo "Details in $OUTPUT/results.txt (example inputs and outputs in $OUTPUT/examples)"
[ "$(grep -vc '^SAME ' "$OUTPUT/results.txt")" = "0" ]
