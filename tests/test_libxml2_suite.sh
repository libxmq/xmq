#!/bin/sh
# Run the native xml parts over the libxml2 test suite (tests/libxml2) and compare
# with libxml2's expected results, see tests/libxml2_suite/runtest.c.
# Usage: tests/test_libxml2_suite.sh <build output dir> [-v] [category...]
# Builds the test program (native code only, libxml2 is not needed) with the address
# and undefined behaviour sanitizers into the output dir and runs it from the repository root.

ROOT=$(cd "$(dirname "$0")/.." && pwd)
if [ -z "$1" ]
then
    echo "Usage: tests/test_libxml2_suite.sh <build output dir> [-v] [category...]"
    exit 1
fi
OUT="$1"
shift
mkdir -p "$OUT" || exit 1
OUT=$(cd "$OUT" && pwd)

CC=${CC:-gcc}
CFLAGS="-g -O1 -fsanitize=address,undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer -I$ROOT/src/main/c"
P="$ROOT/src/main/c/parts"
PARTS="$P/always.c $P/membuffer.c $P/hashmap.c $P/xml_dom.c $P/xml_save.c $P/xml_parser.c $P/xml_html.c $P/xml_html_parser.c $P/xml_html_save.c $P/xml_chars.c $P/xml_xpath.c $P/xml_uri.c $P/xml_unicode.c $P/xml_regexp.c $P/xml_pattern.c $P/xml_xsd10_types.c $P/xml_xsd10_schemas.c $P/xml_xsd10_parse.c $P/xml_xsd10_build.c $P/xml_xsd10_valid.c"

$CC $CFLAGS -o "$OUT/libxml2_runtest" "$ROOT/tests/libxml2_suite/runtest.c" $PARTS -lm || exit 1

cd "$ROOT" || exit 1
# The runner is built with ASan, which must be loaded first: drop preloads such as
# libeatmydata (eatmydata make test on github).
env -u LD_PRELOAD ASAN_OPTIONS=detect_leaks=1 "$OUT/libxml2_runtest" -k "$ROOT/tests/libxml2_suite/known_failures.txt" -d "$ROOT/tests/libxml2" "$@"
