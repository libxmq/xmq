#!/bin/bash
# libxmq - Copyright 2026 Fredrik Öhrström (spdx: MIT)
#
# Parse every preprocessed C file of xmq itself using the C grammar
# library/lang/c.ixml. The build already runs gcc -E for every C file
# and stores the result next to the object file as foo.o.source,
# e.g. build/x86_64-pc-linux-gnu/release/parts/yaep_vlobject.o.source
#
# Every file must parse completely and the parse must not be ambiguous.
#
# Usage: tests/test_parse_myself.sh [XMQ_BINARY] [OUTPUT_DIR] [SOURCE_DIR]
#
# SOURCE_DIR defaults to the directory of the XMQ_BINARY, which is where
# the build places the .o.source files.
#
# Set FILTER to only parse some files, either C file names (comma separated)
# e.g. FILTER=ixml.c or FILTER=ixml.c,xmq.c which match the exact file name,
# or a regex matched against the .o.source path, e.g. FILTER=yaep_
# From make: make test_parse_myself FILTER=ixml.c
# Set JOBS to limit the number of parallel parses.
# Set EXCLUDE to a regex of files to skip. The default skips parts/text.c
# since it is 10MB of automatically generated unicode category tables,
# set EXCLUDE= (empty) to parse it as well. EXCLUDE is not used when FILTER is set.
#
# Set XMQJ to the xmqj binary (e.g. XMQJ=build/xmqj) to use a parser generated
# from the grammar (xmqj generate-parser --c) instead of the ixml engine.
# The generated parser is built once and parses all files in a few seconds,
# the ixml engine needs minutes. The generated parser only detects syntax
# errors, it is LL(*)/ALL(*) and cannot report ambiguities.
# From make: make test_parse_myself_fast
#
# When stderr is a terminal, a status line shows the progress of the
# running parses, using xmq --progress (only the ixml engine reports progress).

# Use . as decimal separator when printing the parse times.
export LC_ALL=C

PROG=${1:-./build/x86_64-pc-linux-gnu/release/xmq}
OUTPUT=${2:-build/x86_64-pc-linux-gnu/release/test_output/parse_myself}
SOURCES=${3:-$(dirname "$PROG")}
GRAMMAR=library/lang/c.ixml
JOBS=${JOBS:-$(nproc 2>/dev/null || echo 4)}
EXCLUDE=${EXCLUDE-parts/text\.o\.source}

if [ ! -x "$PROG" ] || [ ! -f "$GRAMMAR" ]
then
    echo "Usage: tests/test_parse_myself.sh [XMQ_BINARY] [OUTPUT_DIR] [SOURCE_DIR]"
    exit 1
fi

mkdir -p "$OUTPUT"
rm -f "$OUTPUT"/*.xml "$OUTPUT"/*.xmq "$OUTPUT"/*.stderr "$OUTPUT"/*.done

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

FILES=$(find "$SOURCES" -name "*.o.source" | sort)
if [ -n "$FILTER" ]
then
    if [[ "$FILTER" =~ ^[A-Za-z0-9_.,-]+\.c$ ]]
    then
        # C file names, ixml.c,xmq.c becomes the regex (^|/)(ixml|xmq)\.o\.source$
        NAMES=$(echo "$FILTER" | sed 's/\.c,/|/g; s/\.c$//; s/\./\\./g')
        FILTER="(^|/)($NAMES)\.o\.source\$"
    fi
    FILES=$(echo "$FILES" | grep -E "$FILTER")
elif [ -n "$EXCLUDE" ]
then
    FILES=$(echo "$FILES" | grep -v -E "$EXCLUDE")
fi
if [ -z "$FILES" ]
then
    echo "No .o.source files found in $SOURCES, run make first."
    exit 1
fi
TOTAL=$(echo "$FILES" | wc -l)

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
        local done=$(ls "$OUTPUT"/*.done 2>/dev/null | wc -l)
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
            local name=$(basename "$base" .o.source)
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
    echo "Parsing $TOTAL preprocessed C files with the parser generated from $GRAMMAR using $JOBS jobs."
else
    echo "Parsing $TOTAL preprocessed C files with $GRAMMAR using $JOBS jobs."
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
