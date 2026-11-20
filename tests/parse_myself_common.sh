#!/bin/bash
# libxmq - Copyright 2026 Fredrik Öhrström (spdx: MIT)
#
# The common part of tests/test_c_parse_myself.sh and tests/test_java_parse_myself.sh,
# sourced by them. Parses every file in FILES with GRAMMAR, in parallel with JOBS jobs,
# using the ixml engine of PROG or, when XMQJ is set, a parser generated from GRAMMAR.
# Every file must parse completely and the parse must not be ambiguous.
#
# Expects: PROG GRAMMAR OUTPUT FILES (newline separated) SOURCES (the prefix removed
# from the file names in OUTPUT) WHAT (eg "preprocessed C files") JOBS XMQJ (optional)

PARSER=""
if [ -n "$XMQJ" ]
then
    # Generate and compile the C parser once, it reads a file and prints xmq.
    PARSER=$OUTPUT/cparser
    if ! "$XMQJ" generate-parser --c "$GRAMMAR" --prefix=cparser > "$PARSER.c" 2> "$PARSER.err"
    then
        echo "FAIL: xmqj generate-parser --c $GRAMMAR"
        head -5 "$PARSER.err"
        exit 1
    fi
    if ! cc -O2 -Isrc/main/c -o "$PARSER" "$PARSER.c" $(./tests/xmq_link_libs.sh "$(dirname "$PROG")") 2> "$PARSER.cerr"
    then
        echo "FAIL: compiling the generated parser $PARSER.c"
        head -5 "$PARSER.cerr"
        exit 1
    fi
    echo "Built the generated parser $PARSER"
fi

TOTAL=$(( $(echo "$FILES" | wc -l) ))

# Clear the status line before printing a result, if we are printing status lines.
CLEAR=""
if [ -t 2 ]
then
    CLEAR=$'\r\033[K'
fi

# Parse a single file and print one result line: STATUS seconds file [details]
parse_one()
{
    local src=$1
    local name=$(echo "${src#$SOURCES/}" | tr '/' '_')
    local out=$OUTPUT/$name.xml
    local start=$(date +%s.%N)
    if [ -n "$PARSER" ]
    then
        out=$OUTPUT/$name.xmq
        "$PARSER" "$src" > "$out" 2> "$OUTPUT/$name.stderr"
    else
        "$PROG" --progress "$GRAMMAR" "$src" to-xml > "$out" 2> "$OUTPUT/$name.stderr"
    fi
    local rc=$?
    local secs=$(echo "$(date +%s.%N) - $start" | bc)
    local result
    if [ -n "$PARSER" ] && [ $rc != 0 ] && grep -q ': error: ' "$OUTPUT/$name.stderr"
    then
        # The generated parser prints file:line:col: error: message
        local where=$(grep -m 1 ': error: ' "$OUTPUT/$name.stderr" | sed "s|^.*:\([0-9]*\):\([0-9]*\): error: |line \1 column \2: |")
        result=$(printf "FAIL %6.1fs %s syntax error at %s" "$secs" "$src" "$where")
    elif grep -q 'ixml:state="failed"' "$out"
    then
        local where=$(grep -o '<line>[0-9]*</line><column>[0-9]*</column>' "$out" | sed 's|<line>\([0-9]*\)</line><column>\([0-9]*\)</column>|line \1 column \2|')
        result=$(printf "FAIL %6.1fs %s syntax error at %s" "$secs" "$src" "$where")
    elif [ $rc != 0 ]
    then
        result=$(printf "FAIL %6.1fs %s xmq exited with %s" "$secs" "$src" "$rc")
    elif [ -z "$PARSER" ] && head -c 1000 "$out" | grep -q 'state="ambiguous"'
    then
        result=$(printf "FAIL %6.1fs %s ambiguous parse" "$secs" "$src")
    else
        result=$(printf "OK:  %6.1fs %s" "$secs" "$src")
    fi
    echo "$result" > "$OUTPUT/$name.done"
    printf "%s%s\n" "$CLEAR" "$result"
}
export -f parse_one
export PROG GRAMMAR OUTPUT SOURCES CLEAR PARSER

# Print a single status line on stderr with the number of finished parses
# and the percentage parsed of each running parse, taken from the last
# xmq --progress update (ixml 123/456 ...) written to its stderr file.
show_status()
{
    while true
    do
        local done=$(( $(ls "$OUTPUT"/*.done 2>/dev/null | wc -l) ))
        local line="[$done/$TOTAL done]"
        for f in "$OUTPUT"/*.stderr
        do
            [ -f "$f" ] || continue
            local base=${f%.stderr}
            [ -f "$base.done" ] && continue
            local p=$(tr '\r' '\n' < "$f" | grep '^ixml [0-9]' | tail -n 1 | cut -d ' ' -f 2)
            [ -n "$p" ] || continue
            local pos=${p%/*}
            local len=${p#*/}
            [ "$len" -gt 0 ] 2>/dev/null || continue
            local name=$(basename "$base")
            name=${name%.o.source}
            name=${name%.java}
            name=${name#parts_}
            line="$line $name $((pos*100/len))%"
        done
        # Do not wrap the line on narrow terminals.
        local cols=$(tput cols 2>/dev/null || echo 120)
        printf "\r%s\033[K" "${line:0:$((cols-1))}" >&2
        sleep 1
    done
}

if [ -n "$PARSER" ]
then
    echo "Parsing $TOTAL $WHAT with the parser generated from $GRAMMAR using $JOBS jobs."
else
    echo "Parsing $TOTAL $WHAT with $GRAMMAR using $JOBS jobs."
fi

STATUS_PID=""
if [ -t 2 ]
then
    show_status &
    STATUS_PID=$!
    trap 'kill $STATUS_PID 2>/dev/null' EXIT
fi

echo "$FILES" | xargs -P "$JOBS" -I{} bash -c 'parse_one "$@"' _ {}

if [ -n "$STATUS_PID" ]
then
    kill $STATUS_PID 2>/dev/null
    wait $STATUS_PID 2>/dev/null
    printf "\r\033[K" >&2
fi

FAILED=$(cat "$OUTPUT"/*.done | grep -c "^FAIL")
PASSED=$(cat "$OUTPUT"/*.done | grep -c "^OK:")
echo "====================================="
cat "$OUTPUT"/*.done | grep "^FAIL"
echo "Parse myself passed: $PASSED failed: $FAILED"
[ "$FAILED" = "0" ]
