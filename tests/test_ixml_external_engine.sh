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
# An engine that does not support an extension (found with a small probe grammar), eg
# coffeepot (kaffepot) has neither ! nor =<, is expected to fail the tests whose grammar
# uses it. Such a test is counted as expected to fail whatever its result (the engine
# rejects the grammar), the grammar is checked for the extensions with
# library/core/ixmlnotcost.ixml.
#
# A known difference of an engine, that is not about the extensions, is listed in
# tests/ixml_engine_known_differences.txt with its reason and is reported as such.
#
# Usage: tests/test_ixml_external_engine.sh <xmq binary> <output dir> <engine>
#   make test_markup_blitz
#   make test_kaffepot

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
total_expected=0
total_known=0

# known_difference <test name>: prints the reason if the test is a known difference of the engine.
known_difference()
{
    awk -v e="$ENGINE" -v t="$1" '$1 == e && $2 == t { $1 = ""; $2 = ""; sub(/^ +/, ""); print; exit }' tests/ixml_engine_known_differences.txt
}

# Does the engine support the extensions? A probe grammar for each.
printf "s = 'a', !'b', ['a'-'z'].\n" > "$OUTPUT/probe_not.ixml"
printf "s = x | y. x =< 'a'. y = 'a'.\n" > "$OUTPUT/probe_cost.ixml"
printf 'ac' > "$OUTPUT/probe_not.inp"
printf 'a' > "$OUTPUT/probe_cost.inp"
supports_not=no
supports_cost=no
"$PROG" "$OUTPUT/probe_not.ixml" "$OUTPUT/probe_not.inp" 2>/dev/null | grep -q '^s = ac$' && supports_not=yes
"$PROG" "$OUTPUT/probe_cost.ixml" "$OUTPUT/probe_cost.inp" 2>/dev/null | grep -q '^ *y = a$' && supports_cost=yes

# expected_failure <grammar file>: true if the grammar uses an extension that the engine
# does not support, then the test is expected to fail.
expected_failure()
{
    [ -f "$1" ] || return 1
    local nots
    local costs
    nots=$(env -u XMQ_IXML_ENGINE "$PROG" library/core/ixmlnotcost.ixml "$1" select 'count(//not)' to-text 2>/dev/null)
    costs=$(env -u XMQ_IXML_ENGINE "$PROG" library/core/ixmlnotcost.ixml "$1" select 'count(//@cost)' to-text 2>/dev/null)
    if [ "$supports_not" = no ] && [ "${nots:-0}" != 0 ]; then return 0; fi
    if [ "$supports_cost" = no ] && [ "${costs:-0}" != 0 ]; then return 0; fi
    return 1
}

# grammar_of <group> <test file>: the grammar of a test.
grammar_of()
{
    local base
    base=$(basename "$2")
    case "$1" in
        correct) echo "$(dirname "$2")/${base%.output.xmq}.ixml" ;;
        *) echo "$OUTPUT/$1/${base%.test}.ixml" ;;
    esac
}

# run_group <name> <test script> <test files...>
run_group()
{
    local name=$1
    local script=$2
    shift 2
    local ok=0
    local failed=0
    local skipped=0
    local expected=0
    local known=0
    for t in "$@"
    do
        [ -f "$t" ] || continue
        if [ -n "$FILTER" ] && [[ ! "$t" =~ $FILTER ]]; then continue; fi
        local out
        out=$($script "$PROG" "$OUTPUT/$name" "$t" 2>&1)
        if expected_failure "$(grammar_of "$name" "$t")"
        then
            # The grammar uses an extension the engine does not support, whatever the result
            # (the engine rejects the grammar) it is not a comparison.
            expected=$((expected+1))
        elif echo "$out" | grep -q "^OK"
        then
            ok=$((ok+1))
        elif echo "$out" | grep -q "^SKIP"
        then
            skipped=$((skipped+1))
        elif reason=$(known_difference "$(basename "${t%.test}" | sed 's/\.output\.xmq$//')") && [ -n "$reason" ]
        then
            known=$((known+1))
            echo "KNOWN: $t ($reason)"
        else
            failed=$((failed+1))
            echo "FAIL: $t"
            echo "$out" | grep -E "^$ENGINE:|^ERR" | head -3 | sed 's/^/    /'
        fi
    done
    local note=""
    if [ $expected -gt 0 ]; then note="$note, $expected expected to fail (unsupported extension)"; fi
    if [ $known -gt 0 ]; then note="$note, $known known differences"; fi
    if [ $skipped -gt 0 ]; then note="$note, $skipped skipped (only the internal engine)"; fi
    echo "$name: $ok passed, $failed failed$note"
    total_ok=$((total_ok+ok))
    total_failed=$((total_failed+failed))
    total_expected=$((total_expected+expected))
    total_known=$((total_known+known))
}

