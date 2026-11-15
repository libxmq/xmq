#!/bin/bash
# libxmq - Copyright 2026 Fredrik Öhrström (spdx: MIT)
#
# Compare the C command line (xmq) with the Java command line (xmqj) byte for byte,
# output, errors and exit code of "to-xml", on every xml file in the repository and in
# the libxml2 test suite (if fetched), plain and after expand-entities.
#
# Usage: tests/test_cj.sh XMQ_BINARY XMQJ_BINARY OUTPUT_DIR
# JOBS=n sets the number of parallel jobs (default 8).

X=$1
J=$2
OUT=$3
if [ ! -x "$X" ] || [ ! -x "$J" ] || [ -z "$OUT" ]
then
    echo "Usage: tests/test_cj.sh XMQ_BINARY XMQJ_BINARY OUTPUT_DIR"
    exit 1
fi

ROOT=$(cd "$(dirname "$0")/.." && pwd)
rm -rf "$OUT"
mkdir -p "$OUT"
OUT=$(cd "$OUT" && pwd)
X=$(cd "$(dirname "$X")" && pwd)/$(basename "$X")
J=$(cd "$(dirname "$J")" && pwd)/$(basename "$J")

# Files where C and Java deliberately or knowingly differ (file names, space separated).
EXCLUDE=""

FILES="$OUT/files.txt"
find "$ROOT" \( -path "$ROOT/build" -o -path "$ROOT/.git" -o -path "$ROOT/3rdparty" \) -prune -o -name "*.xml" -print > "$FILES"
# The libxml2 test suite, from 3rdparty/fetch_and_build.sh libxml2 or, as on the
# github build, from 3rdparty/fetch_and_build.sh x86_64-pc-linux-gnu.
for d in "$ROOT/3rdparty/libxml2/test" "$ROOT/3rdparty/libxml2-posix/test"
do
    if [ -d "$d" ]
    then
        find "$d" -name "*.xml" >> "$FILES"
        break
    fi
done
for e in $EXCLUDE
do
    grep -v "/$e\$" "$FILES" > "$FILES.tmp"
    mv "$FILES.tmp" "$FILES"
done

export X J OUT
one()
{
    f=$1
    shift
    k=$(echo "$f $*" | md5sum | cut -c1-12)
    (cd "$OUT"; timeout 60 "$X" "$f" "$@" to-xml > "$OUT/$k.c" 2>&1; echo "rc=$?" >> "$OUT/$k.c")
    (cd "$OUT"; timeout 120 "$J" "$f" "$@" to-xml > "$OUT/$k.j" 2>&1; echo "rc=$?" >> "$OUT/$k.j")
    if cmp -s "$OUT/$k.c" "$OUT/$k.j"
    then
        rm "$OUT/$k.c" "$OUT/$k.j"
    else
        echo "DIFF: $f $* (diff $OUT/$k.c $OUT/$k.j)" >> "$OUT/differ.txt"
    fi
}
export -f one

touch "$OUT/differ.txt"
# Every file plain, and the files declaring entities also after expand-entities.
{
    cat "$FILES"
    grep -l "<!ENTITY" $(cat "$FILES") 2>/dev/null | sed 's/$/ expand-entities/'
} | xargs -P "${JOBS:-8}" -L 1 bash -c 'one "$@"' _

N=$(( $(wc -l < "$FILES") + $(grep -l "<!ENTITY" $(cat "$FILES") 2>/dev/null | wc -l) ))
D=$(wc -l < "$OUT/differ.txt")
sort "$OUT/differ.txt" | sed "s#$ROOT/##"
if [ "$D" = "0" ]
then
    echo "OK: C and Java command lines give the same to-xml for $N cases"
else
    echo "ERR: C and Java command lines differ in $D of $N cases"
    exit 1
fi
