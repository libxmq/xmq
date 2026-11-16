#!/bin/bash
# libxmq - Copyright 2026 Fredrik Öhrström (spdx: MIT)
#
# Build xmq, libxmq.so and libxmq.a with parts excluded (the XMQ_NO_... defines in xmq.h)
# into <output dir>/release, next to a copy of spec.inc, so that tests/test.sh can run on it.
# tests/test.sh asks xmq --features and skips the tests that need an excluded part.
# Usage: tests/build_without.sh <output dir> <name>...
# where a name is XMQ_PRINTING, JSON, YAML, LIBXSLT, XSD, IXML or HTML, eg:
#   tests/build_without.sh build/without_ixml IXML
#   tests/test.sh build/without_ixml/release build/without_ixml/release/test_output
# (make test_without WITHOUT="IXML" does both.) JOBS=n parallel compiles.

ROOT=$(cd "$(dirname "$0")/.." && pwd)
if [ -z "$2" ]
then
    echo "Usage: tests/build_without.sh <output dir> <XMQ_PRINTING|JSON|YAML|LIBXSLT|XSD|IXML|HTML>..."
    exit 1
fi
OUT="$1"
shift
mkdir -p "$OUT/release/obj" || exit 1
OUT=$(cd "$OUT" && pwd)
REL="$OUT/release"

DEFINES=""
for n in "$@"
do
    case "$n" in
    XMQ_PRINTING|JSON|YAML|LIBXSLT|XSD|IXML|HTML) DEFINES="$DEFINES -DXMQ_NO_$n" ;;
    *) echo "Unknown part to exclude: $n (XMQ_PRINTING JSON YAML LIBXSLT XSD IXML HTML)"; exit 1 ;;
    esac
done

# spec.inc of the configured build (tests/test.sh reads it).
SPEC=$(ls "$ROOT"/build/*/spec.inc 2> /dev/null | grep -v /default/ | head -n 1)
if [ -z "$SPEC" ]
then
    echo "Run configure first, no build/*/spec.inc found."
    exit 1
fi
cp "$SPEC" "$OUT/spec.inc"

CC=${CC:-gcc}
CFLAGS="-O2 -g -fPIC -Wall -Wno-unused-function -DPLATFORM_POSIX $DEFINES -DVERSION=\"$(cat "$ROOT/dist/VERSION")-without\""
JOBS=${JOBS:-$(nproc 2> /dev/null || echo 4)}
export CC CFLAGS ROOT REL

compile()
{
    o="$REL/obj/$(basename "$1" .c).o"
    if ! $CC $CFLAGS -I"$ROOT/src/main/c" -I"$ROOT/src/main/c/parts" -c -o "$o" "$1" 2> "$o.log"
    then
        echo "ERR: compiling $1 failed, see $o.log"
        return 1
    fi
}
export -f compile

SOURCES="$ROOT/src/main/c/xmq.c $(ls "$ROOT"/src/main/c/parts/*.c | grep -v -e testinternals -e winapi)"
if ! printf '%s\n' $SOURCES | xargs -P "$JOBS" -I{} bash -c 'compile {}'
then
    exit 1
fi

OBJS=$(ls "$REL"/obj/*.o)
$CC -shared -o "$REL/libxmq.so" $OBJS -lm || exit 1
rm -f "$REL/libxmq.a"
ar rcs "$REL/libxmq.a" $OBJS || exit 1
$CC $CFLAGS -I"$ROOT/src/main/c" -o "$REL/xmq" "$ROOT/src/main/c/xmq-cli.c" $OBJS -lm -lpthread || exit 1

echo "Built $REL/xmq without:$DEFINES (features: $("$REL/xmq" --features))"
