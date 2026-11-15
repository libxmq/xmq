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

do_upstream_notice() {
    # The native xml parts (xml_*.c) are partly derived from libxml2 and libxslt, keep their notices.
    cat > ${ROOT}/notice <<'NOTICE'

/* The xml parts (PART C XML_*) are partly ported from libxml2 (version 2.16.0 as found
   in the libxmq fork), which is covered by the following notice (from libxml2/Copyright):

 Copyright (C) 1998-2012 Daniel Veillard.  All Rights Reserved.
 Copyright (C) The Libxml2 Contributors.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is fur-
nished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FIT-
NESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.

   See CONTRIBUTORS for the libxml2 contributors.
*/

/* The xslt 1.0 parts (PART C XML_LIBXSLT1_*) are ported from libxslt (version 1.1.45
   as found in the libxmq fork), which is covered by the following notice (from
   libxslt/Copyright):

 Copyright (C) 2001-2002 Daniel Veillard.  All Rights Reserved.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is fur-
nished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FIT-
NESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
DANIEL VEILLARD BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CON-
NECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of Daniel Veillard shall not
be used in advertising or otherwise to promote the sale, use or other deal-
ings in this Software without prior written authorization from him.
*/
NOTICE
    # Insert the notice after the end of the first comment (the libxmq license).
    awk -v nf="${ROOT}/notice" 'done == 0 && /^\*\/$/ { print; while ((getline line < nf) > 0) print line; done = 1; next } { print }' ${ROOT}/xmq-in-progress > ${ROOT}/tmp
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
do_part yaml
do_part membuffer
do_part stack
do_part text
do_part lang
do_part utf8
do_part entities
do_part vector
do_part xml_chars
do_part xml_dom
do_part xml_parser
do_part xml_save
do_part xml_html
do_part xml_html_parser
do_part xml_html_save
do_part xml_xpath
do_part xml_uri
do_part xml_unicode
do_part xml_regexp
do_part xml_pattern
do_part xml_xsd10_types
do_part xml_xsd10_schemas
do_part xml_xsd10_parse
do_part xml_xsd10_build
do_part xml_xsd10_valid
do_part xml_libxslt1_xslt
do_part xml_libxslt1_transform
do_part xml
do_part xmq_internals
do_part xmq_parser
do_part xmq_printer
do_part yaep_allocate
do_part yaep_cspc
do_part yaep_hashtab
do_part yaep_objstack
do_part yaep_vlobject
do_part yaep
do_part yaep_structs
do_part yaep_util
do_part yaep_symbols
do_part yaep_terminal_bitset
do_part yaep_print
do_part yaep_tree


do_version
do_upstream_notice
