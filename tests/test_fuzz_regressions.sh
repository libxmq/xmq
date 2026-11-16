#!/bin/bash
# libxmq - Copyright 2026 Fredrik Öhrström (spdx: MIT)
#
# Replay the inputs the fuzzer found (tests/fuzz/regressions.txt) through the fuzz
# harness (tests/fuzz/fuzz_xmq.c), built with gcc and the address and undefined
# behaviour sanitizers. The input buffer ends exactly at stop, without a NUL.
# Usage: tests/test_fuzz_regressions.sh <build output dir>

ROOT=$(cd "$(dirname "$0")/.." && pwd)
if [ -z "$1" ]
then
    echo "Usage: tests/test_fuzz_regressions.sh <build output dir>"
    exit 1
fi
OUT="$1"
mkdir -p "$OUT/inputs" || exit 1
OUT=$(cd "$OUT" && pwd)

CC=${CC:-gcc}
"$ROOT/scripts/build_xmq_from_parts.sh" "$OUT" "$ROOT/src/main/c/xmq.c" "$ROOT/dist/VERSION" || exit 1
cp "$OUT/xmq-in-progress" "$OUT/xmq.c"
$CC -g -O1 -fsanitize=address,undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer \
    -I"$ROOT/src/main/c" -o "$OUT/fuzz_replay" \
    "$ROOT/tests/fuzz/fuzz_replay.c" "$ROOT/tests/fuzz/fuzz_xmq.c" "$OUT/xmq.c" -lm || exit 1

passed=0
failed=0
while IFS='|' read -r name input
do
    case "$name" in ''|'#'*) continue ;; esac
    printf -- "$input" > "$OUT/inputs/$name"
    # The runner is built with ASan, which must be loaded first: drop preloads such as libeatmydata.
    if env -u LD_PRELOAD ASAN_OPTIONS=detect_leaks=1 timeout 10 "$OUT/fuzz_replay" "$OUT/inputs/$name" > "$OUT/$name.log" 2>&1
    then
        passed=$((passed+1))
    else
        echo "ERR: fuzz regression $name (see $OUT/$name.log)"
        failed=$((failed+1))
    fi
done < "$ROOT/tests/fuzz/regressions.txt"

if [ "$failed" != "0" ]
then
    echo "Fuzz regressions passed: $passed failed: $failed"
    exit 1
fi
echo "OK: fuzz regressions ($passed)"
