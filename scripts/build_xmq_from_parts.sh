#!/bin/sh
# libxmq - Copyright 2023 Fredrik Öhrström (spdx: MIT)

# Replace #include "parts/..." with the actual content of the included files, c and h files.
# This is used to generate the dist/xmq.c file which is a self-contained single source
# file suitable for copy/pasting into your own project.

if [ -z "$1" ] || [ -z "$2" ]
then
    echo "Usage: build_xmq_from_parts output_dir .../xmq.c .../dist/VERSION"
    exit 0
fi

ROOT=$1
XMQ=$2
# The dist/VERSION is used to inject into the xmq.c source code.
VERSION="\"$(cat "$3")\""
XMQ_H=$(dirname "$XMQ")/xmq.h
PARTS=$(dirname "$XMQ")/parts

echo "VERSION=${VERSION}"

mkdir -p "${ROOT}"
cp "$XMQ" "${ROOT}/xmq-in-progress"
cp "$XMQ_H" "${ROOT}"

SED="sed"
if ! sed -doesnotexist 2>&1 | grep -q GNU
then
    SED="gsed"
fi

do_part() {
    PART="$1"
    PART_UC=$(echo "$1" | tr a-z A-Z)

    echo "// PART H ${PART_UC} ////////////////////////////////////////" > "${ROOT}/parts_${PART}_h"
    $SED -n "/define ${PART_UC}_H/,/endif \/\/ ${PART_UC}_H/p" "${PARTS}/${PART}.h" | $SED '1d; $d' >> "${ROOT}/parts_${PART}_h"
    $SED -e "/\/${PART}\.h\"/ {" -e "r ${ROOT}/parts_${PART}_h" -e 'd }' "${ROOT}/xmq-in-progress" > "${ROOT}/tmp"

    echo "// PART C ${PART_UC}_C ////////////////////////////////////////" > ${ROOT}/parts_${PART}_c
    echo >> ${ROOT}/parts_${PART}_c
    $SED -n "/ifdef ${PART_UC}_MODULE/,/endif \/\/ ${PART_UC}_MODULE/p" ${PARTS}/${PART}.c >> ${ROOT}/parts_${PART}_c
    echo >> ${ROOT}/parts_${PART}_c
    $SED -e "/\/${PART}\.c/ {" -e "r ${ROOT}/parts_${PART}_c" -e 'd }' ${ROOT}/tmp > ${ROOT}/xmq-in-progress
}

do_version() {
    $SED "s/return VERSION;/return ${VERSION};/" ${ROOT}/xmq-in-progress > ${ROOT}/tmp
    mv ${ROOT}/tmp ${ROOT}/xmq-in-progress
}

do_building_dist_xmq() {
    $SED "s|//#define BUILDING_DIST_XMQ|#define BUILDING_DIST_XMQ|" ${ROOT}/xmq-in-progress > ${ROOT}/tmp
    mv ${ROOT}/tmp ${ROOT}/xmq-in-progress
}

do_building_dist_xmq
do_part always
do_part colors
do_part core
do_part default_themes
do_part hashmap
do_part ixml
do_part json
do_part membuffer
do_part stack
do_part text
do_part utf8
do_part entities
do_part vector
do_part xml
do_part xmq_internals
do_part xmq_parser
do_part xmq_printer
do_part yaep_allocate
do_part yaep_hashtab
do_part yaep_objstack
do_part yaep_vlobject
do_part yaep
do_part yaep_structs
do_part yaep_util
do_part yaep_symbols
do_part yaep_terminal_bitset


do_version
