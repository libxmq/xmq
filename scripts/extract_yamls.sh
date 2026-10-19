#!/usr/bin/env bash
set -euo pipefail
shopt -s nullglob

# Extract the input section (between INPUT and OUTPUT) of xmq regression tests
# and turn each one into an .yml fuzz-testcase seed.
#
#   tests/yaml_001_node.test  ->  fuzz_testcases/xmq/yaml_001_node.yml
#   ...
#
# Each .test file has the layout:
#   INPUT
#   prutt: 1
#   OUTPUT
#   ...
#   END

SRC_DIR=${1:-tests}
OUT_DIR=${2:-fuzz_testcases/yaml}

# First START / DEBUG pair only (matches the regression test layout above).
extract() {
    awk '
        /^INPUT$/   { in_start = 1; next }
        /^OUTPUT$/   { if (in_start) exit }
        in_start    { print }
    ' "$1"
}

mkdir -p "$OUT_DIR"

count=0
skip=0
# Range 001 .. 038 inclusive, matching tests/NNN_*.test
for f in tests/yaml_*.test; do
    base=$(basename "$f" .test)
    out="$OUT_DIR/${base}.yml"
    if extract "$f" > "$out"; then
        count=$((count + 1))
    else
        echo "ERROR: failed to extract $f" >&2
        rm -f "$out"
    fi
done

echo "Extracted $count .yml seed(s) into $OUT_DIR ($skip number(s) had no tests)."
