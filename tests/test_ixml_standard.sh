#!/bin/bash
# libxmq - Copyright 2026 Fredrik Öhrström (spdx: MIT)
#
# Run the ixml community group test suite, https://codeberg.org/InvisibleXML/ixml
# (cloned into 3rdparty/ixml if it is not there), with an ixml processor, and
# verify the results with xmq.
#
# Usage: tests/test_ixml_standard.sh [processor] [output dir]
#   processor   xmq (default), coffeepot or markup-blitz (found in PATH or ~/bin)
#
# Environment:
#   XMQ             the xmq binary, used for the plan and the comparisons, and as the
#                   processor xmq (default build/x86_64-pc-linux-gnu/release/xmq)
#   FILTER          only run the tests whose catalog/set/case contains this text
#   UNICODE_VERSION the Unicode version for the dependencies of tests (default 17.0, the
#                   version of xmq's character classes, see the unicode-version-*-diagnostic tests)
#   TIMEOUT         seconds for each test (default 60)
#   IXML_SUITE      the test suite checkout (default 3rdparty/ixml)
#
# How it works:
#   1. xmq transforms the top catalog, tests/test-catalog.xml, with
#      tests/ixml_standard/catalog-to-tests.xslq into a plan: a shell script of
#      calls to the functions below. It follows the catalogs that a catalog
#      refers to (test-set-ref), resolving file names from each catalog.
#   2. The plan is run: every test case parses its input with the grammar of its
#      test set using the processor. A grammar test checks the grammar itself:
#      that it is rejected, or that its xml form (the grammar parsed with
#      library/core/ixml.ixml) is the expected one.
#   3. The outcome is a result document, a failure (ixml:state=failed), a dynamic
#      error (a failure that names an error code Dnn) or a rejected grammar (no xml
#      at all). A result is compared with the expected results (alternatives)
#      using the canonical form from tests/ixml_standard/canonical.xslq, which
#      compares like fn:deep-equal.
#   4. The results are written to OUTPUT/results.txt, one line per test, the
#      failures with how to reproduce them to OUTPUT/failures.txt, and a summary
#      for each catalog is printed. The exit code is 1 if a test failed.

PROCESSOR=${1:-xmq}
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
OUTPUT=${2:-$ROOT/build/test_output/ixml_standard/$PROCESSOR}
XMQ=${XMQ:-$ROOT/build/x86_64-pc-linux-gnu/release/xmq}
SUITE=${IXML_SUITE:-$ROOT/3rdparty/ixml}
UNICODE_VERSION=${UNICODE_VERSION:-17.0}
TIMEOUT=${TIMEOUT:-60}
CANON=$ROOT/tests/ixml_standard/canonical.xslq
IXML_IXML=$ROOT/library/core/ixml.ixml

if [ ! -x "$XMQ" ]
then
    echo "No xmq binary $XMQ, run make first or set XMQ."
    exit 1
fi

if [ ! -f "$SUITE/tests/test-catalog.xml" ]
then
    echo "Fetching the ixml test suite into $SUITE"
    git clone -q https://codeberg.org/InvisibleXML/ixml.git "$SUITE" || exit 1
fi

# The processors: run GRAMMAR INPUT OUT ERR, the exit code is returned.
find_program()
{
    if command -v "$1" > /dev/null; then command -v "$1"
    elif [ -x "$HOME/bin/$1" ]; then echo "$HOME/bin/$1"
    fi
}

