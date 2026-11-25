#!/bin/sh
# libxmq - Copyright (C) 2026 Fredrik Öhrström (spdx: MIT)

# Run the tests of libxmq for javascript (make libxmq_js) with node:
#   tests/test_libxmq_js.sh build/libxmq_js build/libxmq_js/test_output
# Every tests/libxmq_js/test_*.mjs gets the package directory and the output directory
# as arguments and prints OK: or ERR: lines. Then the existing test runners of the C xmq
# run with tests/libxmq_js_cli.mjs as the xmq binary, for the tests that it supports:
# tests/test_single.sh and tests/test_pipes.sh (tokenize --type=debugtokens and
# debugcontent of tests/[0-9][0-9][0-9]_*.test and tests/pipe_*.test) and
# tests/test_error.sh (the parse errors of tests/error_*.test).

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

# The tests find @xmldom/xmldom in the tools of the build.
NODE_PATH="$(realpath "$BUILD/tools/node_modules")"
export NODE_PATH

status=0
for t in tests/libxmq_js/test_*.mjs
do
    node "$t" "$BUILD/package" "$OUTPUT" || status=1
done

# The xmq binary for the existing runners.
PROG="$(realpath "$OUTPUT")/xmq"
printf '#!/bin/sh\nLIBXMQ_JS="%s" exec node "%s" "$@"\n' "$(realpath "$BUILD/package")" "$(realpath tests/libxmq_js_cli.mjs)" > "$PROG"
chmod +x "$PROG"

# run_suite runner test_files...: prints the failures and one summary line.
run_suite()
{
    runner="$1"
    shift
    ok=0
    failed=0
    for t in "$@"
    do
        if out=$("$runner" "$PROG" "$OUTPUT/suites" "$t" 2>&1) && echo "$out" | grep -q '^OK'
        then
            ok=$((ok+1))
        else
            failed=$((failed+1))
            echo "$out"
        fi
    done
    if [ "$failed" = 0 ]
    then
        echo "OK: $runner with libxmq_js ($ok tests)"
    else
        echo "ERR: $runner with libxmq_js ($ok ok, $failed failed)"
        status=1
    fi
}

run_suite tests/test_single.sh tests/[0-9][0-9][0-9]_*.test
run_suite tests/test_pipes.sh tests/pipe_*.test
run_suite tests/test_error.sh tests/error_*.test

exit $status
