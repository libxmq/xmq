#!/bin/bash
# libxmq - Copyright 2026 Fredrik Öhrström (spdx: MIT)
#
# Run the ixml tests with an external ixml engine, eg markup-blitz, invoked by xmq with
# XMQ_IXML_ENGINE=engine, to check that the engines agree, in particular on the
# extensions negative lookahead !factor and the rule costs =< and =<<. The results are
# compared by meaning (tests/compare_ixml_engine.py): a failure must be at the same
# position, but its message is the engine's own, and an ambiguous result must be
# ambiguous for both.
#
# The tests run are the ones where the ixml engine matters:
#   tests/ixml/correct/*            the ixml community group examples
#   tests/ixml_not_*.test           negative lookahead
#   tests/ixml_cost_*.test          rule costs
#   tests/ixml_leftrec_*.test       left recursive grammars
#   tests/ixml_parse_*.test         parsing with ixml grammars
#   library/*/test-*                the library grammars and their tests
# Not the tests of xmq's own engine features, eg --traceixml, --complete or the error
# messages of a bad grammar.
#
# Usage: tests/test_ixml_external_engine.sh <xmq binary> <output dir> <engine>
#   make test_markup_blitz

PROG=$1
OUTPUT=$2
ENGINE=$3

if [ -z "$PROG" ] || [ -z "$OUTPUT" ] || [ -z "$ENGINE" ]
then
    echo "Usage: tests/test_ixml_external_engine.sh <xmq binary> <output dir> <engine>"
    exit 1
fi

if ! command -v "$ENGINE" > /dev/null
then
    echo "SKIP: $ENGINE not found in PATH"
    exit 0
fi

PROG=$(cd "$(dirname "$PROG")" && pwd)/$(basename "$PROG")
mkdir -p "$OUTPUT"
export XMQ_IXML_ENGINE=$ENGINE

total_ok=0
total_failed=0

# run_group <name> <test script> <test files...>
run_group()
{
    local name=$1
    local script=$2
    shift 2
    local ok=0
    local failed=0
    local skipped=0
    for t in "$@"
    do
        [ -f "$t" ] || continue
        if [ -n "$FILTER" ] && [[ ! "$t" =~ $FILTER ]]; then continue; fi
        local out
        out=$($script "$PROG" "$OUTPUT/$name" "$t" 2>&1)
        if echo "$out" | grep -q "^OK"
        then
            ok=$((ok+1))
        elif echo "$out" | grep -q "^SKIP"
        then
            skipped=$((skipped+1))
        else
            failed=$((failed+1))
            echo "FAIL: $t"
            echo "$out" | grep -E "^$ENGINE:|^ERR" | head -3 | sed 's/^/    /'
        fi
    done
    echo "$name: $ok passed, $failed failed$([ $skipped -gt 0 ] && echo ", $skipped skipped (only the internal engine)")"
    total_ok=$((total_ok+ok))
    total_failed=$((total_failed+failed))
}

echo "Running the ixml tests with XMQ_IXML_ENGINE=$ENGINE"

run_group correct tests/test_ixml_correct.sh tests/ixml/correct/*.output.xmq
run_group not tests/test_ixml_not.sh tests/ixml_not_[0-9][0-9][0-9]_*.test
run_group cost tests/test_ixml_not.sh tests/ixml_cost_[0-9][0-9][0-9]_*.test
run_group leftrec tests/test_ixml_not.sh tests/ixml_leftrec_[0-9][0-9][0-9]_*.test
run_group parse tests/test_ixml_parse.sh tests/ixml_parse_[0-9][0-9][0-9]_*.test

# The library tests, they print their own pass/FAIL lines.
if [ -z "$FILTER" ] || [[ "library" =~ $FILTER ]]
then
    tests/test_library.sh "$PROG" "$OUTPUT/library" > "$OUTPUT/library.log" 2>&1
    lib_ok=$(grep -c "^pass:" "$OUTPUT/library.log")
    lib_failed=$(grep -c "^FAIL:" "$OUTPUT/library.log")
    grep "^FAIL:" "$OUTPUT/library.log"
    echo "library: $lib_ok passed, $lib_failed failed (see $OUTPUT/library.log)"
    total_ok=$((total_ok+lib_ok))
    total_failed=$((total_failed+lib_failed))
fi

echo "====================================="
echo "$ENGINE: $total_ok passed, $total_failed failed"
[ $total_failed -eq 0 ]
