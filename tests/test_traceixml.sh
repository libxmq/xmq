#!/bin/bash
# libxmq - Copyright 2026 Fredrik Öhrström (spdx: MIT)
#
# Test xmq --traceixml=file: the trace of an ixml parse, and the stylesheets
# library/xmq-utils/render-trace-to-html.xslq, render-trace-to-tex.xslq and
# render-trace-to-terminal.xslq that render it.
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

# The tree is built top-down, the last symbol first: a parent is added before its children,
# and the first node added in S (step 2, 3 with yaep) is the last M, over 4 at 6-7.
[ "$($PROG "$OUTPUT/trace.xmq" select 'count(/trace/tree/node[@parent >= @step])' to-text)" = "0" ] || fail "a parent is added after its child"
SECOND=2
if [ "$ENGINE" = "yaep" ]; then SECOND=3; fi
[ "$($PROG "$OUTPUT/trace.xmq" select "count(/trace/tree/node[@step = $SECOND and @from = 6])" to-text)" = "1" ] || fail "the tree is not built top-down from the right"

# A file ending with .xml gets the trace as xml.
$PROG --traceixml="$OUTPUT/trace.xml" "$OUTPUT/formula.ixml" -i '1+2*3+4' > /dev/null || fail "parse failed (xml)"
head -c 5 "$OUTPUT/trace.xml" | grep -q '<?xml' || fail "trace.xml is not xml"

# Every tree node marks an item as used in the web page.
$PROG "$OUTPUT/trace.xmq" transform library/xmq-utils/render-trace-to-html.xslq to-html > "$OUTPUT/trace.html" || fail "html render failed"
[ "$(( $(grep -o 'complete used' "$OUTPUT/trace.html" | wc -l) ))" = "$NODES" ] || fail "html does not mark the $NODES used items"
# The tree building table has a row with a new node for every node.
[ "$(( $(grep -o 'node new' "$OUTPUT/trace.html" | wc -l) ))" = "$NODES" ] || fail "html does not show the $NODES added nodes"

$PROG "$OUTPUT/trace.xmq" transform library/xmq-utils/render-trace-to-tex.xslq to-text > "$OUTPUT/trace.tex" || fail "tex render failed"
grep -q 'begin{longtable}' "$OUTPUT/trace.tex" || fail "tex has no table"
[ "$(( $(grep -o 'cellcolor{used}' "$OUTPUT/trace.tex" | wc -l) ))" = "$NODES" ] || fail "tex does not mark the $NODES used items"
[ "$(( $(grep -o 'cellcolor{treenew}' "$OUTPUT/trace.tex" | wc -l) ))" = "$NODES" ] || fail "tex does not show the $NODES added nodes"

# In the terminal: ansi colors, and the tables line up, the │ before the rules is in the same
# column on every row (ω and ↓ are one character, counted as one byte here).
$PROG "$OUTPUT/trace.xmq" transform library/xmq-utils/render-trace-to-terminal.xslq to-text > "$OUTPUT/trace.txt" || fail "terminal render failed"
grep -q $'\x1b\[30;48;5;151m' "$OUTPUT/trace.txt" || fail "terminal output has no ansi colors"
grep -q 'the chosen tree' "$OUTPUT/trace.txt" || fail "terminal output has no chosen tree"
# Not \x1b in the sed pattern and the count as a number: macos (BSD) sed and wc differ.
ESC=$(printf '\033')
COLUMNS_USED=$(( $(sed "s/${ESC}\[[0-9;]*m//g" "$OUTPUT/trace.txt" | grep '│' | sed 's/│.*//; s/ω/w/g; s/↓/v/g' | awk '{ print length($0) }' | sort -u | wc -l) ))
[ "$COLUMNS_USED" = "1" ] || fail "terminal tables do not line up"

# A choice by cost: A and B both match aaa, B costs 1. Both are built, S → B is not chosen.
[ "$($PROG "$OUTPUT/trace.xmq" select "count(/trace/tree/node[@chosen = 'no'])" to-text)" = "0" ] || fail "an unambiguous tree has nodes not chosen"
printf 'S = A | B.\nA = "a"+.\nB =< "a"+.\n' > "$OUTPUT/cost.ixml"
$PROG --traceixml="$OUTPUT/cost.xmq" "$OUTPUT/cost.ixml" -i 'aaa' > "$OUTPUT/cost_tree.xmq" || fail "cost parse failed"
grep -q 'A = aaa' "$OUTPUT/cost_tree.xmq" || fail "the cheapest tree was not chosen"
[ "$($PROG "$OUTPUT/cost.xmq" select "count(/trace/tree/node[@rule = 'S' and @chosen = 'yes' and @cost = 0])" to-text)" = "1" ] || fail "S → A is not chosen with cost 0"
[ "$($PROG "$OUTPUT/cost.xmq" select "count(/trace/tree/node[@rule = 'B' and @chosen = 'no' and @cost = 1])" to-text)" = "1" ] || fail "B is not rejected with cost 1"
$PROG "$OUTPUT/cost.xmq" transform library/xmq-utils/render-trace-to-html.xslq to-html > "$OUTPUT/cost.html" || fail "cost html render failed"
grep -q 'node new rejected' "$OUTPUT/cost.html" || fail "html does not show the rejected nodes"
grep -q 'the chosen tree' "$OUTPUT/cost.html" || fail "html does not show the chosen tree"
$PROG "$OUTPUT/cost.xmq" transform library/xmq-utils/render-trace-to-tex.xslq to-text > "$OUTPUT/cost.tex" || fail "cost tex render failed"
grep -q 'not chosen' "$OUTPUT/cost.tex" || fail "tex does not show the rejected nodes"

