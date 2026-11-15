#!/bin/sh
# libxmq - Copyright 2026 Fredrik Öhrström (spdx: MIT)
#
# Run the native xslt 1.0 engine over the libxslt test suite in tests/libxslt and
# compare with its expected .out and .err files (see tests/libxslt_suite/runtest.c).
# The tests listed in tests/libxslt_suite/known_failures.txt may fail.
#
# Usage: tests/test_libxslt_suite.sh OUTPUT_DIR

ROOT=$(cd "$(dirname "$0")/.." && pwd)
OUT=${1:-$ROOT/build/libxslt_suite}
mkdir -p "$OUT"
OUT=$(cd "$OUT" && pwd)
CC=${CC:-gcc}
P="$ROOT/src/main/c/parts"
PARTS="$P/always.c $P/membuffer.c $P/hashmap.c $P/xml_dom.c $P/xml_save.c $P/xml_parser.c $P/xml_html.c \
$P/xml_html_parser.c $P/xml_html_save.c $P/xml_chars.c $P/xml_xpath.c $P/xml_uri.c $P/xml_libxslt1_xslt.c \
$P/xml_libxslt1_transform.c"

$CC -O1 -g -I"$ROOT/src/main/c" -o "$OUT/libxslt_runtest" "$ROOT/tests/libxslt_suite/runtest.c" $PARTS -lm || exit 1

DIRS=""
for d in REC2 REC general encoding documents numbers keys namespaces extensions reports exslt/common exslt/crypto exslt/date exslt/dynamic exslt/functions exslt/math exslt/saxon exslt/sets exslt/strings plugins
do
    [ -d "$ROOT/tests/libxslt/tests/$d" ] && DIRS="$DIRS $ROOT/tests/libxslt/tests/$d"
done
# Run from the output directory, xsl:document writes relative to the current directory
# of each test (the runtest changes into the test directory).
cd "$OUT" || exit 1
"$OUT/libxslt_runtest" -k "$ROOT/tests/libxslt_suite/known_failures.txt" $DIRS > "$OUT/runtest.txt" 2>&1
RC=$?
tail -1 "$OUT/runtest.txt"
if [ $RC != 0 ]
then
    grep -E "^(FAIL|KNOWN FAILURE PASSES)" "$OUT/runtest.txt"
    echo "ERR: libxslt test suite, see $OUT/runtest.txt"
    exit 1
fi
echo "OK: libxslt test suite"
