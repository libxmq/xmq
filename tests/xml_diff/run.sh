#!/bin/sh
# Differential tests of the native xml parts against libxml2, used while libxmq moves away from libxml2.
# Usage: tests/xml_diff/run.sh [output_dir]
# Builds the diff tools with address/undefined sanitizers and runs them over
# every xml file in the repository and in the libxml2 test suite (if fetched).

ROOT=$(cd "$(dirname "$0")/../.." && pwd)
OUT=${1:-$ROOT/build/xml_diff}
mkdir -p "$OUT"
# The tools run from the output dir, make the path absolute.
OUT=$(cd "$OUT" && pwd)

CC=${CC:-gcc}
# Compare against the libxml2 fork that libxmq releases are built with (3rdparty/fetch_and_build.sh libxml2).
LIBXML2="$ROOT/3rdparty/libxml2"
if [ ! -f "$LIBXML2/.libs/libxml2.a" ]
then
    echo "Please run: 3rdparty/fetch_and_build.sh libxml2"
    exit 1
fi
CFLAGS="-g -O1 -fsanitize=address,undefined -fno-omit-frame-pointer -I$ROOT/src/main/c -I$LIBXML2/include"
LIBS="$LIBXML2/.libs/libxml2.a -lm"
PARTS="$ROOT/src/main/c/parts/always.c $ROOT/src/main/c/parts/membuffer.c $ROOT/src/main/c/parts/hashmap.c $ROOT/src/main/c/parts/xml_dom.c $ROOT/src/main/c/parts/xml_save.c $ROOT/src/main/c/parts/xml_parser.c $ROOT/src/main/c/parts/xml_html.c $ROOT/src/main/c/parts/xml_html_parser.c $ROOT/src/main/c/parts/xml_html_save.c $ROOT/src/main/c/parts/xml_chars.c $ROOT/src/main/c/parts/xml_xpath.c"

$CC $CFLAGS -o "$OUT/xml_save_diff" "$ROOT/tests/xml_diff/xml_save_diff.c" $PARTS $LIBS || exit 1
$CC $CFLAGS -o "$OUT/xml_parse_diff" "$ROOT/tests/xml_diff/xml_parse_diff.c" $PARTS $LIBS || exit 1
$CC $CFLAGS -o "$OUT/xml_html_diff" "$ROOT/tests/xml_diff/xml_html_diff.c" $PARTS $LIBS || exit 1
$CC $CFLAGS -o "$OUT/xml_xpath_diff" "$ROOT/tests/xml_diff/xml_xpath_diff.c" $PARTS $LIBS || exit 1
$CC $CFLAGS -o "$OUT/xml_entity_diff" "$ROOT/tests/xml_diff/xml_entity_diff.c" $PARTS $LIBS || exit 1

FILES="$OUT/files.txt"
# The entity expansion tests are only for xml_entity_diff, many are deliberately broken.
find "$ROOT" \( -path "$ROOT/build" -o -path "$ROOT/.git" -o -path "$ROOT/3rdparty" -o -path "$ROOT/tests/xml_diff/entities" \) -prune -o -name "*.xml" -print > "$FILES"
if [ -d "$ROOT/3rdparty/libxml2/test" ]
then
    find "$ROOT/3rdparty/libxml2/test" -name "*.xml" >> "$FILES"
fi

# Run from the output dir so relative external entities resolve nowhere interesting.
cd "$OUT" || exit 1
# One invocation each (xargs -x would split long lists, so pass the list via a file).
ASAN_OPTIONS=detect_leaks=1 "$OUT/xml_save_diff" $(cat "$FILES")
RC1=$?
ASAN_OPTIONS=detect_leaks=1 "$OUT/xml_parse_diff" -x "$ROOT/tests/xml_diff/expected_parse_differences.txt" $(cat "$FILES")
RC2=$?
ASAN_OPTIONS=detect_leaks=1 "$OUT/xml_entity_diff" -x "$ROOT/tests/xml_diff/expected_entity_differences.txt" $(cat "$FILES") "$ROOT"/tests/xml_diff/entities/*.xml
RC5=$?
HTML_FILES="$OUT/html_files.txt"
find "$ROOT" \( -path "$ROOT/build" -o -path "$ROOT/.git" -o -path "$ROOT/3rdparty" \) -prune -o \( -name "*.html" -o -name "*.htm" \) -print > "$HTML_FILES"
if [ -d "$ROOT/3rdparty/libxml2/test/HTML" ]
then
    find "$ROOT/3rdparty/libxml2/test/HTML" -type f >> "$HTML_FILES"
fi
TOKENIZER_TESTS=""
for t in "$ROOT"/3rdparty/libxml2/test/html-tokenizer/*.test
do
    [ -f "$t" ] && TOKENIZER_TESTS="$TOKENIZER_TESTS -t $t"
done
# FUZZ=count:seed for the html differential fuzzer, default 2000 documents.
ASAN_OPTIONS=detect_leaks=1 "$OUT/xml_html_diff" -x "$ROOT/tests/xml_diff/expected_html_differences.txt" $TOKENIZER_TESTS -f "${FUZZ:-2000:1}" $(cat "$HTML_FILES")
RC3=$?
ASAN_OPTIONS=detect_leaks=1 "$OUT/xml_xpath_diff" $(cat "$FILES") "$ROOT"/tests/xml_diff/entities/*.xml
RC4=$?
[ $RC1 = 0 ] && [ $RC2 = 0 ] && [ $RC3 = 0 ] && [ $RC4 = 0 ] && [ $RC5 = 0 ]