echo "Running the ixml tests with XMQ_IXML_ENGINE=$ENGINE (supports ! $supports_not, =< $supports_cost)"

run_group correct tests/test_ixml_correct.sh tests/ixml/correct/*.output.xmq
run_group not tests/test_ixml_not.sh tests/ixml_not_[0-9][0-9][0-9]_*.test
run_group cost tests/test_ixml_not.sh tests/ixml_cost_[0-9][0-9][0-9]_*.test
run_group leftrec tests/test_ixml_not.sh tests/ixml_leftrec_[0-9][0-9][0-9]_*.test
run_group parse tests/test_ixml_parse.sh tests/ixml_parse_[0-9][0-9][0-9]_*.test

# The library tests, they print their own pass/FAIL lines.
if [ -z "$FILTER" ] || [[ "library" =~ $FILTER ]]
then
    tests/test_library.sh "$PROG" "$OUTPUT/library" > "$OUTPUT/library.log" 2>&1
    lib_ok=0
    lib_failed=0
    lib_expected=0
    # A result line is: pass:|FAIL: test-<kind>.<test> xmq <kind>, the grammar is
    # library/*/<kind>.ixml. A grammar with an unsupported extension is expected to fail.
    grep -E "^(pass|FAIL): test-" "$OUTPUT/library.log" > "$OUTPUT/library.results"
    while read -r line
    do
        kind=$(echo "$line" | awk '{print $NF}')
        grammar=$(ls library/*/"$kind".ixml 2>/dev/null | head -1)
        if expected_failure "$grammar"
        then
            lib_expected=$((lib_expected+1))
        elif [ "${line%%:*}" = "pass" ]
        then
            lib_ok=$((lib_ok+1))
        else
            lib_failed=$((lib_failed+1))
            echo "$line"
        fi
    done < "$OUTPUT/library.results"
    # The checks that are not about a test directory, eg core/ixml.ixml parses itself.
    other_ok=$(grep "^pass:" "$OUTPUT/library.log" | grep -vc "^pass: test-")
    other_failed=$(grep "^FAIL:" "$OUTPUT/library.log" | grep -v "^FAIL: test-")
    lib_ok=$((lib_ok+other_ok))
    if [ -n "$other_failed" ]; then echo "$other_failed"; lib_failed=$((lib_failed+$(echo "$other_failed" | wc -l))); fi
    note=""
    if [ $lib_expected -gt 0 ]; then note=", $lib_expected expected to fail (unsupported extension)"; fi
    echo "library: $lib_ok passed, $lib_failed failed$note (see $OUTPUT/library.log)"
    total_ok=$((total_ok+lib_ok))
    total_failed=$((total_failed+lib_failed))
    total_expected=$((total_expected+lib_expected))
fi

echo "====================================="
note=""
if [ $total_expected -gt 0 ]; then note=", $total_expected expected to fail (unsupported extension)"; fi
if [ $total_known -gt 0 ]; then note="$note, $total_known known differences"; fi
echo "$ENGINE: $total_ok passed, $total_failed failed$note"
[ $total_failed -eq 0 ]
