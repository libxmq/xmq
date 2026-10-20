#!/bin/sh
# libxmq - Copyright 2026 Fredrik Öhrström (spdx: MIT)
# The source name (file name or "stdin") must be visible in the
# parentheses of parse error messages, e.g.
#   badflow.yaml:2:0: error: ...
# and not "(null):2:0: error: ...".
PROG=$1
OUTPUT=$2

if [ -z "$OUTPUT" ] || [ -z "$PROG" ]
then
    echo "Usage: tests/test_special_010_yaml_source_name.sh [XMQ_BINARY] [OUTPUT_DIR]"
    exit 1
fi

mkdir -p $OUTPUT

printf 'a: 1\nb: [\n' > $OUTPUT/badflow.yaml

$PROG $OUTPUT/badflow.yaml > $OUTPUT/file.out 2> $OUTPUT/file.err
[ $? -ne 0 ] || { echo "ERROR: expected a parse failure"; exit 1; }

cat $OUTPUT/file.err | sed "s|$OUTPUT|...|" > $OUTPUT/file.err.norm
cat << 'EOF' > $OUTPUT/file.expected
.../badflow.yaml:3:0: error: unexpected characterYAML: a flow collection is not closed "" U+0

EOF

if ! diff $OUTPUT/file.expected $OUTPUT/file.err.norm
then
    echo "ERROR: yaml file error should show the file name"
    if [ -n "$USE_MELD" ]; then meld $OUTPUT/file.expected $OUTPUT/file.err.norm; else diff $OUTPUT/file.expected $OUTPUT/file.err.norm; fi
    exit 1
fi

printf 'a: 1\nb: [\n' | $PROG --yaml - > $OUTPUT/stdin.out 2> $OUTPUT/stdin.err
grep -q '^stdin:[0-9]*:[0-9]*: error:' $OUTPUT/stdin.err || {
    echo "ERROR: yaml stdin error should show 'stdin:'"
    exit 1
}

echo "OK: test special 010 yaml source name"
