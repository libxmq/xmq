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
# And the libxslt fork (3rdparty/fetch_and_build.sh libxslt) for the xslt harnesses.
LIBXSLT="$ROOT/3rdparty/libxslt"
if [ ! -f "$LIBXSLT/libxslt/.libs/libxslt.a" ]
then
    echo "Please run: 3rdparty/fetch_and_build.sh libxslt"
    exit 1
fi
CFLAGS="-g -O1 -fsanitize=address,undefined -fno-omit-frame-pointer -I$ROOT/src/main/c -I$LIBXML2/include -I$LIBXSLT"
LIBS="$LIBXML2/.libs/libxml2.a -lm"
PARTS="$ROOT/src/main/c/parts/always.c $ROOT/src/main/c/parts/membuffer.c $ROOT/src/main/c/parts/hashmap.c $ROOT/src/main/c/parts/xml_dom.c $ROOT/src/main/c/parts/xml_save.c $ROOT/src/main/c/parts/xml_parser.c $ROOT/src/main/c/parts/xml_html.c $ROOT/src/main/c/parts/xml_html_parser.c $ROOT/src/main/c/parts/xml_html_save.c $ROOT/src/main/c/parts/xml_chars.c $ROOT/src/main/c/parts/xml_xpath.c $ROOT/src/main/c/parts/xml_uri.c $ROOT/src/main/c/parts/xml_unicode.c $ROOT/src/main/c/parts/xml_regexp.c $ROOT/src/main/c/parts/xml_pattern.c $ROOT/src/main/c/parts/xml_xsd10_types.c $ROOT/src/main/c/parts/xml_xsd10_schemas.c $ROOT/src/main/c/parts/xml_xsd10_parse.c $ROOT/src/main/c/parts/xml_xsd10_build.c $ROOT/src/main/c/parts/xml_xsd10_valid.c $ROOT/src/main/c/parts/xml_libxslt1_xslt.c $ROOT/src/main/c/parts/xml_libxslt1_transform.c"

$CC $CFLAGS -o "$OUT/xml_save_diff" "$ROOT/tests/xml_diff/xml_save_diff.c" $PARTS $LIBS || exit 1
$CC $CFLAGS -o "$OUT/xml_parse_diff" "$ROOT/tests/xml_diff/xml_parse_diff.c" $PARTS $LIBS || exit 1
$CC $CFLAGS -o "$OUT/xml_html_diff" "$ROOT/tests/xml_diff/xml_html_diff.c" $PARTS $LIBS || exit 1
$CC $CFLAGS -o "$OUT/xml_xpath_diff" "$ROOT/tests/xml_diff/xml_xpath_diff.c" $PARTS $LIBS || exit 1
$CC $CFLAGS -o "$OUT/xml_entity_diff" "$ROOT/tests/xml_diff/xml_entity_diff.c" $PARTS $LIBS || exit 1
$CC $CFLAGS -o "$OUT/xml_libxslt1_pattern_diff" "$ROOT/tests/xml_diff/xml_libxslt1_pattern_diff.c" $PARTS "$LIBXSLT/libxslt/.libs/libxslt.a" $LIBS || exit 1
$CC $CFLAGS -o "$OUT/xml_libxslt1_compile_diff" "$ROOT/tests/xml_diff/xml_libxslt1_compile_diff.c" $PARTS "$LIBXSLT/libxslt/.libs/libxslt.a" $LIBS || exit 1
$CC $CFLAGS -o "$OUT/xml_libxslt1_transform_diff" "$ROOT/tests/xml_diff/xml_libxslt1_transform_diff.c" $PARTS "$LIBXSLT/libxslt/.libs/libxslt.a" $LIBS || exit 1
$CC $CFLAGS -o "$OUT/xml_regexp_diff" "$ROOT/tests/xml_diff/xml_regexp_diff.c" $PARTS $LIBS || exit 1
$CC $CFLAGS -o "$OUT/xml_pattern_diff" "$ROOT/tests/xml_diff/xml_pattern_diff.c" $PARTS $LIBS || exit 1
$CC $CFLAGS -o "$OUT/xml_xsd10_types_diff" "$ROOT/tests/xml_diff/xml_xsd10_types_diff.c" $PARTS $LIBS || exit 1
# Includes libxml2's xmlschemas.c (to reach its static functions), which needs the private headers.
$CC $CFLAGS -I"$LIBXML2" -o "$OUT/xml_xsd10_schemas_diff" "$ROOT/tests/xml_diff/xml_xsd10_schemas_diff.c" $PARTS $LIBS || exit 1
$CC $CFLAGS -o "$OUT/xml_xsd10_valid_diff" "$ROOT/tests/xml_diff/xml_xsd10_valid_diff.c" $PARTS $LIBS || exit 1
$CC $CFLAGS -I"$LIBXML2" -o "$OUT/xml_xsd10_parse_diff" "$ROOT/tests/xml_diff/xml_xsd10_parse_diff.c" $PARTS $LIBS || exit 1
$CC $CFLAGS -I"$LIBXML2" -o "$OUT/xml_xsd10_build_diff" "$ROOT/tests/xml_diff/xml_xsd10_build_diff.c" $PARTS $LIBS || exit 1
$CC $CFLAGS -o "$OUT/xml_libxslt1_runtest" "$ROOT/tests/libxslt_suite/runtest.c" $PARTS -lm || exit 1

