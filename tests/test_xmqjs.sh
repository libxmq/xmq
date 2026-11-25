#!/bin/sh
# libxmq - Copyright (C) 2026 Fredrik Öhrström (spdx: MIT)

# Run the test suite (tests/test.sh) on xmqjs, the xmq command line compiled to wasm for
# node (make xmqjs).
#   tests/test_xmqjs.sh build/xmqjs build/xmqjs.wasm/test_output [FILTER]
#
# The wasm port is limited, so failures are expected: unlike make test, all tests are run
# (a copy of tests/test.sh that does not stop at the first failure), the failures are
# listed at the end and the exit code is 0. The log is OUTPUT/../test_xmqjs.log.
# Known failures, wasm cannot start other programs:
#   - cmd_003_for_each, cmd_004_for_each_dot, cmd_005_for_each_dotdot (for-each runs sh).
#   - the kaffepot and markup-blitz engine tests (XMQ_IXML_ENGINE runs another program).
#   - traceixml --traceixml=terminal (runs xmq again to render the trace).
#
# What is skipped, and why:
#   - the C api programs tests/test_???_*.c, they link with libxmq.so and xmqjs has none.
#   - the dist test, it builds dist/xmq.c with the native compiler (a linux64 build only).
# Everything else runs with xmqjs as the xmq binary.

XMQJS="$1"
OUTPUT="$2"
FILTER="$3"

if [ -z "$XMQJS" ] || [ -z "$OUTPUT" ]
then
    echo "Usage: tests/test_xmqjs.sh build/xmqjs build/xmqjs.wasm/test_output [FILTER]"
    exit 1
fi

if [ ! -x "$XMQJS" ]
then
    echo "tests/test_xmqjs.sh: $XMQJS not found, run make xmqjs"
    exit 1
fi

if ! command -v node > /dev/null
then
    echo "tests/test_xmqjs.sh: node not found"
    exit 1
fi

# tests/test.sh wants a build directory with xmq in it and a spec.inc next to it. node
# follows the symbolic link, so xmqjs finds its wasm file as usual.
BUILD="$(dirname "$OUTPUT")/test_build"
rm -rf "$BUILD"
mkdir -p "$BUILD"
ln -s "$(realpath "$XMQJS")" "$BUILD/xmq"
cat > "$BUILD/spec.inc" <<EOF
CONF_NAME="wasm32-unknown-emscripten"
CONF_MNEMONIC="xmqjs"
PLATFORM="POSIX"
EOF

LOG="$(dirname "$OUTPUT")/test_xmqjs.log"
sed 's/echo "Testing aborted"; exit 1/echo "Testing continues"/' tests/test.sh > "$BUILD/test.sh"
bash "$BUILD/test.sh" "$BUILD" "$OUTPUT" $FILTER 2>&1 | tee "$LOG"

echo
echo "xmqjs: $(grep -c '^OK' "$LOG") OK, $(grep -c '^SKIP' "$LOG") SKIP, $(grep -c -E '^(ERR|FAIL)' "$LOG") failed (expected for the wasm port, see $0):"
grep -E '^(ERR|FAIL)' "$LOG"
exit 0
