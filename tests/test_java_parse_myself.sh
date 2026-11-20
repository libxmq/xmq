#!/bin/bash
# libxmq - Copyright 2026 Fredrik Öhrström (spdx: MIT)
#
# Parse every Java file of xmq itself (the .java files tracked by git) using the
# Java grammar library/lang/java.ixml.
#
# Every file must parse completely and the parse must not be ambiguous.
#
# Usage: tests/test_java_parse_myself.sh [XMQ_BINARY] [OUTPUT_DIR]
#
# Set FILTER to only parse some files, either Java file names (comma separated)
# e.g. FILTER=Main.java or FILTER=Main.java,XMQ.java which match the exact file name,
# or a regex matched against the path, e.g. FILTER=xsd10/
# From make: make test_java_parse_myself FILTER=Main.java
# Set JOBS to limit the number of parallel parses.
# Set EXCLUDE to a regex of files to skip. EXCLUDE is not used when FILTER is set.
#
# Set XMQJ to the xmqj binary (e.g. XMQJ=build/xmqj) to use a parser generated
# from the grammar (xmqj generate-parser --c) instead of the ixml engine.
# From make: make test_java_parse_myself_fast
#
# When stderr is a terminal, a status line shows the progress of the
# running parses, using xmq --progress (only the ixml engine reports progress).

# Use . as decimal separator when printing the parse times.
export LC_ALL=C

PROG=${1:-./build/x86_64-pc-linux-gnu/release/xmq}
OUTPUT=${2:-build/x86_64-pc-linux-gnu/release/test_output/java_parse_myself}
SOURCES=.
GRAMMAR=library/lang/java.ixml
JOBS=${JOBS:-$(nproc 2>/dev/null || echo 4)}
EXCLUDE=${EXCLUDE-}

if [ ! -x "$PROG" ] || [ ! -f "$GRAMMAR" ]
then
    echo "Usage: tests/test_java_parse_myself.sh [XMQ_BINARY] [OUTPUT_DIR]"
    exit 1
fi

mkdir -p "$OUTPUT"
rm -f "$OUTPUT"/*.xml "$OUTPUT"/*.xmq "$OUTPUT"/*.stderr "$OUTPUT"/*.done

FILES=$(git ls-files '*.java' | sort)
if [ -n "$FILTER" ]
then
    if [[ "$FILTER" =~ ^[A-Za-z0-9_.,-]+\.java$ ]]
    then
        # Java file names, Main.java,XMQ.java becomes the regex (^|/)(Main|XMQ)\.java$
        NAMES=$(echo "$FILTER" | sed 's/\.java,/|/g; s/\.java$//; s/\./\\./g')
        FILTER="(^|/)($NAMES)\.java\$"
    fi
    FILES=$(echo "$FILES" | grep -E "$FILTER")
elif [ -n "$EXCLUDE" ]
then
    FILES=$(echo "$FILES" | grep -v -E "$EXCLUDE")
fi
if [ -z "$FILES" ]
then
    echo "No .java files found."
    exit 1
fi

WHAT="Java files"
. tests/parse_myself_common.sh
[ "$FAILED" = "0" ] || exit 1

# With the generated parser (it prints xmq) also run the cleanup library/lang/java.xslq on
# every tree. It must work and leave none of the elements of the raw tree. (The ixml
# engine prints xml, and some trees, eg XmlHtml.java, nest deeper than xmq reads xml.)
if [ -n "$PARSER" ]
then
    clean_one()
    {
        local tree=$1
        local out=${tree%.xmq}.clean
        if ! "$PROG" --trim=none "$tree" transform library/lang/java.xslq > "$out" 2> "$out.err"
        then
            echo "FAIL cleanup $tree: $(head -1 "$out.err")"
        elif grep -qE '^ *(primary|field_select|call|index|reference|outer_new|outer_this|outer_super|class_selector|conditional_expression|leading_comment|rbrace)( |$|\()' "$out"
        then
            echo "FAIL cleanup $tree left a raw element: $(grep -m 1 -E '^ *(primary|field_select|call|index|reference|outer_new|outer_this|outer_super|class_selector|conditional_expression|leading_comment|rbrace)( |$|\()' "$out" | sed 's/^ *//')"
        fi
    }
    export -f clean_one
    CLEAN_FAILED=$(ls "$OUTPUT"/*.java.xmq | xargs -P "$JOBS" -I{} bash -c 'clean_one "$@"' _ {})
    if [ -n "$CLEAN_FAILED" ]
    then
        echo "$CLEAN_FAILED"
        echo "Cleanup failed: $(( $(echo "$CLEAN_FAILED" | wc -l) ))"
        exit 1
    fi
    echo "Cleanup with library/lang/java.xslq passed: $TOTAL"
fi
