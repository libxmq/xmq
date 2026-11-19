#!/bin/bash
# libxmq - Copyright 2026 Fredrik Öhrström (spdx: MIT)
#
# Test xmq check-library and xmq refresh-library against a library served from a local
# directory (XMQ_LIBRARY_URL=file://...) with an index made by scripts/build_library_index.sh.
# HOME is a fresh directory, so the downloads go to OUTPUT/home/.local/share/xmq/library.
# Usage: tests/test_library_refresh.sh <xmq binary> <output dir>

# An absolute path, unless the program has a prefix (eg valgrind).
PROG=$1
if [ -x "$1" ]; then PROG=$(cd "$(dirname "$1")" && pwd)/$(basename "$1"); fi
OUTPUT=$2/library_refresh

fail()
{
    echo "ERR: library refresh $1"
    exit 1
}

if ! command -v curl > /dev/null
then
    echo "SKIP: tests/test_library_refresh.sh (needs curl)"
    exit 0
fi

rm -rf "$OUTPUT"
mkdir -p "$OUTPUT/server" "$OUTPUT/home"
cp -rp library/. "$OUTPUT/server/"
scripts/build_library_index.sh "$OUTPUT/server" > /dev/null || fail "no index"

export HOME="$OUTPUT/home"
export XMQ_LIBRARY_URL="file://$OUTPUT/server/"
unset XMQ_LIBRARY
CACHE="$HOME/.local/share/xmq/library"
TOTAL=$(grep -c 'file(path' "$OUTPUT/server/index.xmq")

# Nothing is downloaded yet, every file is new.
"$PROG" check-library > "$OUTPUT/check1.txt" || fail "check-library failed"
[ "$(grep -c '^new ' "$OUTPUT/check1.txt")" = "$TOTAL" ] || fail "not all $TOTAL files are new"
[ -d "$CACHE/data" ] && fail "check-library downloaded files"

"$PROG" refresh-library > "$OUTPUT/refresh1.txt" || fail "refresh-library failed"
cmp -s "$OUTPUT/server/data/csv.ixml" "$CACHE/data/csv.ixml" || fail "csv.ixml was not refreshed"
"$PROG" check-library > "$OUTPUT/check2.txt"
grep -q "The library is up to date ($TOTAL files)" "$OUTPUT/check2.txt" || fail "not up to date after refresh-library"

# A file changed on the server, more recently than the local one.
echo '{changed}' >> "$OUTPUT/server/data/csv.ixml"
touch -d '+1 hour' "$OUTPUT/server/data/csv.ixml"
scripts/build_library_index.sh "$OUTPUT/server" > /dev/null
"$PROG" check-library > "$OUTPUT/check3.txt"
grep -q '^updated data/csv.ixml' "$OUTPUT/check3.txt" || fail "the changed csv.ixml is not listed"
"$PROG" refresh-library > /dev/null || fail "refresh-library of csv.ixml failed"
cmp -s "$OUTPUT/server/data/csv.ixml" "$CACHE/data/csv.ixml" || fail "the changed csv.ixml was not refreshed"

# A file changed locally, more recently than the one on the server, is kept.
echo '{mine}' >> "$CACHE/data/tsv.ixml"
touch -d '+2 hours' "$CACHE/data/tsv.ixml"
"$PROG" check-library > "$OUTPUT/check4.txt"
grep -q '^newer   data/tsv.ixml' "$OUTPUT/check4.txt" || fail "the local tsv.ixml is not listed as newer"
"$PROG" refresh-library > /dev/null
grep -q '{mine}' "$CACHE/data/tsv.ixml" || fail "refresh-library replaced the newer local tsv.ixml"

# A download with another sha256 than the index is not used.
rm "$CACHE/core/xmq.ixml"
sed -i "/path   = core\/xmq.ixml/{n;n;s/sha256 = .*/sha256 = $(printf '0%.0s' $(seq 64))/}" "$OUTPUT/server/index.xmq"
"$PROG" refresh-library > /dev/null 2> "$OUTPUT/badsha.err" && fail "a bad sha256 exits 0"
grep -q 'is not the one in the library index' "$OUTPUT/badsha.err" || fail "no message for a bad sha256"
[ -f "$CACHE/core/xmq.ixml" ] && fail "a download with a bad sha256 was kept"

# The refreshed files are used for ixml: names before the installed library.
printf 'a,b\n1,2\n' > "$OUTPUT/in.csv"
"$PROG" --verbose ixml:data/csv "$OUTPUT/in.csv" to-xmq 2>&1 > /dev/null | grep -q "$CACHE/data/csv.ixml" || fail "ixml:data/csv does not use the refreshed file"

echo "OK: library refresh"
