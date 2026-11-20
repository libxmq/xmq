#!/bin/sh
# libxmq - Copyright 2026 Fredrik Öhrström (spdx: MIT)
#
# Installed as build/xmq by make release, debug and asan. It runs the most recently
# built xmq (build/<host>/release, debug or asan) with the library of this source
# tree, so that eg build/xmq ixml:data/csv foo.csv uses library/data/csv.ixml of the
# source and not a downloaded or installed one.
#
#   XMQ_LIBRARY      <source>/library, used before ~/.local/share/xmq/library
#                    and the installed library.
#   XMQ_LIBRARY_URL  file://<source>/library/ for list-library, describe-library
#                    and refresh-library (unless already set).

BUILD=$(cd "$(dirname "$0")" && pwd)
ROOT=$(dirname "$BUILD")

PROG=""
for x in "$BUILD"/*/release/xmq "$BUILD"/*/debug/xmq "$BUILD"/*/asan/xmq
do
    [ -x "$x" ] || continue
    if [ -z "$PROG" ] || [ "$x" -nt "$PROG" ]; then PROG=$x; fi
done

if [ -z "$PROG" ]
then
    echo "build/xmq: no xmq has been built, run make" >&2
    exit 1
fi

XMQ_LIBRARY="$ROOT/library"
export XMQ_LIBRARY
if [ -z "$XMQ_LIBRARY_URL" ]
then
    XMQ_LIBRARY_URL="file://$ROOT/library/"
    export XMQ_LIBRARY_URL
fi

exec "$PROG" "$@"
