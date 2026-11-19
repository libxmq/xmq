#!/bin/sh
# libxmq - Copyright 2026 Fredrik Öhrström (spdx: MIT)
#
# Write the library index, index.xmq, for xmq check-library and xmq refresh-library:
# every grammar and stylesheet of the library (the files that install.sh installs)
# with its size, sha256 and modification time (UTC):
#
#   library {
#       file(path   = data/csv.ixml
#            size   = 293
#            sha256 = 9a2b...
#            mtime  = 2026-10-20T10:08:55Z)
#       ...
#   }
#
# Publish index.xmq together with the library files on https://libxmq.org/library/ and
# keep the modification times of the files (eg rsync -t), xmq compares them with the
# time of the local files to decide which is newer.
#
# Usage: scripts/build_library_index.sh [library_dir] [output_dir]
# The defaults are library and library (the index is written to library/index.xmq).

LIBRARY=${1:-library}
OUTDIR=${2:-$LIBRARY}

if [ ! -d "$LIBRARY" ]
then
    echo "No library directory $LIBRARY"
    exit 1
fi

sha256()
{
    if command -v sha256sum > /dev/null; then sha256sum "$1" | cut -d' ' -f1
    else shasum -a 256 "$1" | cut -d' ' -f1
    fi
}

OUT="$OUTDIR/index.xmq"
TMP="$OUT.tmp"
{
    echo "// The library index, made by scripts/build_library_index.sh, see xmq help check-library."
    echo "library {"
    (cd "$LIBRARY" && find . -name 'test-*' -prune -o -name 'c-tests' -prune -o -type f \
         \( -name '*.ixml' -o -name '*.xslq' -o -name '*.xml' -o -name '*.xmq' \) ! -name 'index.xmq' -print) |
    sed 's|^\./||' | LC_ALL=C sort |
    while read -r f
    do
        echo "    file(path   = $f"
        echo "         size   = $(wc -c < "$LIBRARY/$f" | tr -d ' ')"
        echo "         sha256 = $(sha256 "$LIBRARY/$f")"
        echo "         mtime  = $(date -u -r "$LIBRARY/$f" +%Y-%m-%dT%H:%M:%SZ))"
    done
    echo "}"
} > "$TMP" && mv "$TMP" "$OUT"

echo "Wrote $OUT"
