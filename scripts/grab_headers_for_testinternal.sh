#!/bin/sh
# libxmq - Copyright 2023 Fredrik Öhrström (spdx: MIT)

HEADER=$1

# // DECLARATIONS /////////////////////////////////////////////////
# // DEFINITIONS ///////////////////////////////////////////////////////////////////////////////

sed -n '/^\/\/ DECLARATIONS/,/^\/\/ DEFINITIONS/p' $HEADER | tail -n +2 | sed '$d'
