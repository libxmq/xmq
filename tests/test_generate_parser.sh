#!/bin/bash
# libxmq - Copyright 2026 Fredrik Öhrström (spdx: MIT)
#
# Test the C parser generator: xmqj generate-parser --c grammar.ixml
#
# For every LL(*) grammar that has test inputs, generate the C parser,
# compile it with libxmq and compare its output with the ixml engine:
#     xmq grammar.ixml input   vs   ./grammar_parser input
# Grammars that are not LL(*) are skipped.
#
# Usage: tests/test_generate_parser.sh [XMQ_BINARY] [XMQJ_BINARY] [OUTPUT_DIR]

PROG=${1:-./build/x86_64-pc-linux-gnu/release/xmq}
XMQJ=${2:-./build/xmqj}
OUTPUT=${3:-build/x86_64-pc-linux-gnu/release/test_output/generate_parser}
LIBDIR=$(dirname "$PROG")
CFLAGS="-O1 -Isrc/main/c"
LIBS="$LIBDIR/libxmq.a -lxslt -lexslt -lxml2 -lz -lm"
# The generated Java parsers are compiled against the xmq jar.
JAR=$(ls build/xmq-*.jar 2>/dev/null | grep -vE -- '-(exec|javadoc|sources)\.jar$' | head -1)

if [ ! -x "$XMQJ" ] || [ ! -f "$LIBDIR/libxmq.a" ]
then
    echo "Build first, make and make xmqj"
    exit 1
fi

mkdir -p "$OUTPUT"
passed=0
failed=0
skipped=0

# compare grammar input...
compare()
{
    local g=$1
    shift
    local name=$(basename "$g" .ixml | tr -c 'A-Za-z0-9_\n' '_')
    if ! "$XMQJ" analyze "$g" > /dev/null 2>&1
    then
        skipped=$((skipped+1))
        [ -n "$VERBOSE" ] && echo "SKIP: $g"
        return
    fi
    if ! "$XMQJ" generate-parser --c "$g" > "$OUTPUT/$name.c" 2> "$OUTPUT/$name.err"
    then
        echo "FAIL: generate $g: $(head -1 "$OUTPUT/$name.err")"
        failed=$((failed+1))
        return
    fi
    if ! cc $CFLAGS -o "$OUTPUT/$name" "$OUTPUT/$name.c" $LIBS 2> "$OUTPUT/$name.cerr"
    then
        echo "FAIL: compile $g"
        head -5 "$OUTPUT/$name.cerr"
        failed=$((failed+1))
        return
    fi
    local cls=""
    if [ -n "$JAR" ]
    then
        cls=P_$name
        mkdir -p "$OUTPUT/java"
        if ! "$XMQJ" generate-parser --java "$g" --class=$cls > "$OUTPUT/java/$cls.java" 2> "$OUTPUT/$name.jerr" \
           || ! javac -nowarn -d "$OUTPUT/java" -cp "$JAR" "$OUTPUT/java/$cls.java" 2> "$OUTPUT/$name.jcerr"
        then
            echo "FAIL: generate or compile java $g"
            head -5 "$OUTPUT/$name.jerr" "$OUTPUT/$name.jcerr"
            failed=$((failed+1))
            cls=""
        fi
    fi
    for input in "$@"
    do
        local expected=$("$PROG" "$g" "$input" 2>&1)
        check_one "$g" "$input" "$expected" C "$OUTPUT/$name"
        [ -n "$cls" ] && check_one "$g" "$input" "$expected" Java java -cp "$OUTPUT/java:$JAR" $cls
    done
}

