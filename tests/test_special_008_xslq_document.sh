#!/bin/sh
# libxmq - Copyright 2026 Fredrik Öhrström (spdx: MIT)

PROG=$1
OUTPUT=$2
TEST_NAME="Test special 008 xsl transform documt('')"

if [ -z "$OUTPUT" ] || [ -z "$PROG" ]
then
    echo "Usage: tests/test_special....sh [XMQ_BINARY] [OUTPUT_DIR]"
    exit 1
fi

mkdir -p $OUTPUT

# note the annoying escape of $ below, for \$unit and \$hrunit
cat <<EOF > $OUTPUT/foo.xslq
xsl:stylesheet(version   = 1.0
               xmlns:xsl = http://www.w3.org/1999/XSL/Transform
               xmlns:my  = urn:my)
{
    my:tohrunit {
        entry(key = pct) = %
        entry(key = m3) = m³
        entry(key = m3h) = m³/h
        entry(key = c) = °C
        entry(key = dbm) = dBm
        entry(key = h) = h
        entry(key = hca) = hca
        entry(key = kw) = kW
        entry(key = kwh) = kWh
        entry(key = v) = V
        entry(key = y) = years
    }
    xsl:template(match = /)
    {
        xsl:variable(name   = unit
                     select = "'m3'")
        xsl:variable(name   = hrunit
                     select = "document('')/*/my:tohrunit/entry[@key = \$unit]")
        result {
            xsl:value-of(select = \$hrunit)
        }
    }
}
EOF


OUTPUT=$($PROG -z transform $OUTPUT/foo.xslq)
EXPECT="result(xmlns:my = urn:my) = m³"

if [ "$OUTPUT" = "$EXPECT" ]
then
    echo OK: $TEST_NAME
else
    echo ERR: $TEST_NAME
    echo "Got >$OUTPUT< but expected >$EXPECT<"
    exit 1
fi
