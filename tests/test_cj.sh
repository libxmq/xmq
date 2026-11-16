#!/bin/bash
# libxmq - Copyright 2026 Fredrik Öhrström (spdx: MIT)
#
# Compare the C command line (xmq) with the Java command line (xmqj) byte for byte,
# output, errors and exit code of "to-xml", on every xml file in the repository and in
# the libxml2 test suite (tests/libxml2), plain and after expand-entities, and of
# "transform" on the libxslt test suite (tests/libxslt) and of "validate" on the libxml2
# schema test suite (tests/libxml2/test/schemas) and the tests/xsd_*.test cases.
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
find "$ROOT" \( -path "$ROOT/build" -o -path "$ROOT/.git" -o -path "$ROOT/3rdparty" -o -path "$ROOT/fuzz_testcases" -o -path "$ROOT/fuzz_findings_clang" \) -prune -o -name "*.xml" -print > "$FILES"
# The libxml2 test suite is in tests/libxml2, found by the find above.
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

# transform: every stylesheet name.xsl with an input name.xml of the libxslt test suite
# and of tests/xml_diff/xslt, run from inside (a copy of) its directory like libxslt's
# runtest, with the parameters of runtest. C and Java run in their own copies, so that
# the files written by xsl:document can be compared too.
XSLT_DIRS=""
for d in REC2 REC general encoding documents numbers keys namespaces extensions reports
do
    [ -d "$ROOT/tests/libxslt/tests/$d" ] && XSLT_DIRS="$XSLT_DIRS $ROOT/tests/libxslt/tests/$d"
done
XSLT_DIRS="$XSLT_DIRS $ROOT/tests/xml_diff/xslt"
XSLT_CASES="$OUT/xslt_cases.txt"
: > "$XSLT_CASES"
for d in $XSLT_DIRS
do
    k=$(echo "$d" | md5sum | cut -c1-12)
    mkdir -p "$OUT/xslt/c/$k" "$OUT/xslt/j/$k"
    cp -r "$d"/. "$OUT/xslt/c/$k"
    cp -r "$d"/. "$OUT/xslt/j/$k"
    for x in "$d"/*.xsl
    do
        b=$(basename "$x" .xsl)
        [ -f "$d/$b.xml" ] && echo "$k $b $d" >> "$XSLT_CASES"
    done
done
onex()
{
    k=$1
    b=$2
    d=$3
    (cd "$OUT/xslt/c/$k"; timeout 60 "$X" "$b.xml" transform --stringparam=test=passed_value --stringparam=test2=passed_value2 "$b.xsl" to-xml > "$OUT/x_${k}_${b}.c" 2>&1; echo "rc=$?" >> "$OUT/x_${k}_${b}.c")
    (cd "$OUT/xslt/j/$k"; timeout 120 "$J" "$b.xml" transform --stringparam=test=passed_value --stringparam=test2=passed_value2 "$b.xsl" to-xml > "$OUT/x_${k}_${b}.j" 2>&1; echo "rc=$?" >> "$OUT/x_${k}_${b}.j")
    if cmp -s "$OUT/x_${k}_${b}.c" "$OUT/x_${k}_${b}.j"
    then
        rm "$OUT/x_${k}_${b}.c" "$OUT/x_${k}_${b}.j"
    else
        echo "DIFF: $d/$b.xsl transform (diff $OUT/x_${k}_${b}.c $OUT/x_${k}_${b}.j)" >> "$OUT/differ.txt"
    fi
}
export -f onex
xargs -P "${JOBS:-8}" -L 1 bash -c 'onex "$@"' _ < "$XSLT_CASES"
NX=$(wc -l < "$XSLT_CASES")
N=$((N + NX))
# The files written by xsl:document.
if ! diff -r "$OUT/xslt/c" "$OUT/xslt/j" > "$OUT/xslt_files.diff"
then
    echo "DIFF: files written by the transforms (see $OUT/xslt_files.diff)" >> "$OUT/differ.txt"
fi

# validate: every schema prefix_N.xsd of the libxml2 schema suite with its instances
# prefix_*_M.xml (paired like schemasTest of runtest), run from tests/libxml2 with the
# relative paths of runtest, and the xsd_*.test cases of tests (see tests/test_xsd.sh).
VALIDATE_CASES="$OUT/validate_cases.txt"
: > "$VALIDATE_CASES"
SCHEMAS="$ROOT/tests/libxml2/test/schemas"
if [ -d "$SCHEMAS" ]
then
    for x in "$SCHEMAS"/*_*.xsd
    do
        b=$(basename "$x" .xsd)
        p=$b
        case "$p" in *_?) p=${p%_?} ;; esac
        case "$p" in *_?) p=${p%_?} ;; esac
        for i in "$SCHEMAS/${p}"_*.xml
        do
            [ -f "$i" ] || continue
            ib=$(basename "$i")
            # runtest only processes instances named ..._M.xml
            case "$ib" in *_?.xml) ;; *) continue ;; esac
            echo "$ROOT/tests/libxml2 ./test/schemas/$ib ./test/schemas/$b.xsd" >> "$VALIDATE_CASES"
        done
    done
fi
mkdir -p "$OUT/xsd"
for t in "$ROOT"/tests/xsd_*.test
do
    [ -f "$t" ] || continue
    n=$(basename "$t" .test)
    sed -n '/^XSD.*$/,/^INPUT$/p' "$t" | tail -n +2 | sed '$d' > "$OUT/xsd/$n.xsd"
    sed -n '/^INPUT$/,/^OUTPUT$/p' "$t" | tail -n +2 | sed '$d' > "$OUT/xsd/$n.input"
    echo "$OUT/xsd $n.input $n.xsd" >> "$VALIDATE_CASES"
done
onev()
{
    d=$1
    i=$2
    s=$3
    k=$(echo "$d $i $s" | md5sum | cut -c1-12)
    (cd "$d"; timeout 60 "$X" "$i" validate "$s" to-xml > "$OUT/v_$k.c" 2>&1; echo "rc=$?" >> "$OUT/v_$k.c")
    (cd "$d"; timeout 120 "$J" "$i" validate "$s" to-xml > "$OUT/v_$k.j" 2>&1; echo "rc=$?" >> "$OUT/v_$k.j")
    if cmp -s "$OUT/v_$k.c" "$OUT/v_$k.j"
    then
        rm "$OUT/v_$k.c" "$OUT/v_$k.j"
    else
        echo "DIFF: $d $i validate $s (diff $OUT/v_$k.c $OUT/v_$k.j)" >> "$OUT/differ.txt"
    fi
}
export -f onev
xargs -P "${JOBS:-8}" -L 1 bash -c 'onev "$@"' _ < "$VALIDATE_CASES"
NV=$(wc -l < "$VALIDATE_CASES")
N=$((N + NV))

D=$(wc -l < "$OUT/differ.txt")
sort "$OUT/differ.txt" | sed "s#$ROOT/##"
if [ "$D" = "0" ]
then
    echo "OK: C and Java command lines give the same to-xml for $N cases ($NX transforms, $NV validates)"
else
    echo "ERR: C and Java command lines differ in $D of $N cases ($NX transforms, $NV validates)"
    exit 1
fi
