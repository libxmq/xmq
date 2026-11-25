#!/bin/sh
# libxmq - Copyright (C) 2026 Fredrik Öhrström (spdx: MIT)

# Run the tests of libxmq for javascript (make libxmq_js) with node:
#   tests/test_libxmq_js.sh build/libxmq_js build/libxmq_js/test_output
# Every tests/libxmq_js/test_*.mjs gets the package directory and the output directory
# as arguments and prints OK: or ERR: lines.

BUILD="$1"
OUTPUT="$2"

if [ -z "$BUILD" ] || [ -z "$OUTPUT" ]
then
    echo "Usage: tests/test_libxmq_js.sh build/libxmq_js build/libxmq_js/test_output"
    exit 1
fi

if [ ! -f "$BUILD/package/esm/index.js" ]
then
    echo "tests/test_libxmq_js.sh: $BUILD/package not found, run make libxmq_js"
    exit 1
fi

if ! command -v node > /dev/null
then
    echo "tests/test_libxmq_js.sh: node not found"
    exit 1
fi

rm -rf "$OUTPUT"
mkdir -p "$OUTPUT"

# The tests find @xmldom/xmldom in the tools of the build (with require).
NODE_PATH="$(realpath "$BUILD/tools/node_modules")"
export NODE_PATH

status=0
for t in tests/libxmq_js/test_*.mjs
do
    node "$t" "$BUILD/package" "$OUTPUT" || status=1
done

exit $status
