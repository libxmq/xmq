#!/bin/sh

set -e

if [ "$1" = "" ] || [ "$1" = "-h" ]; then
    echo "Usage: install.sh [build_dir] [dest_dir]
    Example: install.sh build/default/release /usr/local
    "
    exit 0
fi

FROM="$1"
DESTDIR="$2"

if [ ! -f "$FROM/../spec.inc" ]; then
    echo "Oups, please supply a build dir such as build/default/release"
    exit 1
fi

if [ -z "$DESTDIR" ]; then
    . "$FROM/../spec.inc"
    DESTDIR="$PREFIX"
fi

if [ ! -d "$DESTDIR" ]; then
    echo "Oups, please supply a valid dest_dir directory. The directory \"$DESTDIR\" does not exist."
    exit 1
fi

#. "${FROM}/../spec.inc"

install_cmd="install -m 755 -s"
install_lib_cmd="install -m 644"
install_shared_lib_cmd="install -m 644 -s"

if [ "$(uname)" = "Darwin" ]; then
    install_cmd="install -m 755 -c"
    install_lib_cmd="install -m 644 -c"
    install_shared_lib_cmd="install -m 644 -c"
fi

# Ensure destination directories exist
mkdir -p "${DESTDIR}/bin"
mkdir -p "${DESTDIR}/lib"
mkdir -p "${DESTDIR}/share/man/man1"
mkdir -p "${DESTDIR}/share/bash-completion/completions"

$install_cmd "${FROM}/xmq" "${DESTDIR}/bin/xmq"
$install_lib_cmd "${FROM}/libxmq.a" "${DESTDIR}/lib/libxmq.a"
$install_shared_lib_cmd "${FROM}/libxmq.so" "${DESTDIR}/lib/libxmq.so"
mkdir -p "${DESTDIR}/include"
cp "src/main/c/xmq.h" "${DESTDIR}/include/xmq.h"
chmod 644 "${DESTDIR}/include/xmq.h"
install -m 644 "doc/xmq.1" "${DESTDIR}/share/man/man1/xmq.1"
install -m 644 "scripts/autocompletion_for_xmq.sh" "${DESTDIR}/share/bash-completion/completions/xmq"

echo
echo "Installed xmq and libxmq into: $DESTDIR"