case "$PROCESSOR" in
    xmq)
        run_processor() { timeout "$TIMEOUT" "$XMQ" --ixml="$1" "$2" to-xml > "$3" 2> "$4"; }
        # The xml form of a grammar: the grammar parsed with the ixml grammar.
        run_grammar_xml() { run_processor "$IXML_IXML" "$1" "$2" "$3"; }
        ;;
    coffeepot)
        PROG=$(find_program coffeepot)
        run_processor() { timeout "$TIMEOUT" "$PROG" -g:"$1" -i:"$2" > "$3" 2> "$4"; }
        run_grammar_xml() { run_processor "$IXML_IXML" "$1" "$2" "$3"; }
        ;;
    markup-blitz)
        PROG=$(find_program markup-blitz)
        run_processor() { timeout "$TIMEOUT" "$PROG" "$1" "$2" > "$3" 2> "$4"; }
        # Without a grammar markup-blitz parses the input with the ixml grammar.
        run_grammar_xml() { timeout "$TIMEOUT" "$PROG" "$1" > "$2" 2> "$3"; }
        ;;
    *)
        echo "Unknown processor $PROCESSOR, use xmq, coffeepot or markup-blitz."
        exit 1
        ;;
esac
if [ "$PROCESSOR" != xmq ] && [ -z "$PROG" ]
then
    echo "Cannot find $PROCESSOR in PATH or ~/bin."
    exit 1
fi

rm -rf "$OUTPUT"
mkdir -p "$OUTPUT/work" "$OUTPUT/grammars" "$OUTPUT/failures"
WORK=$OUTPUT/work
RESULTS=$OUTPUT/results.txt
FAILURES=$OUTPUT/failures.txt
: > "$RESULTS"
: > "$FAILURES"

# 1. The plan.
CATALOG=$SUITE/tests/test-catalog.xml
if ! "$XMQ" --trim=none "$CATALOG" transform --stringparam=catalog="$CATALOG" \
     "$ROOT/tests/ixml_standard/catalog-to-tests.xslq" to-text > "$OUTPUT/plan.sh"
then
    echo "Could not make the test plan from $CATALOG"
    exit 1
fi

# The canonical form of an xml file, or of the K:th assert-xml of a catalog.
canon()
{
    if [ -n "$2" ]
    then
        "$XMQ" --xml --trim=none "$1" transform --stringparam=which="$2" "$CANON" to-text 2> /dev/null
    else
        "$XMQ" --xml --trim=none "$1" transform "$CANON" to-text 2> /dev/null
    fi
}

# The expected results are the same for every processor, remember them.
declare -A EXPECTED_CANON
expected_canon()
{
    local key="$1#$2"
    if [ -z "${EXPECTED_CANON[$key]+x}" ]
    then
        EXPECTED_CANON[$key]=$(canon "$1" "$2"; echo .)
    fi
    printf '%s' "${EXPECTED_CANON[$key]%.}"
}

# 2. The functions called by the plan.
CATALOG_NAME=""
DEPTH=0
declare -a SET_NAME SET_GRAMMAR SET_DEPS
NUM_GRAMMARS=0
NUM_TESTS=0

