#!/bin/sh
# libxmq - Copyright 2025 Fredrik Öhrström (spdx: MIT)

PROG=$1
OUTPUT=$2

if [ -z "$OUTPUT" ] || [ -z "$PROG" ]
then
    echo "Usage: tests/test_samples.sh [XQ_BINARY] [OUTPUT_DIR]"
    exit 1
fi

mkdir -p $OUTPUT/samples_output

$PROG ixml:data/csv tests/samples/plumbing_products.csv xsl:data/table-to-web to-html > $OUTPUT/samples_output/plumbing_products.html

COUNT=$($PROG $OUTPUT/samples_output/plumbing_products.html select 'count(//td)' to-text)

if [ "$COUNT" != "110" ]
then
    echo "COUNT=$COUNT"
    echo "ERROR: Expected 110 td elemens in plumbing_products after ixml->html transform."
    exit 1
fi

echo "OK: sample plumbing_products count td"

LAST=$($PROG $OUTPUT/samples_output/plumbing_products.html select '/html/body/table/tr[last()]/td[last()]' to-text)

if [ "$LAST" != "2025-09-30" ]
then
    echo "LAST=$LAST"
    echo "ERROR: Expected 2025-09-30 as last td element in last tr in plumbing_products after ixml->html transform."
    exit 1
fi

echo "OK: sample plumbing_products last td in last tr"
