#!/bin/sh
# libxmq - Copyright (C) 2026 Fredrik Öhrström (spdx: MIT)

# Wrap the xmqjs executable (the xmq command line compiled to wasm, make xmqjs) in a node
# package with the grammar library, and pack it with npm pack.
#   scripts/build_xmq_node_package.sh build/xmqjs build/xmqjs.wasm/xmqjs.wasm build/xmq_node_package 4.2.0
# The package directory is OUTPUT/package, the packed file OUTPUT/xmqjs-VERSION.tgz.

set -e

if [ -z "$1" ] || [ -z "$2" ] || [ -z "$3" ] || [ -z "$4" ]
then
    echo "Usage: build_xmq_node_package.sh xmqjs xmqjs.wasm output_dir version"
    exit 1
fi

XMQJS="$1"
XMQJS_WASM="$2"
OUTPUT="$3"
VERSION="$4"
PKG="$OUTPUT/package"

if [ ! -x "$XMQJS" ] || [ ! -f "$XMQJS_WASM" ]
then
    echo "build_xmq_node_package.sh: $XMQJS or $XMQJS_WASM not found, run make xmqjs"
    exit 1
fi

if ! command -v npm >/dev/null 2>&1
then
    echo "build_xmq_node_package.sh: npm not found"
    exit 1
fi

rm -rf "$PKG"
mkdir -p "$PKG"

# In the package the wasm file is next to the executable.
cp "$XMQJS" "$PKG/xmqjs"
cp "$XMQJS_WASM" "$PKG/xmqjs.wasm"
chmod 755 "$PKG/xmqjs"
sed "s/@VERSION@/$VERSION/" src/main/wasm/package.json > "$PKG/package.json"
cp src/main/wasm/README.md CONTRIBUTORS "$PKG/"

# The license of libxmq followed by the notices of libxml2 and libxslt, which the xml
# parts are ported from. The notices are taken from the dist build script (without the
# C comment markers), so that they exist in one place only.
{
    cat LICENSE
    sed -n "/<<'NOTICE'/,/^NOTICE$/p" scripts/build_xmq_from_parts.sh | sed -e '1d' -e '$d' -e '/^\*\/$/d' -e 's|^/\* ||'
} > "$PKG/LICENSE"

# The grammar library, the same files that make install puts in share/xmq/library.
# xmqjs finds it next to itself.
(cd library && find . -name 'test-*' -prune -o -name 'c-tests' -prune -o -type f \
     \( -name '*.ixml' -o -name '*.xslq' -o -name '*.xml' -o -name '*.xmq' \) -print) |
while read -r f
do
    mkdir -p "$(dirname "$PKG/library/$f")"
    cp "library/$f" "$PKG/library/$f"
done

rm -f "$OUTPUT"/xmqjs-*.tgz
(cd "$PKG" && npm pack --silent --pack-destination ..) > /dev/null
echo "Built $OUTPUT/xmqjs-$VERSION.tgz ($(find "$PKG/library" -type f | wc -l) library files)"
