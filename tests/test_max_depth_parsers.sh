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
    echo $(( $(grep -o '<[a-z_]' "$1" | wc -l) ))
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

# chars <char> <count>, the char repeated count times.
chars()
{
    head -c "$2" /dev/zero | tr '\0' "$1"
}

# no_crash <name> <expected output> <command...>, the command must not crash (exhaust
# the stack) and must print the expected output.
no_crash()
{
    local name=$1 expect=$2
    shift 2
    "$@" > "$OUTPUT/$name.out" 2> "$OUTPUT/$name.err"
    local rc=$?
    if [ "$rc" -gt 128 ]
    then
        echo "ERR: max depth $name crashed (exit code $rc)"
        failed=$((failed+1))
    elif ! grep -q -- "$expect" "$OUTPUT/$name.out" "$OUTPUT/$name.err"
    then
        echo "ERR: max depth $name did not report: $expect"
        failed=$((failed+1))
    fi
}

if has_feature ixml
then
    # Nested comments in an ixml grammar are counted, not parsed recursively.
    { chars '{' 200000; chars '}' 200000; echo ' s: "x".'; } > "$OUTPUT/comments.ixml"
    printf 'x' > "$OUTPUT/x.txt"
    no_crash ixml_nested_comments 's = x' "$PROG" "$OUTPUT/comments.ixml" "$OUTPUT/x.txt" to-xmq
    # Nested groups in an ixml grammar are limited.
    { printf 's: '; chars '(' 1001; printf '"x"'; chars ')' 1001; echo '.'; } > "$OUTPUT/groups.ixml"
    no_crash ixml_nested_groups 'groups are nested too deep' "$PROG" "$OUTPUT/groups.ixml" "$OUTPUT/x.txt" to-xmq
    { printf 's: '; chars '(' 1000; printf '"x"'; chars ')' 1000; echo '.'; } > "$OUTPUT/groups_ok.ixml"
    no_crash ixml_nested_groups_ok 's = x' "$PROG" "$OUTPUT/groups_ok.ixml" "$OUTPUT/x.txt" to-xmq
    # A hidden recursive rule gives a deep parse tree without elements, the tree is
    # walked when generating the dom and freed without recursion.
    printf "s: -'(', -s, -')'; 'x'.\n" > "$OUTPUT/hidden.ixml"
    { chars '(' 200000; printf 'x'; chars ')' 200000; } > "$OUTPUT/hidden.txt"
    no_crash ixml_hidden_recursion 's = x' "$PROG" "$OUTPUT/hidden.ixml" "$OUTPUT/hidden.txt" to-xmq
fi

if [ "$failed" = "0" ]
then
    echo "OK: max depth parsers ($passed languages accept $MAX and reject $((MAX+1)) nested elements)"
    exit 0
fi
echo "ERR: max depth parsers passed: $passed failed: $failed"
exit 1
