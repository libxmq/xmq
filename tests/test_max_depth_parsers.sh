#!/bin/bash
# libxmq - Copyright 2026 Fredrik Öhrström (spdx: MIT)
#
# The parsers accept at most 10000 nested elements: xmq bodies, json objects and
# arrays, xml elements, yaml collections and the elements of an ixml parse. A
# document nested 10000 deep is parsed and printed as xmq, xml and json, one
# nested 10001 deep is an error (and not a stack overflow).
#
# Usage: tests/test_max_depth_parsers.sh <xmq or xmqj binary> <output dir>

PROG=$1
OUTPUT=$2/max_depth_parsers
MAX=10000

if [ -z "$PROG" ] || [ -z "$2" ]
then
    echo "Usage: tests/test_max_depth_parsers.sh <xmq or xmqj binary> <output dir>"
    exit 1
fi

mkdir -p "$OUTPUT"

FEATURES=" $($PROG --features) "
has_feature()
{
    case "$FEATURES" in *" $1 "*) return 0 ;; esac
    return 1
}

passed=0
failed=0

# repeat <string> <count>
repeat()
{
    local s=$1 n=$2 out=""
    local i
    for ((i = 0; i < n; i++)); do out+=$s; done
    printf '%s' "$out"
}

# generate <language> <depth> <file>, the innermost element is empty, so the
# resulting document has exactly <depth> nested elements.
generate()
{
    local lang=$1 n=$2 file=$3
    case "$lang" in
        xmq)  { repeat 'a{' $n; repeat '}' $n; echo; } > "$file" ;;
        json) { repeat '[' $n; repeat ']' $n; echo; } > "$file" ;;
        xml)  { repeat '<a>' $n; repeat '</a>' $n; echo; } > "$file" ;;
        # A block sequence of sequences, the innermost is the empty flow sequence [].
        yaml) { repeat '- ' $((n-1)); echo '[]'; } > "$file" ;;
        # The grammar a: "(", a?, ")". gives an element a for every pair of parentheses.
        ixml) { repeat '(' $n; repeat ')' $n; } > "$file" ;;
    esac
}

# count_elements <file.xml>, the number of start tags.
count_elements()
{
    grep -o '<[a-z_]' "$1" | wc -l
}

check()
{
    local lang=$1
    local file=$OUTPUT/depth_$MAX.$lang
    local over=$OUTPUT/depth_$((MAX+1)).$lang
    local cmd=()
    local expect

    generate "$lang" $MAX "$file"
    generate "$lang" $((MAX+1)) "$over"

    if [ "$lang" = "ixml" ]
    then
        printf 'a: "(", a?, ")".\n' > "$OUTPUT/depth.ixml"
        cmd=("$PROG" "$OUTPUT/depth.ixml")
        expect="nesting is too deep"
    else
        cmd=("$PROG")
        if [ "$lang" = "xml" ]; then expect="Excessive depth in document: $MAX"; else expect="nesting is too deep"; fi
    fi

    # 10000 deep is parsed and printed.
    local ok=true
    for out in to-xml to-xmq to-json
    do
        if ! "${cmd[@]}" "$file" $out > "$OUTPUT/out_$lang.$out" 2> "$OUTPUT/err_$lang.$out"
        then
            echo "ERR: max depth $lang $MAX $out failed: $(head -c 200 "$OUTPUT/err_$lang.$out")"
            ok=false
        fi
    done
    if $ok
    then
        local n
        n=$(count_elements "$OUTPUT/out_$lang.to-xml")
        if [ "$n" != "$MAX" ]
        then
            echo "ERR: max depth $lang $MAX gave $n elements, expected $MAX"
            ok=false
        fi
    fi

    # 10001 deep is an error.
    "${cmd[@]}" "$over" to-xml > "$OUTPUT/over_$lang.out" 2> "$OUTPUT/over_$lang.err"
    local rc=$?
    if [ "$rc" -gt 128 ]
    then
        echo "ERR: max depth $lang $((MAX+1)) crashed (exit code $rc)"
        ok=false
    elif ! grep -q "$expect" "$OUTPUT/over_$lang.out" "$OUTPUT/over_$lang.err"
    then
        echo "ERR: max depth $lang $((MAX+1)) did not report: $expect"
        ok=false
    fi

    if $ok
    then
        passed=$((passed+1))
    else
        failed=$((failed+1))
    fi
}

check xmq
check xml
if has_feature json; then check json; else echo "SKIP: max depth json (needs json)"; fi
if has_feature yaml; then check yaml; else echo "SKIP: max depth yaml (needs yaml)"; fi
if has_feature ixml; then check ixml; else echo "SKIP: max depth ixml (needs ixml)"; fi

if [ "$failed" = "0" ]
then
    echo "OK: max depth parsers ($passed languages accept $MAX and reject $((MAX+1)) nested elements)"
    exit 0
fi
echo "ERR: max depth parsers passed: $passed failed: $failed"
exit 1
