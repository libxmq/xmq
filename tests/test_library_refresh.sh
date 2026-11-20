#!/bin/bash
# libxmq - Copyright 2026 Fredrik Öhrström (spdx: MIT)
#
# Test xmq check-library, refresh-library, list-library and describe-library against a library
# served from a local directory (XMQ_LIBRARY_URL=file://...) with the versioned copies and the
# index made by scripts/build_library_index.py.
# HOME is a fresh directory, so the downloads go to OUTPUT/home/.local/share/xmq/library.
# Usage: tests/test_library_refresh.sh <xmq binary> <output dir>

# An absolute path, unless the program has a prefix (eg valgrind).
PROG=$1
if [ -x "$1" ]; then PROG=$(cd "$(dirname "$1")" && pwd)/$(basename "$1"); fi
mkdir -p "$2"
OUTPUT=$(cd "$2" && pwd)/library_refresh

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
scripts/build_library_index.py "$OUTPUT/server" > /dev/null 2>&1 || fail "no index"

export HOME="$OUTPUT/home"
export XMQ_LIBRARY_URL="file://$OUTPUT/server/"
unset XMQ_LIBRARY
# Not the library installed by make install, eg in /usr/local/share/xmq/library.
mkdir -p "$OUTPUT/installed"
export XMQ_INSTALLED_LIBRARY="$OUTPUT/installed"
CACHE="$HOME/.local/share/xmq/library"
# The file edits are done with python3 (needed for build_library_index.py anyway),
# since sed -i and touch -d differ between GNU and BSD (macos).

# replace_first <old> <new> <file>, in the first line.
replace_first()
{
    python3 -c 'import sys
p, old, new = sys.argv[1], sys.argv[2], sys.argv[3]
lines = open(p, encoding="utf-8").read().split("\n")
lines[0] = lines[0].replace(old, new, 1)
open(p, "w", encoding="utf-8").write("\n".join(lines))' "$3" "$1" "$2"
}

# touch_later <seconds> <file>, set the modification time to now plus seconds.
touch_later()
{
    python3 -c 'import os, sys, time
t = time.time() + int(sys.argv[1])
os.utime(sys.argv[2], (t, t))' "$1" "$2"
}

# bad_sha256 <path> <index.xmq>, replace the sha256 of the file entry with the path by zeros.
bad_sha256()
{
    python3 -c 'import re, sys
path, p = sys.argv[1], sys.argv[2]
s = open(p, encoding="utf-8").read()
i = re.search(r"path *= " + re.escape(path) + r"\n", s).start()
m = re.compile(r"sha256 *= [0-9a-f]+").search(s, i)
s = s[:m.start()] + "sha256 = " + "0" * 64 + s[m.end():]
open(p, "w", encoding="utf-8").write(s)' "$1" "$2"
}

TOTAL=$(grep -c 'file(path' "$OUTPUT/server/index.xmq")

# Nothing is downloaded yet, every file is new.
"$PROG" check-library > "$OUTPUT/check1.txt" || fail "check-library failed"
[ "$(grep -c '^new ' "$OUTPUT/check1.txt")" = "$TOTAL" ] || fail "not all $TOTAL files are new"
[ -d "$CACHE/data" ] && fail "check-library downloaded files"

"$PROG" refresh-library > "$OUTPUT/refresh1.txt" || fail "refresh-library failed"
cmp -s "$OUTPUT/server/data/csv.ixml" "$CACHE/data/csv.ixml" || fail "csv.ixml was not refreshed"
"$PROG" check-library > "$OUTPUT/check2.txt"
grep -q "The library is up to date ($TOTAL files)" "$OUTPUT/check2.txt" || fail "not up to date after refresh-library"

