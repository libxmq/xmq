#!/bin/sh

set -e

if [ "$1" = "" ] || [ "$1" = "-h" ]
then
    echo "Usage: uninstall.sh [dest_dir]
    Example: uninstall.sh /usr/local
    "
    exit 0
fi

DESTDIR="$1"

if [ ! -d "$DESTDIR" ]
then
    echo "Oups, please supply a valid dest_dir directory."
    exit 1
fi

rm -f "${DESTDIR}/bin/xmq"
rm -f "${DESTDIR}/lib/libxmq.a"
rm -f "${DESTDIR}/lib/libxmq.so"
rm -f "${DESTDIR}/include/xmq.h"
rm -f "${DESTDIR}/share/man/man1/xmq.1"

rm -f "${DESTDIR}/share/bash-completion/completions/xmq"

# The library installed by install.sh.
if [ -d "${DESTDIR}/share/xmq/library" ]
then
    rm -r "${DESTDIR}/share/xmq/library"
    rmdir "${DESTDIR}/share/xmq" 2> /dev/null || true
fi

echo
echo "xmq and libxmq sucessfully uninstalled."