# check_one grammar input expected kind parser...
check_one()
{
    local g=$1 input=$2 expected=$3 kind=$4
    shift 4
    local got=$("$@" "$input" 2>&1)
    if echo "$expected" | grep -q "state = failed"
    then
        # Rejected by the ixml engine, the generated parser must reject too.
        if "$@" "$input" > /dev/null 2>&1
        then
            echo "FAIL: $kind $g accepted $input, the ixml engine rejects it"
            failed=$((failed+1))
        else
            passed=$((passed+1))
        fi
    elif [ "$expected" = "$got" ]
    then
        passed=$((passed+1))
    else
        echo "FAIL: $kind $g $input"
        diff <(echo "$expected") <(echo "$got") | head -10
        failed=$((failed+1))
    fi
}

# The ixml conformance grammars.
for g in tests/ixml/correct/*.ixml
do
    inp=${g%.ixml}.inp
    [ -f "$inp" ] && compare "$g" "$inp"
done

# The library grammars with test inputs.
for g in library/*/*.ixml
do
    dir=$(dirname "$g")/test-$(basename "$g" .ixml)
    inputs=$(ls "$dir"/*.inp 2>/dev/null)
    [ -n "$inputs" ] && compare "$g" $inputs
done

# The not (!), cost (=<) and left recursion tests, grammar and input from the test file.
mkdir -p "$OUTPUT/ext"
for t in tests/ixml_not_[0-9][0-9][0-9]_*.test tests/ixml_cost_[0-9][0-9][0-9]_*.test tests/ixml_leftrec_[0-9][0-9][0-9]_*.test
do
    n=$(basename "$t" .test)
    sed -n '/^START$/,/^INPUT$/p' "$t" | tail -n +2 | sed '$d' > "$OUTPUT/ext/$n.ixml"
    sed -n '/^INPUT$/,/^OUTPUT$/p' "$t" | tail -n +2 | sed '$d' > "$OUTPUT/ext/$n.inp"
    if [ "1" = "$(wc -l < "$OUTPUT/ext/$n.inp")" ]
    then
        # Like test_ixml_not.sh, a single line input has no newline.
        printf '%s' "$(cat "$OUTPUT/ext/$n.inp")" > "$OUTPUT/ext/$n.inp"
    fi
    compare "$OUTPUT/ext/$n.ixml" "$OUTPUT/ext/$n.inp"
done

# The xmq command line grammar on the joined case lines.
cc -o "$OUTPUT/cli_join" tests/cli_join_main.c || exit 1
mkdir -p "$OUTPUT/cli"
n=0
check()
{
    shift 2
    n=$((n+1))
    "$OUTPUT/cli_join" "$@" | tr -d '\n' > "$OUTPUT/cli/case$n.txt"
}
. tests/cli_grammar_cases.sh
compare library/xmq-utils/xmqcli.ixml "$OUTPUT"/cli/case*.txt

# The committed command line parser must be what the generator produces.
if "$XMQJ" generate-parser --c library/xmq-utils/xmqcli.ixml --prefix=xmqcli > "$OUTPUT/xmq-cli-parser.h" \
   && cmp -s "$OUTPUT/xmq-cli-parser.h" src/main/c/xmq-cli-parser.h
then
    passed=$((passed+1))
else
    echo "FAIL: src/main/c/xmq-cli-parser.h is not up to date with library/xmq-utils/xmqcli.ixml, run make generate_parsers"
    failed=$((failed+1))
fi
if "$XMQJ" generate-parser --java library/xmq-utils/xmqcli.ixml --package=org.libxmq.imp --class=XMQCliParser > "$OUTPUT/XMQCliParser.java" \
   && cmp -s "$OUTPUT/XMQCliParser.java" src/main/java/org/libxmq/imp/XMQCliParser.java
then
    passed=$((passed+1))
else
    echo "FAIL: src/main/java/org/libxmq/imp/XMQCliParser.java is not up to date with library/xmq-utils/xmqcli.ixml, run make generate_parsers"
    failed=$((failed+1))
fi

echo "Generated parsers passed: $passed failed: $failed (grammars skipped, not LL(*): $skipped)"
[ "$failed" = "0" ]