# a*b is a declaration (cost 1) or an expression: the letters a and b are built for the type
# and name of the declaration, which is not chosen, and used by the factors of the expression.
printf 'statement = declaration | expression.\ndeclaration =< type, "*", name.\nexpression = factor, "*", factor.\ntype = letter.\nname = letter.\nfactor = letter.\n-letter = ["a"-"z"].\n' > "$OUTPUT/shared.ixml"
$PROG --traceixml="$OUTPUT/shared.xmq" "$OUTPUT/shared.ixml" -i 'a*b' > "$OUTPUT/shared_tree.xmq" || fail "shared parse failed"
grep -q 'expression {' "$OUTPUT/shared_tree.xmq" || fail "the expression was not chosen"
[ "$($PROG "$OUTPUT/shared.xmq" select "count(/trace/tree/node[@chosen = 'yes'][not(@tree-parent)])" to-text)" = "0" ] || fail "a chosen node has no tree-parent"
[ "$($PROG "$OUTPUT/shared.xmq" select "count(/trace/tree/node[@rule = 'letter' and @chosen = 'yes' and @parent != @tree-parent])" to-text)" = "2" ] || fail "the shared letters are not used by another parent"
$PROG "$OUTPUT/shared.xmq" transform library/xmq-utils/render-trace-to-html.xslq to-html > "$OUTPUT/shared.html" || fail "shared html render failed"
$PROG "$OUTPUT/shared.html" select "//tr[@class = 'chosen']" to-xmq | grep -q factor && fail "the chosen tree row does not show the deepest nodes"

# --traceixml=terminal saves trace_formula.xmq (after the grammar formula.ixml) and shows
# it with xsl:xmq-utils/render-trace-to-terminal, the library is taken from XMQ_LIBRARY.
# Only xmq has it, not xmqj.
if [ "$ENGINE" = "yaep" ]
then
    ROOT=$(pwd)
    ABS_PROG=$(cd "$(dirname "$PROG")" && pwd)/$(basename "$PROG")
    rm -f "$OUTPUT/trace_formula.xmq"
    (cd "$OUTPUT" && XMQ_LIBRARY="$ROOT/library" "$ABS_PROG" --traceixml=terminal formula.ixml -i '1+2*3+4' > terminal.txt) || fail "--traceixml=terminal failed"
    [ -s "$OUTPUT/trace_formula.xmq" ] || fail "--traceixml=terminal did not save trace_formula.xmq"
    grep -q 'the chosen tree' "$OUTPUT/terminal.txt" || fail "--traceixml=terminal did not show the trace"
    grep -q $'\x1b\[' "$OUTPUT/terminal.txt" || fail "--traceixml=terminal has no colors"

    # --traceixml=br saves trace_formula.xmq, renders it to trace_formula.html and opens it
    # in the browser. A fake open (the command used to start the browser) records the file.
    ABS_OUT=$(cd "$OUTPUT" && pwd)
    mkdir -p "$OUTPUT/fakebin"
    printf '#!/bin/sh\necho "$1" > "%s/opened.txt"\n' "$ABS_OUT" > "$OUTPUT/fakebin/open"
    chmod +x "$OUTPUT/fakebin/open"
    rm -f "$OUTPUT/trace_formula.xmq" "$OUTPUT/trace_formula.html" "$OUTPUT/opened.txt"
    (cd "$OUTPUT" && PATH="$ABS_OUT/fakebin:$PATH" XMQ_LIBRARY="$ROOT/library" "$ABS_PROG" --traceixml=br formula.ixml -i '1+2*3+4' > br.txt) || fail "--traceixml=br failed"
    [ -s "$OUTPUT/trace_formula.xmq" ] || fail "--traceixml=br did not save trace_formula.xmq"
    grep -q 'the chosen tree' "$OUTPUT/trace_formula.html" || fail "--traceixml=br did not render trace_formula.html"
    [ "$(cat "$OUTPUT/opened.txt" 2>/dev/null)" = "trace_formula.html" ] || fail "--traceixml=br did not open trace_formula.html in the browser"
    grep -q '1+2\*3+4\|S {' "$OUTPUT/br.txt" || fail "--traceixml=br did not print the parse"
    "$PROG" --traceixml=bad "$OUTPUT/formula.ixml" -i 1 > /dev/null 2>&1 && fail "--traceixml=bad is accepted"
    "$PROG" --traceixml="$OUTPUT/trace.json" "$OUTPUT/formula.ixml" -i 1 > /dev/null || fail "--traceixml=trace.json failed"
    head -c 1 "$OUTPUT/trace.json" | grep -q '{' || fail "trace.json is not json"
    XMQ_LIBRARY="$ROOT/library" "$PROG" "$OUTPUT/trace.xmq" transform xsl:xmq-utils/render-trace-to-html to-html > "$OUTPUT/xsl.html" || fail "transform xsl: failed"
    grep -q 'node new' "$OUTPUT/xsl.html" || fail "transform xsl:xmq-utils/render-trace-to-html gave no trace"
fi

echo "OK: traceixml ($ENGINE)"
