#!/bin/sh
# libxmq - Copyright 2023 Fredrik Öhrström (spdx: MIT)

if [ -z "$1" ]
then
    echo "Usage: build_xmq_from_parts src/main/c/xmq.c"
    exit 0
fi

XMQ=$1
PARTS=$(dirname $1)/parts

cp $XMQ build/xmq-in-progress

do_part() {
    PART="$1"
    PART_UC=$(echo "$1" | tr a-z A-Z)

    echo "// PARTS ${PART_UC} ////////////////////////////////////////" > build/parts_${PART}_h
    sed -n "/define ${PART_UC}_H/,/endif \/\/ ${PART_UC}_H/p" src/main/c/parts/${PART}.h | sed '1d; $d' >> build/parts_${PART}_h
    sed -e "/${PART}.h/ {" -e "r build/parts_${PART}_h" -e 'd }' build/xmq-in-progress > build/tmp

    echo "// PARTS ${PART_UC}_C ////////////////////////////////////////" > build/parts_${PART}_c
    echo >> build/parts_${PART}_c
    sed -n "/ifdef ${PART_UC}_MODULE/,/endif \/\/ ${PART_UC}_MODULE/p" src/main/c/parts/${PART}.c >> build/parts_${PART}_c
    echo >> build/parts_${PART}_c
    sed -e "/${PART}.c/ {" -e "r build/parts_${PART}_c" -e 'd }' build/tmp > build/xmq-in-progress
    rm build/tmp
}

do_part utils
do_part membuffer
do_part stack
do_part hashmap
