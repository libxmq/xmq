#!/bin/bash
# libxmq - Copyright 2026 Fredrik Öhrström (spdx: MIT)
#
# Test xmq --traceixml=file: the trace of an ixml parse, and the stylesheets
# OpenMike/render-trace-to-html.xslq and OpenMike/render-trace-to-tex.xslq that render it.
# Usage: tests/test_traceixml.sh <xmq or xmqj binary> <output dir>

PROG=$1
OUTPUT=$2/traceixml
mkdir -p "$OUTPUT"

fail()
{
    echo "ERR: traceixml $1"
    exit 1
}

printf 'S = S, "+", M | M.\nM = M, "*", T | T.\nT = "1" | "2" | "3" | "4".\n' > "$OUTPUT/formula.ixml"

$PROG --traceixml="$OUTPUT/trace.xmq" "$OUTPUT/formula.ixml" -i '1+2*3+4' > "$OUTPUT/tree.xmq" || fail "parse failed"
[ -s "$OUTPUT/trace.xmq" ] || fail "no trace written"

# 8 rules, 8 tokens (with the end of input), 11 rules in the tree, 12 with yaep (its start rule $).
ENGINE=$(grep -o 'engine = [a-z]*' "$OUTPUT/trace.xmq" | cut -d' ' -f3)
NODES=11
if [ "$ENGINE" = "yaep" ]; then NODES=12; fi
[ "$($PROG "$OUTPUT/trace.xmq" select 'count(/trace/grammar/rule)' to-text)" = "8" ] || fail "wrong number of rules"
[ "$($PROG "$OUTPUT/trace.xmq" select 'count(/trace/input/token)' to-text)" = "8" ] || fail "wrong number of tokens"
[ "$($PROG "$OUTPUT/trace.xmq" select 'count(/trace/tree/node)' to-text)" = "$NODES" ] || fail "wrong number of tree nodes"

# A file ending with .xml gets the trace as xml.
$PROG --traceixml="$OUTPUT/trace.xml" "$OUTPUT/formula.ixml" -i '1+2*3+4' > /dev/null || fail "parse failed (xml)"
head -c 5 "$OUTPUT/trace.xml" | grep -q '<?xml' || fail "trace.xml is not xml"

# Every tree node marks an item as used in the web page.
$PROG "$OUTPUT/trace.xmq" transform OpenMike/render-trace-to-html.xslq to-html > "$OUTPUT/trace.html" || fail "html render failed"
[ "$(grep -o 'complete used' "$OUTPUT/trace.html" | wc -l)" = "$NODES" ] || fail "html does not mark the $NODES used items"

$PROG "$OUTPUT/trace.xmq" transform OpenMike/render-trace-to-tex.xslq to-text > "$OUTPUT/trace.tex" || fail "tex render failed"
grep -q 'begin{longtable}' "$OUTPUT/trace.tex" || fail "tex has no table"
[ "$(grep -o 'cellcolor{used}' "$OUTPUT/trace.tex" | wc -l)" = "$NODES" ] || fail "tex does not mark the $NODES used items"

echo "OK: traceixml ($ENGINE)"
