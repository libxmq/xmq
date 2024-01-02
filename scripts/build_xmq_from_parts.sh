#!/bin/sh
# libxmq - Copyright 2023 Fredrik Öhrström (spdx: MIT)

# Replace #include "parts/..." with the actual content of the included files, c and h files.
# This is used to genereate the dist/xmq.c file which is a self-contained single source
# file suitable for copy/pasting into your own project.

if [ -z "$1" ] || [ -z "$2" ]
then
    echo "Usage: build_xmq_from_parts output_dir xmq.c"
    exit 0
fi

# Assume this script is run from the source root where VERSION is stored.
VERSION="\"$(cat ../../VERSION)\""
echo "VERSION=${VERSION}"

ROOT=$1
XMQ=$2
XMQ_H=$(dirname $XMQ)/xmq.h
PARTS=$(dirname $XMQ)/parts

mkdir -p ${ROOT}
cp $XMQ ${ROOT}/xmq-in-progress
cp $XMQ_H ${ROOT}

SED=sed
if ! sed -doesnotexist 2>&1 | grep -q GNU
then
    SED=gsed
fi

do_part() {
    PART="$1"
    PART_UC=$(echo "$1" | tr a-z A-Z)

    echo "// PARTS ${PART_UC} ////////////////////////////////////////" > ${ROOT}/parts_${PART}_h
    $SED -n "/define ${PART_UC}_H/,/endif \/\/ ${PART_UC}_H/p" ${PARTS}/${PART}.h | $SED '1d; $d' >> ${ROOT}/parts_${PART}_h
    $SED -e "/${PART}\.h\"/ {" -e "r ${ROOT}/parts_${PART}_h" -e 'd }' ${ROOT}/xmq-in-progress > ${ROOT}/tmp

    echo "// PARTS ${PART_UC}_C ////////////////////////////////////////" > ${ROOT}/parts_${PART}_c
    echo >> ${ROOT}/parts_${PART}_c
    $SED -n "/ifdef ${PART_UC}_MODULE/,/endif \/\/ ${PART_UC}_MODULE/p" ${PARTS}/${PART}.c >> ${ROOT}/parts_${PART}_c
    echo >> ${ROOT}/parts_${PART}_c
    $SED -e "/${PART}\.c/ {" -e "r ${ROOT}/parts_${PART}_c" -e 'd }' ${ROOT}/tmp > ${ROOT}/xmq-in-progress
}

do_version() {
    $SED "s/return VERSION;/return ${VERSION};/" ${ROOT}/xmq-in-progress > ${ROOT}/tmp
    mv ${ROOT}/tmp ${ROOT}/xmq-in-progress
}

do_part always
do_part hashmap
do_part json
do_part membuffer
do_part stack
do_part text
do_part utf8
do_part entities
do_part xml
do_part xmq_internals
do_part xmq_parser
do_part xmq_printer

do_version
