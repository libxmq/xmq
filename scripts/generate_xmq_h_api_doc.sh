#!/bin/sh
# libxmq - Copyright 2026 Fredrik Öhrström (spdx: MIT)
#
# Generate the api documentation page for xmq.h.
#
# An empty C file that includes xmq.h is preprocessed with gcc -E -C -dD, which keeps
# the comments and the macro definitions. Only the lines from xmq.h are kept (using the
# line markers, so the system headers are dropped). This is parsed with the ixml grammar
# library/lang/c.ixml, which keeps comments and defines as elements, and transformed with
# web/resources/genapi.xslq into a page listing the functions, types, enums and defines
# that start with xmq or XMQ, with their doxygen doc comments.
#
# Usage: scripts/generate_xmq_h_api_doc.sh [output.html] [xmq binary]
# The defaults are build/web/api.html and build/default/release/xmq.

OUT=${1:-build/web/api.html}
XMQ=${2:-build/default/release/xmq}
CC=${CC:-gcc}

if [ ! -x "$XMQ" ]
then
    echo "Cannot find the xmq binary $XMQ, run make first."
    exit 1
fi

TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

echo '#include "xmq.h"' > "$TMP/api.c"

# Keep the comments (-C) and the defines (-dD).
$CC -E -C -dD -Isrc/main/c "$TMP/api.c" > "$TMP/api.i" || exit 1

# Keep the line markers and the lines from xmq.h.
awk '/^# [0-9]+ "/ { keep = ($3 ~ /xmq\.h"$/); print; next } keep' "$TMP/api.i" > "$TMP/xmq_h.i"

mkdir -p "$(dirname "$OUT")"
TODAY=$(date +'%Y-%m-%d %H:%M')
"$XMQ" library/lang/c.ixml "$TMP/xmq_h.i" \
       transform --stringparam=date="$TODAY" web/resources/genapi.xslq \
       to-html > "$OUT.tmp" || { rm -f "$OUT.tmp"; exit 1; }
mv "$OUT.tmp" "$OUT"
echo "Wrote $OUT"
