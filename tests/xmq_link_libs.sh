#!/bin/bash
# libxmq - Copyright 2026 Fredrik Öhrström (spdx: MIT)
#
# Print the libraries needed to link a program (eg a generated parser)
# against libxmq.a from a build directory. libxmq has no dependencies
# beyond the C library.
#
# Usage: tests/xmq_link_libs.sh build/x86_64-pc-linux-gnu/release

LIBDIR=${1:-build/x86_64-pc-linux-gnu/release}

echo "$LIBDIR/libxmq.a -lpthread -lm"
