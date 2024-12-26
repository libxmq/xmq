#!/bin/sh

set -e

if [ "$1" = "" ] || [ "$1" = "-h" ]
then
    echo "Usage: install.sh [build_dir] [dest_dir]
    Example: install.sh build/default/release /usr/local
    "
    exit 0
fi

FROM="$1"
DESTDIR="$2"

if [ ! -f "$FROM/../spec.inc" ]
then
    echo "Oups, please supply a build dir such as build/default/release"
    exit 1
fi

if [ ! -d "$DESTDIR" ]
then
    echo "Oups, please supply a valid dest_dir directory."
    exit 1
fi

#. "${FROM}/../spec.inc"

install -Dm 755 -s "${FROM}/xmq" "${DESTDIR}/bin/xmq"
install -Dm 644 -s "${FROM}/libxmq.a" "${DESTDIR}/lib/libxmq.a"
install -Dm 644 -s "${FROM}/libxmq.so" "${DESTDIR}/lib/libxmq.so"
cp "src/main/c/xmq.h" "${DESTDIR}/include/xmq.h"
chmod 644 "src/main/c/xmq.h"
install -Dm 644 "doc/xmq.1" "${DESTDIR}/man/man1/xmq.1"
install -Dm 644 "scripts/autocompletion_for_xmq.sh" "${DESTDIR}/share/bash-completion/completions/xmq"

echo
echo "xmq and libxmq sucessfully installed."