# A file changed on the server, more recently than the local one, without a new version is
# refused. With the new version 1b it is copied to csv_v1b.ixml and csv_v1.ixml.
echo '{changed}' >> "$OUTPUT/server/data/csv.ixml"
scripts/build_library_index.py "$OUTPUT/server" > /dev/null 2> "$OUTPUT/noversion.err" && fail "a change without a new version was accepted"
grep -q 'give it a new version (eg v1b or v2a)' "$OUTPUT/noversion.err" || fail "no message for a change without a new version"
replace_first '{ v1a ' '{ v1b ' "$OUTPUT/server/data/csv.ixml"
touch_later 3600 "$OUTPUT/server/data/csv.ixml"
scripts/build_library_index.py "$OUTPUT/server" > /dev/null 2>&1 || fail "the index with csv v1b"
cmp -s "$OUTPUT/server/data/csv.ixml" "$OUTPUT/server/data/csv_v1b.ixml" || fail "no csv_v1b.ixml"
cmp -s "$OUTPUT/server/data/csv.ixml" "$OUTPUT/server/data/csv_v1.ixml" || fail "csv_v1.ixml is not the latest 1.x"
grep -q '{changed}' "$OUTPUT/server/data/csv_v1a.ixml" && fail "csv_v1a.ixml was changed"
"$PROG" check-library > "$OUTPUT/check3.txt"
grep -q '^updated data/csv.ixml' "$OUTPUT/check3.txt" || fail "the changed csv.ixml is not listed"
grep -q '^new     data/csv_v1b.ixml' "$OUTPUT/check3.txt" || fail "the new csv_v1b.ixml is not listed"
"$PROG" refresh-library > /dev/null || fail "refresh-library of csv.ixml failed"
cmp -s "$OUTPUT/server/data/csv.ixml" "$CACHE/data/csv.ixml" || fail "the changed csv.ixml was not refreshed"

# A file changed locally, more recently than the one on the server, is kept.
echo '{mine}' >> "$CACHE/data/tsv.ixml"
touch_later 7200 "$CACHE/data/tsv.ixml"
"$PROG" check-library > "$OUTPUT/check4.txt"
grep -q '^newer   data/tsv.ixml' "$OUTPUT/check4.txt" || fail "the local tsv.ixml is not listed as newer"
"$PROG" refresh-library > /dev/null
grep -q '{mine}' "$CACHE/data/tsv.ixml" || fail "refresh-library replaced the newer local tsv.ixml"

# A download with another sha256 than the index is not used.
rm "$CACHE/core/xmq.ixml"
bad_sha256 core/xmq.ixml "$OUTPUT/server/index.xmq"
"$PROG" refresh-library > /dev/null 2> "$OUTPUT/badsha.err" && fail "a bad sha256 exits 0"
grep -q 'is not the one in the library index' "$OUTPUT/badsha.err" || fail "no message for a bad sha256"
[ -f "$CACHE/core/xmq.ixml" ] && fail "a download with a bad sha256 was kept"

# The refreshed files are used for ixml: names before the installed library.
printf 'a,b\n1,2\n' > "$OUTPUT/in.csv"
"$PROG" --verbose ixml:data/csv "$OUTPUT/in.csv" to-xmq 2>&1 > /dev/null | grep -q "$CACHE/data/csv.ixml" || fail "ixml:data/csv does not use the refreshed file"

# The versions of a grammar and its description, with the date of the index.
"$PROG" list-library > "$OUTPUT/list.txt" || fail "list-library failed"
grep -q '^Library index .* created [0-9-]* [0-9:]* UTC$' "$OUTPUT/list.txt" || fail "list-library does not print when the index was created"
grep -q '^ixml:data/csv  *v1ab$' "$OUTPUT/list.txt" || fail "list-library does not list ixml:data/csv v1ab"
grep -q '^xsl:core/xmq  *v2a$' "$OUTPUT/list.txt" || fail "list-library does not list xsl:core/xmq v2a"
grep -q 'csv_v1' "$OUTPUT/list.txt" && fail "list-library lists the versioned copies"
"$PROG" describe-library ixml:data/csv > "$OUTPUT/describe.txt" || fail "describe-library failed"
grep -q '^Library index .* created ' "$OUTPUT/describe.txt" || fail "describe-library does not print when the index was created"
grep -q '^ixml:data/csv v1b$' "$OUTPUT/describe.txt" || fail "describe-library does not print the version"
grep -q '^Create a table/tr/td from a csv file' "$OUTPUT/describe.txt" || fail "describe-library does not print the description"
grep -q '^Versions of ixml:data/csv: v1ab$' "$OUTPUT/describe.txt" || fail "describe-library does not print the versions"
"$PROG" describe-library ixml:data/csv_v1a | grep -q '^ixml:data/csv_v1a v1a$' || fail "describe-library of csv_v1a"
"$PROG" describe-library ixml:data/nothing > /dev/null 2>&1 && fail "describe-library of a missing name exits 0"

# A major version name is used like any library name.
"$PROG" ixml:data/csv_v1 "$OUTPUT/in.csv" to-xmq | grep -q 'td = 1' || fail "ixml:data/csv_v1 does not parse"

echo "OK: library refresh"