FILES="$OUT/files.txt"
# The entity expansion tests are only for xml_entity_diff, many are deliberately broken.
find "$ROOT" \( -path "$ROOT/build" -o -path "$ROOT/.git" -o -path "$ROOT/3rdparty" -o -path "$ROOT/tests/libxml2" -o -path "$ROOT/tests/libxslt" -o -path "$ROOT/tests/xml_diff/entities" \) -prune -o -name "*.xml" -print > "$FILES"
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
find "$ROOT" \( -path "$ROOT/build" -o -path "$ROOT/.git" -o -path "$ROOT/3rdparty" -o -path "$ROOT/tests/libxml2" -o -path "$ROOT/tests/libxslt" \) -prune -o \( -name "*.html" -o -name "*.htm" \) -print > "$HTML_FILES"
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
ASAN_OPTIONS=detect_leaks=1 "$OUT/xml_libxslt1_pattern_diff" $(cat "$FILES")
RC6=$?
XSL_FILES="$OUT/xsl_files.txt"
find "$ROOT/3rdparty/libxslt/tests" -name "*.xsl" | sort > "$XSL_FILES"
find "$ROOT" \( -path "$ROOT/build" -o -path "$ROOT/.git" -o -path "$ROOT/3rdparty" -o -path "$ROOT/tests/libxml2" -o -path "$ROOT/tests/libxslt" \) -prune -o -name "*.xsl" -print >> "$XSL_FILES"
# MUTANTS=count:seed for the stylesheet mutations, default 5 per stylesheet.
ASAN_OPTIONS=detect_leaks=1 "$OUT/xml_libxslt1_compile_diff" -m "${MUTANTS:-5:1}" $(cat "$XSL_FILES")
RC7=$?
# The libxslt test suite: every stylesheet with an input document (name.xml), like libxslt's runtest.
XSLT_TESTS="$OUT/xslt_tests.txt"
# Our own tests (output methods, encodings, xsl:document) first.
ls "$ROOT"/tests/xml_diff/xslt/*.xsl > "$XSLT_TESTS"
for d in REC2 REC general encoding documents numbers keys namespaces extensions reports exslt/common exslt/crypto exslt/date exslt/dynamic exslt/functions exslt/math exslt/saxon exslt/sets exslt/strings plugins
do
    [ -d "$ROOT/3rdparty/libxslt/tests/$d" ] && ls "$ROOT/3rdparty/libxslt/tests/$d"/*.xsl >> "$XSLT_TESTS"
done
# xsl:document writes files relative to the current directory (lx/ and nx/ below it).
mkdir -p "$OUT/xsltrun"
(cd "$OUT/xsltrun" && ASAN_OPTIONS=detect_leaks=1 "$OUT/xml_libxslt1_transform_diff" -m "${MUTANTS:-5:1}" $(cat "$XSLT_TESTS"))
RC8=$?
# The libxslt test suite against its expected .out and .err files (native only, like libxslt's runtest).
RUNTEST_DIRS=""
for d in REC2 REC general encoding documents numbers keys namespaces extensions reports exslt/common exslt/crypto exslt/date exslt/dynamic exslt/functions exslt/math exslt/saxon exslt/sets exslt/strings plugins
do
    [ -d "$ROOT/3rdparty/libxslt/tests/$d" ] && RUNTEST_DIRS="$RUNTEST_DIRS $ROOT/3rdparty/libxslt/tests/$d"
done
ASAN_OPTIONS=detect_leaks=1 "$OUT/xml_libxslt1_runtest" -k "$ROOT/tests/libxslt_suite/known_failures.txt" $RUNTEST_DIRS
RC9=$?
# XSD: the regular expressions and automata, the streaming patterns of identity
# constraints and the built in types.
ASAN_OPTIONS=detect_leaks=1 "$OUT/xml_regexp_diff" -n "${REGEXPS:-20000:1}" -a "${AUTOMATA:-1000:1}" "$LIBXML2"
RC10=$?
ASAN_OPTIONS=detect_leaks=1 "$OUT/xml_pattern_diff" -g "${PATTERNS:-40:1}" -p "$ROOT/3rdparty/libxml2/test/pattern" -r "$ROOT/3rdparty/libxml2/result/pattern" "$ROOT"/3rdparty/libxml2/test/schemas/*.xsd $(cat "$FILES")
RC11=$?
ASAN_OPTIONS=detect_leaks=1 "$OUT/xml_xsd10_types_diff" -r "${XSD_RANDOM:-400:1}" "$ROOT"/3rdparty/libxml2/test/schemas/*
RC12=$?
# XSD validation of the libxml2 schema test suite (and mutated instances).
ASAN_OPTIONS=detect_leaks=1 "$OUT/xml_xsd10_valid_diff" "$ROOT/3rdparty/libxml2"
RC14=$?
ASAN_OPTIONS=detect_leaks=1 "$OUT/xml_xsd10_parse_diff" -r "${XSD_PARSE_RANDOM:-30:1}" "$ROOT"/3rdparty/libxml2/test/schemas/*.xsd
RC15=$?
ASAN_OPTIONS=detect_leaks=1:fast_unwind_on_malloc=0 "$OUT/xml_xsd10_build_diff" -r "${XSD_BUILD_RANDOM:-2000:1}" -p -m "${XSD_BUILD_MUTATIONS:-2000:1}" "$ROOT"/3rdparty/libxml2/test/schemas/*.xsd
RC16=$?
# The leak suppressions of libxml2's own leaks need full stacks.
ASAN_OPTIONS=detect_leaks=1:fast_unwind_on_malloc=0 "$OUT/xml_xsd10_schemas_diff" -r "${XSD_SCHEMAS_RANDOM:-2000:1}" "$ROOT"/3rdparty/libxml2/test/schemas/*.xsd
RC13=$?
[ $RC1 = 0 ] && [ $RC2 = 0 ] && [ $RC3 = 0 ] && [ $RC4 = 0 ] && [ $RC5 = 0 ] && [ $RC6 = 0 ] && [ $RC7 = 0 ] && [ $RC8 = 0 ] && [ $RC9 = 0 ] && [ $RC10 = 0 ] && [ $RC11 = 0 ] && [ $RC12 = 0 ] && [ $RC13 = 0 ] && [ $RC14 = 0 ] && [ $RC15 = 0 ] && [ $RC16 = 0 ]
