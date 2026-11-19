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
# Usage: tests/test_c_parse_myself.sh [XMQ_BINARY] [OUTPUT_DIR] [SOURCE_DIR]
#
# SOURCE_DIR defaults to the directory of the XMQ_BINARY, which is where
# the build places the .o.source files.
#
# Set FILTER to only parse some files, either C file names (comma separated)
# e.g. FILTER=ixml.c or FILTER=ixml.c,xmq.c which match the exact file name,
# or a regex matched against the .o.source path, e.g. FILTER=yaep_
# From make: make test_c_parse_myself FILTER=ixml.c
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
# From make: make test_c_parse_myself_fast
#
# When stderr is a terminal, a status line shows the progress of the
# running parses, using xmq --progress (only the ixml engine reports progress).

# Use . as decimal separator when printing the parse times.
export LC_ALL=C

PROG=${1:-./build/x86_64-pc-linux-gnu/release/xmq}
OUTPUT=${2:-build/x86_64-pc-linux-gnu/release/test_output/c_parse_myself}
SOURCES=${3:-$(dirname "$PROG")}
GRAMMAR=library/lang/c.ixml
JOBS=${JOBS:-$(nproc 2>/dev/null || echo 4)}
EXCLUDE=${EXCLUDE-parts/text\.o\.source}

if [ ! -x "$PROG" ] || [ ! -f "$GRAMMAR" ]
then
    echo "Usage: tests/test_c_parse_myself.sh [XMQ_BINARY] [OUTPUT_DIR] [SOURCE_DIR]"
    exit 1
fi

mkdir -p "$OUTPUT"
rm -f "$OUTPUT"/*.xml "$OUTPUT"/*.xmq "$OUTPUT"/*.stderr "$OUTPUT"/*.done

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

WHAT="preprocessed C files"
. tests/parse_myself_common.sh
