#!/bin/sh
# libxmq - Copyright (C) 2026 Fredrik Öhrström (spdx: MIT)

# Build libxmq for javascript (src/main/ecmascript, typescript) with tsc:
#   scripts/build_libxmq_js.sh build/libxmq_js 4.2.0
# OUTPUT/package is the npm package: esm/ (es modules) and cjs/ (commonjs), both with
# .d.ts files, package.json, README.md and LICENSE. The dev tools (typescript, and
# @xmldom/xmldom for the tests) are installed with npm into OUTPUT/tools, pinned below.

set -e

TYPESCRIPT=typescript@7.0.2
XMLDOM=@xmldom/xmldom@0.9.12

if [ -z "$1" ] || [ -z "$2" ]
then
    echo "Usage: build_libxmq_js.sh output_dir version"
    exit 1
fi

OUTPUT="$1"
VERSION="$2"
SRC=src/main/ecmascript
TOOLS="$OUTPUT/tools"
PKG="$OUTPUT/package"
TSC="$TOOLS/node_modules/.bin/tsc"

if ! command -v npm > /dev/null 2>&1
then
    echo "build_libxmq_js.sh: npm not found"
    exit 1
fi

if [ ! -x "$TSC" ] || [ ! -f "$TOOLS/node_modules/@xmldom/xmldom/package.json" ]
then
    echo "Installing $TYPESCRIPT $XMLDOM into $TOOLS"
    mkdir -p "$TOOLS"
    echo '{"private":true}' > "$TOOLS/package.json"
    (cd "$TOOLS" && npm install --silent --no-audit --no-fund --save-exact "$TYPESCRIPT" "$XMLDOM")
fi

rm -rf "$PKG"
mkdir -p "$PKG"

"$TSC" -p "$SRC/tsconfig.json" --module esnext --outDir "$PKG/esm"
"$TSC" -p "$SRC/tsconfig.json" --module commonjs --outDir "$PKG/cjs"
# The package is "type": "module", the commonjs files say so themselves.
echo '{"type":"commonjs"}' > "$PKG/cjs/package.json"

sed "s/@VERSION@/$VERSION/" "$SRC/package.json" > "$PKG/package.json"
cp "$SRC/README.md" LICENSE "$PKG/"

echo "Built $PKG ($(find "$PKG" -name '*.js' | wc -l) javascript files)"
