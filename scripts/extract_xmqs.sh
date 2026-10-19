#!/usr/bin/env bash
set -euo pipefail
shopt -s nullglob

# Extract the input section (between START and DEBUG) of xmq regression tests
# and turn each one into an .xmq fuzz-testcase seed.
#
#   tests/001_node.test  ->  fuzz_testcases/xmq/001_node.xmq
#   ...
#   tests/047_dquotes.test -> fuzz_testcases/xmq/047_dquotes.xmq
#
# Each .test file has the layout:
#   START
#   <raw xmq input>      <- this part is extracted
#   DEBUG
#   ...
#   CONTENT
#   ...
#   END

SRC_DIR=${1:-tests}
OUT_DIR=${2:-fuzz_testcases/xmq}

# First START / DEBUG pair only (matches the regression test layout above).
extract() {
    awk '
        /^START$/   { in_start = 1; next }
        /^DEBUG$/   { if (in_start) exit }
        in_start    { print }
    ' "$1"
}

mkdir -p "$OUT_DIR"

count=0
skip=0
# Range 001 .. 047 inclusive, matching tests/NNN_*.test
for n in $(seq 1 47); do
    nn=$(printf '%03d' "$n")
    if compgen -G "$SRC_DIR/${nn}_*.test" > /dev/null; then
        for f in $SRC_DIR/${nn}_*.test; do
            base=$(basename "$f" .test)
            out="$OUT_DIR/${base}.xmq"
            if extract "$f" > "$out"; then
                count=$((count + 1))
            else
                echo "ERROR: failed to extract $f" >&2
                rm -f "$out"
            fi
        done
    else
        echo "WARN: no test files for '$nn' (skipped)" >&2
        skip=$((skip + 1))
    fi
done

echo "Extracted $count .xmq seed(s) into $OUT_DIR ($skip number(s) had no tests)."
