#!/bin/bash
# libxmq - Copyright 2026 Fredrik Öhrström (spdx: MIT)
#
# Build the dist style xmq.c (an amalgamation of the parts) with each of the defines that
# exclude a part (XMQ_NO_XMQ_PRINTING, XMQ_NO_JSON, XMQ_NO_YAML, XMQ_NO_LIBXSLT, XMQ_NO_XSD, XMQ_NO_IXML, XMQ_NO_HTML),
# none and all of them, as C and as C++, and run tests/excluded/test_excluded.c against it.
# A build must compile without warnings and link, and the excluded api must return errors.
# Usage: tests/test_excluded.sh <build output dir>

ROOT=$(cd "$(dirname "$0")/.." && pwd)
if [ -z "$1" ]
then
    echo "Usage: tests/test_excluded.sh <build output dir>"
    exit 1
fi
OUT="$1"
mkdir -p "$OUT" || exit 1
OUT=$(cd "$OUT" && pwd)

CC=${CC:-gcc}
CXX=${CXX:-g++}
"$ROOT/scripts/build_xmq_from_parts.sh" "$OUT" "$ROOT/src/main/c/xmq.c" "$ROOT/dist/VERSION" > /dev/null || exit 1
cp "$OUT/xmq-in-progress" "$OUT/xmq.c"
cp "$ROOT/src/main/c/xmq.h" "$OUT/xmq.h"

ALL="-DXMQ_NO_XMQ_PRINTING -DXMQ_NO_JSON -DXMQ_NO_YAML -DXMQ_NO_LIBXSLT -DXMQ_NO_XSD -DXMQ_NO_IXML -DXMQ_NO_HTML"
# one NAME LANG DEFINES...: build and run one variant, write $OUT/excl_NAME_LANG.result.
one()
{
    name=$1
    lang=$2
    shift 2
    r="$OUT/excl_${name}_$lang.result"
    : > "$r"
    {
        if [ "$lang" = "c" ]
        then
            $CC -O0 -Wall -Werror -Wno-unused-function -I"$OUT" "$@" -o "$OUT/excl_${name}_$lang" \
                "$ROOT/tests/excluded/test_excluded.c" "$OUT/xmq.c" -lm > "$OUT/excl_${name}_$lang.log" 2>&1
        else
            $CXX -O0 -Wall -Werror -Wno-unused-function -I"$OUT" "$@" -o "$OUT/excl_${name}_$lang" \
                -x c++ "$ROOT/tests/excluded/test_excluded.c" -x c++ "$OUT/xmq.c" -lm > "$OUT/excl_${name}_$lang.log" 2>&1
        fi
        if [ "$?" != "0" ]
        then
            echo "ERR: excluded build $name ($lang) does not build, see $OUT/excl_${name}_$lang.log" > "$r"
            return
        fi
        if ! "$OUT/excl_${name}_$lang" > "$OUT/excl_${name}_$lang.out" 2>&1
        then
            { echo "ERR: excluded build $name ($lang) fails:"; cat "$OUT/excl_${name}_$lang.out"; } > "$r"
        fi
    }
}

# The variants are built in parallel (JOBS, default the number of cpus).
JOBS=${JOBS:-$(nproc 2> /dev/null || echo 4)}
VARIANTS="none:
no_xmq_printing:-DXMQ_NO_XMQ_PRINTING
no_json:-DXMQ_NO_JSON
no_yaml:-DXMQ_NO_YAML
no_libxslt:-DXMQ_NO_LIBXSLT
no_xsd:-DXMQ_NO_XSD
no_ixml:-DXMQ_NO_IXML
no_html:-DXMQ_NO_HTML
all:$ALL"
while IFS=: read -r name defines
do
    for lang in c cc
    do
        while [ "$(jobs -r | wc -l)" -ge "$JOBS" ]; do wait -n; done
        one "$name" "$lang" $defines &
    done
done <<< "$VARIANTS"
wait

failed=0
for r in "$OUT"/excl_*.result
do
    if [ -s "$r" ]
    then
        cat "$r"
        failed=1
    fi
done
if [ "$failed" != "0" ]
then
    exit 1
fi
echo "OK: excluded builds (none, each of XMQ_NO_XMQ_PRINTING/JSON/YAML/LIBXSLT/XSD/IXML/HTML and all, C and C++)"
