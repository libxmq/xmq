#!/bin/bash
# libxmq - Copyright 2026 Fredrik Öhrström (spdx: MIT)
#
# Print the libraries needed to link a program (eg a generated parser)
# against libxmq.a from a build directory, using the same libxml2, libxslt
# and zlib as the build itself. These come from the spec.mk written by
# configure, so a libxml2 built in 3rdparty is used when the build uses it.
#
# Usage: tests/xmq_link_libs.sh build/x86_64-pc-linux-gnu/release
#
# Without a spec.mk the installed system libraries are used.

LIBDIR=${1:-build/x86_64-pc-linux-gnu/release}
SPEC=$(dirname "$LIBDIR")/spec.mk

# Print the value of VAR:=value from spec.mk.
spec_var()
{
    grep -m 1 "^$1:=" "$SPEC" | sed "s/^$1:=//"
}

if [ -f "$SPEC" ]
then
    echo "$LIBDIR/libxmq.a $(spec_var LIBXSLT_LIBS) $(spec_var LIBXML2_LIBS) $(spec_var ZLIB_LIBS) -lpthread -lm" | tr -s ' '
else
    echo "$LIBDIR/libxmq.a -lxslt -lxml2 -lz -lpthread -lm"
fi
