#!/bin/sh
# libxmq - Copyright 2023 Fredrik Öhrström (spdx: MIT)


if [ -z "$1" ] || [ -z "$2" ]
then
    echo "Usage: build_xmq_from_parts output_dir xmq.c"
    exit 0
fi

ROOT=$1
XMQ=$2
XMQ_H=$(dirname $XMQ)/xmq.h
PARTS=$(dirname $XMQ)/parts

mkdir -p ${ROOT}
cp $XMQ ${ROOT}/xmq-in-progress
cp $XMQ_H ${ROOT}

do_part() {
    PART="$1"
    PART_UC=$(echo "$1" | tr a-z A-Z)

    echo "// PARTS ${PART_UC} ////////////////////////////////////////" > ${ROOT}/parts_${PART}_h
    sed -n "/define ${PART_UC}_H/,/endif \/\/ ${PART_UC}_H/p" ${PARTS}/${PART}.h | sed '1d; $d' >> ${ROOT}/parts_${PART}_h
    sed -e "/${PART}\.h/ {" -e "r ${ROOT}/parts_${PART}_h" -e 'd }' ${ROOT}/xmq-in-progress > ${ROOT}/tmp

    echo "// PARTS ${PART_UC}_C ////////////////////////////////////////" > ${ROOT}/parts_${PART}_c
    echo >> ${ROOT}/parts_${PART}_c
    sed -n "/ifdef ${PART_UC}_MODULE/,/endif \/\/ ${PART_UC}_MODULE/p" ${PARTS}/${PART}.c >> ${ROOT}/parts_${PART}_c
    echo >> ${ROOT}/parts_${PART}_c
    sed -e "/${PART}\.c/ {" -e "r ${ROOT}/parts_${PART}_c" -e 'd }' ${ROOT}/tmp > ${ROOT}/xmq-in-progress
}

do_part utils
do_part membuffer
do_part stack
do_part hashmap
