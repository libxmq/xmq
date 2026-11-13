#!/bin/bash
# libxmq - Copyright 2026 Fredrik Öhrström (spdx: MIT)
#
# Test the xmq command line grammar xmqcli.ixml and the argv joiners.
#
# For every case, the arguments are joined into a command line by the C
# joiner (src/main/c/xmq-cli-join.h) and by the Java joiner
# (org.libxmq.imp.CliJoin, if the jar is built), both must produce the
# expected line. The line is then parsed with the grammar using the C ixml
# engine and the parse tree must match the expected (compact xmq) tree.
#
# Usage: tests/test_cli_grammar.sh [XMQ_BINARY] [OUTPUT_DIR] [--print]
# With --print the results are printed instead of compared, useful when
# adding new cases.

PROG=${1:-./build/x86_64-pc-linux-gnu/release/xmq}
OUTPUT=${2:-build/x86_64-pc-linux-gnu/release/test_output/cli_grammar}
PRINT=$3
GRAMMAR=xmqcli.ixml
JAR=$(ls build/xmq-*[0-9].jar 2>/dev/null | head -1)
if [ -n "$JAR" ] && ! unzip -l "$JAR" 2>/dev/null | grep -q "org/libxmq/imp/CliJoin.class"
then
    echo "Note: $JAR has no CliJoin, skipping the Java joiner, run make xmqj."
    JAR=""
fi

mkdir -p "$OUTPUT"
cc -o "$OUTPUT/cli_join" tests/cli_join_main.c || exit 1

passed=0
failed=0

# check "expected joined line" "expected tree" arg1 arg2 ...
check()
{
    local exp_line="$1"
    local exp_tree="$2"
    shift 2
    local line=$("$OUTPUT/cli_join" "$@")
    local tree=$("$PROG" --ixml=$GRAMMAR -i "$line" to-xmq --compact 2>&1)
    if [ -n "$PRINT" ]
    then
        printf 'check %q %q' "$line" "$tree"
        printf ' %q' "$@"
        echo
        return
    fi
    local ok=true
    if [ "$line" != "$exp_line" ]
    then
        echo "FAIL: C joiner gave: $line"
        echo "      expected:      $exp_line"
        ok=false
    fi
    if [ -n "$JAR" ]
    then
        local jline=$(java -cp "$JAR" org.libxmq.imp.CliJoin "$@")
        if [ "$jline" != "$exp_line" ]
        then
            echo "FAIL: Java joiner gave: $jline"
            echo "      expected:         $exp_line"
            ok=false
        fi
    fi
    if [ "$tree" != "$exp_tree" ]
    then
        echo "FAIL: $line"
        echo "      tree:     $tree"
        echo "      expected: $exp_tree"
        ok=false
    fi
    if $ok
    then
        passed=$((passed+1))
    else
        failed=$((failed+1))
    fi
}

. tests/cli_grammar_cases.sh

if [ -z "$PRINT" ]
then
    echo "CLI grammar passed: $passed failed: $failed"
    [ "$failed" = "0" ]
fi
