#!/bin/sh
# libxmq - Copyright 2026 Fredrik Öhrström (spdx: MIT)
#
# Generate the api documentation page for the public Java api, the classes in
# src/main/java/org/libxmq (not the implementation in imp).
#
# Each file is parsed with the ixml grammar library/lang/java.ixml and cleaned up with
# library/lang/java.xslq, which gives a javadoc element for each /** ... */ as the first
# child of the declaration it documents. The files are combined into a single document
# javaapi { file(name) { compilation_unit ... } ... } with replace-entity, which is
# transformed with web/resources/genjavaapi.xslq into a page with a section per class.
#
# Usage: scripts/generate_java_api_doc.sh [output.html] [xmq binary]
# The defaults are build/web/javaapi.html and build/default/release/xmq.

OUT=${1:-build/web/javaapi.html}
XMQ=${2:-build/default/release/xmq}

if [ ! -x "$XMQ" ]
then
    echo "Cannot find the xmq binary $XMQ, run make first."
    exit 1
fi

TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

# The combined document, with an entity for each parsed file.
echo "javaapi {" > "$TMP/javaapi.xmq"
set --
for f in src/main/java/org/libxmq/*.java
do
    name=$(basename "$f" .java)
    "$XMQ" library/lang/java.ixml "$f" transform library/lang/java.xslq to-xml > "$TMP/$name.xml" || exit 1
    echo "    file(name = $name) { &$name; }" >> "$TMP/javaapi.xmq"
    set -- "$@" replace-entity "$name" --with-file="$TMP/$name.xml"
done
echo "}" >> "$TMP/javaapi.xmq"

mkdir -p "$(dirname "$OUT")"
TODAY=$(date +'%Y-%m-%d %H:%M')
"$XMQ" "$TMP/javaapi.xmq" "$@" \
       transform --stringparam=date="$TODAY" \
                 --stringparam=examples="$(pwd)/examples/java/examples.xmq" \
                 web/resources/genjavaapi.xslq \
       to-html > "$OUT.tmp" || { rm -f "$OUT.tmp"; exit 1; }
mv "$OUT.tmp" "$OUT"
echo "Wrote $OUT"
