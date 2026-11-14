#!/bin/sh
# Differential tests of the native xml parts against libxml2 (see XMLPLAN.md).
# Usage: tests/xml_diff/run.sh [output_dir]
# Builds the diff tools with address/undefined sanitizers and runs them over
# every xml file in the repository and in the libxml2 test suite (if fetched).

ROOT=$(cd "$(dirname "$0")/../.." && pwd)
OUT=${1:-$ROOT/build/xml_diff}
mkdir -p "$OUT"

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
PARTS="$ROOT/src/main/c/parts/always.c $ROOT/src/main/c/parts/membuffer.c $ROOT/src/main/c/parts/xml_dom.c $ROOT/src/main/c/parts/xml_save.c"

$CC $CFLAGS -o "$OUT/xml_save_diff" "$ROOT/tests/xml_diff/xml_save_diff.c" $PARTS $LIBS || exit 1

FILES="$OUT/files.txt"
find "$ROOT" \( -path "$ROOT/build" -o -path "$ROOT/.git" -o -path "$ROOT/3rdparty" \) -prune -o -name "*.xml" -print > "$FILES"
if [ -d "$ROOT/3rdparty/libxml2/test" ]
then
    find "$ROOT/3rdparty/libxml2/test" -name "*.xml" >> "$FILES"
fi

# Run from the output dir so relative external entities resolve nowhere interesting.
cd "$OUT" || exit 1
ASAN_OPTIONS=detect_leaks=1 xargs "$OUT/xml_save_diff" < "$FILES"
