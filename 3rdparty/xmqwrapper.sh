#!/bin/sh
# xmqwrapper.sh — adapt xmq to the iXML test-harness calling convention
# used by the script generated from tests/tools/testcat-to-bash.xsl:
#
#     CMD <input> <grammar> <output>
#
# i.e. argument 1 = input file, 2 = iXML grammar file, 3 = where to write
# the result (as XML, matching the harness's result.*.xml naming).
if [ -z "$1" ] || [ -z "$2" ] || [ -z "$3" ]
then
    echo "Usage: xmqwrapper.sh [input] [grammar] [output]" >&2
    exit 1
fi

relative_path="../build/default/release/xmq"
absolute_path="$(realpath -- "$relative_path")"

XMQ="$absolute_path"

# Parse the input with the iXML grammar (xmq: <input>, grammar via --ixml),
# then convert the XMQ tree to XML for the harness's result file.
"$XMQ" --ixml="$2" "$1" to-xml > "$3"