catalog() { CATALOG_NAME=${1#$SUITE/tests/}; }

set_begin()
{
    DEPTH=$((DEPTH+1))
    SET_NAME[$DEPTH]=$1
    SET_GRAMMAR[$DEPTH]=${SET_GRAMMAR[$((DEPTH-1))]}
    SET_DEPS[$DEPTH]=""
    IN_TEST=""
}

set_end() { DEPTH=$((DEPTH-1)); }

grammar_ref() { SET_GRAMMAR[$DEPTH]=$1; }

grammar_inline()
{
    NUM_GRAMMARS=$((NUM_GRAMMARS+1))
    local g="$OUTPUT/grammars/$NUM_GRAMMARS.ixml"
    printf '%s' "$1" > "$g"
    SET_GRAMMAR[$DEPTH]=$g
}

grammar_vxml() { SET_GRAMMAR[$DEPTH]="vxml:$1"; }

# Dependencies of a set or a test, alternatives are separated by |.
depends()
{
    if [ -n "$IN_TEST" ]
    then
        TEST_DEPS="$TEST_DEPS|$1"
    else
        SET_DEPS[$DEPTH]="${SET_DEPS[$DEPTH]}|$1"
    fi
}

begin_test()
{
    IN_TEST=$1
    TEST_NAME=$2
    TEST_DEPS=""
    INPUT=""
    EXPECT=()
}

grammar_test_begin() { begin_test grammar "(grammar)"; }
case_begin() { begin_test case "$1"; }

input_ref() { INPUT=$1; }
input_inline() { printf '%s' "$1" > "$WORK/inline-input.txt"; INPUT=$WORK/inline-input.txt; }

expect_xml_ref() { EXPECT+=("xml	$1	"); }
expect_xml_inline() { EXPECT+=("xml	$1	$2"); }
expect_not_a_sentence() { EXPECT+=("not-a-sentence"); }
expect_not_a_grammar() { EXPECT+=("not-a-grammar"); }
expect_dynamic_error() { EXPECT+=("dynamic-error"); }
expect_unknown() { EXPECT+=("unknown	$1"); }

# Is one of the alternatives (|a=b;c=d|...) satisfied? Empty is satisfied.
deps_ok()
{
    [ -z "$1" ] && return 0
    local alt
    IFS='|' read -r -a alts <<< "${1#|}"
    for alt in "${alts[@]}"
    do
        local ok=1 cond
        IFS=';' read -r -a conds <<< "$alt"
        for cond in "${conds[@]}"
        do
            [ "$cond" = "Unicode-version=$UNICODE_VERSION" ] || ok=0
        done
        [ "$ok" = 1 ] && return 0
    done
    return 1
}

# The outcome of a run: xml, failed, dynamic-error, not-a-grammar, timeout or crash.
# A processor may report a dynamic error as a failure document or only on stderr.
classify()
{
    local rc=$1 out=$2 err=$3
    if [ "$rc" = 124 ]; then OUTCOME=timeout; return; fi
    if [ "$rc" -ge 128 ]; then OUTCOME=crash; return; fi
    if grep -q "state=[\"']failed[\"']" "$out"
    then
        if grep -qE '\bD[0-9][0-9]\b' "$out"; then OUTCOME=dynamic-error; else OUTCOME=failed; fi
        return
    fi
    if canon "$out" > "$WORK/actual.canon" && [ -s "$WORK/actual.canon" ]
    then
        OUTCOME=xml
    elif grep -qE '\bD[0-9][0-9]\b' "$err"
    then
        OUTCOME=dynamic-error
    else
        OUTCOME=not-a-grammar
    fi
}

record()
{
    local status=$1 detail=$2
    local id="$CATALOG_NAME ${SET_NAME[$DEPTH]} $TEST_NAME"
    printf '%s\t%s\t%s\t%s\t%s\n' "$status" "$CATALOG_NAME" "${SET_NAME[$DEPTH]}" "$TEST_NAME" "$detail" >> "$RESULTS"
    if [ "$status" = FAIL ]
    then
        local dir="$OUTPUT/failures/$NUM_TESTS"
        mkdir -p "$dir"
        cp "$WORK/out.xml" "$dir/output.xml" 2> /dev/null
        cp "$WORK/err.txt" "$dir/stderr.txt" 2> /dev/null
        cp "$WORK/actual.canon" "$dir/actual.canon" 2> /dev/null
        cp "$WORK/input.txt" "$dir/input.txt" 2> /dev/null
        RUN=${RUN//"$WORK/input.txt"/"$dir/input.txt"}
        {
            echo "FAIL $id: $detail"
            echo "    expected: ${EXPECT[*]}" | tr '\t' ' '
            echo "    got:      $OUTCOME, see $dir"
            echo "    run:      $RUN"
        } >> "$FAILURES"
    fi
}

test_end()
{
    local kind=$IN_TEST
    IN_TEST=""
    local id="$CATALOG_NAME/${SET_NAME[$DEPTH]}/$TEST_NAME"
    if [ -n "$FILTER" ] && [[ "$id" != *"$FILTER"* ]]; then return; fi
    NUM_TESTS=$((NUM_TESTS+1))

    local d
    for ((d = 1; d <= DEPTH; d++))
    do
        if ! deps_ok "${SET_DEPS[$d]}"; then record N/A "needs ${SET_DEPS[$d]#|}"; return; fi
    done
    if ! deps_ok "$TEST_DEPS"; then record N/A "needs ${TEST_DEPS#|}"; return; fi

    local grammar=${SET_GRAMMAR[$DEPTH]}
    if [[ "$grammar" == vxml:* ]]; then record N/A "the grammar is in xml form"; return; fi
    if [ -z "$grammar" ]; then record FAIL "no grammar in the catalog"; return; fi

    # The input is copied, so that its name does not decide how it is read.
    local expects_xml=""
    local e
    for e in "${EXPECT[@]}"; do [[ "$e" == xml* ]] && expects_xml=1; done
    rm -f "$WORK/out.xml" "$WORK/err.txt" "$WORK/actual.canon"
    local rc
    if [ "$kind" = grammar ]
    then
        cp "$grammar" "$WORK/input.txt"
        if [ -n "$expects_xml" ]
        then
            RUN="xmq --ixml=$IXML_IXML $WORK/input.txt to-xml"
            run_grammar_xml "$WORK/input.txt" "$WORK/out.xml" "$WORK/err.txt"
        else
            # Is the grammar accepted? Parse an empty input with it.
            : > "$WORK/empty.txt"
            RUN="xmq --ixml=$grammar -i '' to-xml"
            run_processor "$grammar" "$WORK/empty.txt" "$WORK/out.xml" "$WORK/err.txt"
        fi
        rc=$?
    else
        cp "$INPUT" "$WORK/input.txt"
        RUN="xmq --ixml=$grammar $WORK/input.txt to-xml"
        run_processor "$grammar" "$WORK/input.txt" "$WORK/out.xml" "$WORK/err.txt"
        rc=$?
    fi
    classify "$rc" "$WORK/out.xml" "$WORK/err.txt"
    # Parsing an empty input with an accepted grammar may fail, the grammar is fine.
    if [ "$kind" = grammar ] && [ -z "$expects_xml" ] && [ "$OUTCOME" = failed ]; then OUTCOME=grammar-accepted; fi

    local actual=""
    [ "$OUTCOME" = xml ] && actual=$(cat "$WORK/actual.canon"; echo .)
    for e in "${EXPECT[@]}"
    do
        case "$e" in
            xml*)
                local file k
                IFS=$'\t' read -r _ file k <<< "$e"
                if [ "$OUTCOME" = xml ] && [ "$(expected_canon "$file" "$k")." = "$actual" ]
                then
                    record PASS "xml"; return
                fi
                ;;
            not-a-sentence) [ "$OUTCOME" = failed ] && { record PASS "$OUTCOME"; return; } ;;
            not-a-grammar) [ "$OUTCOME" = not-a-grammar ] && { record PASS "$OUTCOME"; return; } ;;
            dynamic-error) [ "$OUTCOME" = dynamic-error ] && { record PASS "$OUTCOME"; return; } ;;
        esac
    done
    if [ "$OUTCOME" = xml ]; then record FAIL "xml differs"; else record FAIL "got $OUTCOME"; fi
}

# 3. Run the plan.
source "$OUTPUT/plan.sh"

# 4. The summary.
echo "ixml test suite with $PROCESSOR ($(cd "$SUITE" && git log -1 --format=%h 2> /dev/null))"
awk -F'\t' '{ n[$2]++; c[$2 "\t" $1]++ }
     END { for (cat in n) printf "  %-45s %4d pass %4d fail %4d n/a\n", cat, c[cat "\tPASS"], c[cat "\tFAIL"], c[cat "\tN/A"] }' "$RESULTS" | sort
awk -F'\t' '{ t[$1]++ } END { printf "  %-45s %4d pass %4d fail %4d n/a\n", "total", t["PASS"], t["FAIL"], t["N/A"] }' "$RESULTS"
echo "Results: $RESULTS"
if [ -s "$FAILURES" ]
then
    echo "Failures: $FAILURES"
    exit 1
fi
exit 0
