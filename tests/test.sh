#!/bin/bash
# libxmq - Copyright 2023-2026 Fredrik Öhrström (spdx: MIT)

#set -e
BUILD=$1
OUTPUT=$2
FILTER=$3

SPEC=$(dirname $1)/spec.inc
if [ ! -f "$SPEC" ]
then
    SPEC=$1/spec.inc
    if [ ! -f "$SPEC" ]
    then
        echo "Cannot find spec.inc file: $SPEC"
        echo "Please run configure."
        exit 1
    fi
fi

. $SPEC

PROG=$1/xmq
LIB=$1/libxmq.so

if [ -n "$USE_VALGRIND" ]
then
    export PROG="$USE_VALGRIND $PROG"
fi

if [ -z "$OUTPUT" ] || [ -z "$PROG" ]
then
    echo "Usage: test.sh build test_output"
    exit 1
fi

rm -rf "$OUTPUT"
mkdir -p "$OUTPUT"

# The optional parts compiled into xmq (see xmq --features and the XMQ_NO_... defines in
# xmq.h). Tests that need an excluded part are skipped.
FEATURES=" $($PROG --features 2> /dev/null) "
has_feature()
{
    case "$FEATURES" in *" $1 "*) return 0 ;; esac
    return 1
}
if ! has_feature xmq-printing
then
    echo "SKIP: all tests, xmq printing is excluded (XMQ_NO_XMQ_PRINTING) and the tests print xmq"
    exit 0
fi
FULL=true
for f in json yaml xslt xsd ixml html; do has_feature $f || FULL=false; done
# needs_features file: false (and a SKIP line) when the test file uses an excluded part.
needs_features()
{
    # Match the file name and the content.
    uses() { echo "$1" | grep -q -i -E "$2" || grep -q -i -E "$2" "$1"; }
    # json is also recognized by its content: a line starting with "key":
    if ! has_feature json && uses "$1" 'json|^[[:space:]]*"[^"]*"[[:space:]]*:'; then echo "SKIP: $1 (needs json)"; return 1; fi
    if ! has_feature yaml && uses "$1" 'yaml|\.yml'; then echo "SKIP: $1 (needs yaml)"; return 1; fi
    if ! has_feature xslt && uses "$1" 'transform|xsl'; then echo "SKIP: $1 (needs xslt)"; return 1; fi
    if ! has_feature xsd && uses "$1" 'validate|xsd|XMLSchema'; then echo "SKIP: $1 (needs xsd)"; return 1; fi
    if ! has_feature ixml && uses "$1" 'ixml|^IXML'; then echo "SKIP: $1 (needs ixml)"; return 1; fi
    # html, not htmq or render-html (they are xmq).
    if ! has_feature html && uses "$1" 'to-html|--html|\.html?\b|<html|doctype html|method="html"|method=.html'; then echo "SKIP: $1 (needs html)"; return 1; fi
    return 0
}


for i in tests/theme_???_*.test
do
    if [ -n $FILTER ] && [[ ! "$i" =~ $FILTER ]]; then continue; fi
    needs_features "$i" || continue
    tests/test_theme.sh "$PROG" "$OUTPUT" "$i"
    if [ "$?" != 0 ]; then echo "Testing aborted"; exit 1 ; fi
done

for i in tests/[0-9][0-9][0-9]_*.test
do
    if [ -n $FILTER ] && [[ ! "$i" =~ $FILTER ]]; then continue; fi
    needs_features "$i" || continue
    tests/test_single.sh "$PROG" "$OUTPUT" "$i"
done

#if [ -z $FILTER ]
#then
#    tests/test_library_grammar.sh "$PROG" "$OUTPUT"
#fi

for i in tests/ixml/correct/*.output.xmq
do
    if [ -n $FILTER ] && [[ ! "$i" =~ $FILTER ]]; then continue; fi
    needs_features "$i" || continue
    tests/test_ixml_correct.sh "$PROG" "$OUTPUT" "$i"
    if [ "$?" != 0 ]; then echo "Testing aborted"; exit 1 ; fi
done

for i in tests/pipe_???_*.test
do
    if [ -n $FILTER ] && [[ ! "$i" =~ $FILTER ]]; then continue; fi
    needs_features "$i" || continue
    tests/test_pipes.sh "$PROG" "$OUTPUT" "$i"
    if [ "$?" != 0 ]; then echo "Testing aborted"; exit 1 ; fi
done

for i in tests/error_???_*.test
do
    if [ -n $FILTER ] && [[ ! "$i" =~ $FILTER ]]; then continue; fi
    needs_features "$i" || continue
    tests/test_error.sh "$PROG" "$OUTPUT" "$i"
#    if [ "$?" != 0 ]; then echo "Testing aborted"; exit 1 ; fi
done

for i in tests/warning_???_*.test
do
    if [ -n $FILTER ] && [[ ! "$i" =~ $FILTER ]]; then continue; fi
    needs_features "$i" || continue
    tests/test_warning.sh "$PROG" "$OUTPUT" "$i"
    if [ "$?" != 0 ]; then echo "Testing aborted"; exit 1 ; fi
done

for i in tests/format_???_*.test
do
    if [ -n $FILTER ] && [[ ! "$i" =~ $FILTER ]]; then continue; fi
    needs_features "$i" || continue
    tests/test_formatting.sh "$PROG" "$OUTPUT" "$i"
    if [ "$?" != 0 ]; then echo "Testing aborted"; exit 1 ; fi
done

for i in tests/noinput_???_*.test
do
    if [ -n $FILTER ] && [[ ! "$i" =~ $FILTER ]]; then continue; fi
    needs_features "$i" || continue
    tests/test_noinput.sh "$PROG" "$OUTPUT" "$i"
    if [ "$?" != 0 ]; then echo "Testing aborted"; exit 1 ; fi
done

for i in tests/cmd_???_*.test
do
    if [ -n $FILTER ] && [[ ! "$i" =~ $FILTER ]]; then continue; fi
    needs_features "$i" || continue
    tests/test_cmd.sh "$PROG" "$OUTPUT" "$i"
    if [ "$?" != 0 ]; then echo "Testing aborted"; exit 1 ; fi
done

for i in tests/json_???_*.test
do
    if [ -n $FILTER ] && [[ ! "$i" =~ $FILTER ]]; then continue; fi
    needs_features "$i" || continue
    tests/test_json.sh "$PROG" "$OUTPUT" "$i"
    if [ "$?" != 0 ]; then echo "Testing aborted"; exit 1 ; fi
done

for i in tests/yaml_???_*.test
do
    if [ -n $FILTER ] && [[ ! "$i" =~ $FILTER ]]; then continue; fi
    needs_features "$i" || continue
    tests/test_yaml.sh "$PROG" "$OUTPUT" "$i"
    if [ "$?" != 0 ]; then echo "Testing aborted"; exit 1 ; fi
done

for i in tests/parse_json_???_*.test
do
    if [ -n $FILTER ] && [[ ! "$i" =~ $FILTER ]]; then continue; fi
    needs_features "$i" || continue
    tests/test_parse_json.sh "$PROG" "$OUTPUT" "$i"
    if [ "$?" != 0 ]; then echo "Testing aborted"; exit 1 ; fi
done

for i in tests/backforth_???_*.test
do
    if [ -n $FILTER ] && [[ ! "$i" =~ $FILTER ]]; then continue; fi
    needs_features "$i" || continue
    tests/test_backforth.sh "$PROG" "$OUTPUT" "$i"
    if [ "$?" != 0 ]; then echo "Testing aborted"; exit 1 ; fi
done

for i in tests/select_???_*.test
do
    if [ -n $FILTER ] && [[ ! "$i" =~ $FILTER ]]; then continue; fi
    needs_features "$i" || continue
    tests/test_select.sh "$PROG" "$OUTPUT" "$i"
    if [ "$?" != 0 ]; then echo "Testing aborted"; exit 1 ; fi
done

for i in tests/xslt_???_*.test
do
    if [ -n $FILTER ] && [[ ! "$i" =~ $FILTER ]]; then continue; fi
    needs_features "$i" || continue
    tests/test_xslt.sh "$PROG" "$OUTPUT" "$i"
    if [ "$?" != 0 ]; then echo "Testing aborted"; exit 1 ; fi
done

for i in tests/statistics_???_*.test
do
    if [ -n $FILTER ] && [[ ! "$i" =~ $FILTER ]]; then continue; fi
    needs_features "$i" || continue
    tests/test_statistics.sh "$PROG" "$OUTPUT" "$i"
    if [ "$?" != 0 ]; then echo "Testing aborted"; exit 1 ; fi
done

for i in tests/xsd_???_*.test
do
    if [ -n $FILTER ] && [[ ! "$i" =~ $FILTER ]]; then continue; fi
    needs_features "$i" || continue
    tests/test_xsd.sh "$PROG" "$OUTPUT" "$i"
    if [ "$?" != 0 ]; then echo "Testing aborted"; exit 1 ; fi
done

if [ -z "$XMQ_IXML_ENGINE" ]
then
    # Only try the not extension if we are using the internal ixml parser engine.
    for i in tests/ixml_not_[0-9][0-9][0-9]_*.test
    do
        if [ -n $FILTER ] && [[ ! "$i" =~ $FILTER ]]; then continue; fi
        needs_features "$i" || continue
    needs_features "$i" || continue
        tests/test_ixml_not.sh "$PROG" "$OUTPUT" "$i"
        if [ "$?" != 0 ]; then echo "Testing aborted"; exit 1 ; fi
    done

    # Only try the cost extension (rule =< ...) if we are using the internal ixml parser engine.
    for i in tests/ixml_cost_[0-9][0-9][0-9]_*.test
    do
        if [ -n $FILTER ] && [[ ! "$i" =~ $FILTER ]]; then continue; fi
        needs_features "$i" || continue
    needs_features "$i" || continue
        tests/test_ixml_not.sh "$PROG" "$OUTPUT" "$i"
        if [ "$?" != 0 ]; then echo "Testing aborted"; exit 1 ; fi
    done

    # Left recursive grammars, also run through the generated parsers (test_generate_parser.sh).
    for i in tests/ixml_leftrec_[0-9][0-9][0-9]_*.test
    do
        if [ -n $FILTER ] && [[ ! "$i" =~ $FILTER ]]; then continue; fi
        needs_features "$i" || continue
    needs_features "$i" || continue
        tests/test_ixml_not.sh "$PROG" "$OUTPUT" "$i"
        if [ "$?" != 0 ]; then echo "Testing aborted"; exit 1 ; fi
    done

    # The xmq parsing using ixml should be possible to do with other engines.
    for i in tests/xmqixml_[0-9][0-9][0-9]_*.test
    do
        if [ -n $FILTER ] && [[ ! "$i" =~ $FILTER ]]; then continue; fi
        needs_features "$i" || continue
    needs_features "$i" || continue
        tests/test_xmqixml_format.sh "$PROG" "$OUTPUT" "$i"
        if [ "$?" != 0 ]; then echo "Testing aborted"; exit 1 ; fi
    done
fi

for i in tests/ixml_grammar_[0-9][0-9][0-9]_*.test
do
    if [ -n $FILTER ] && [[ ! "$i" =~ $FILTER ]]; then continue; fi
    needs_features "$i" || continue
    tests/test_ixml_grammar.sh "$PROG" "$OUTPUT" "$i"
    if [ "$?" != 0 ]; then echo "Testing aborted"; exit 1 ; fi
done

for i in tests/ixml_parse_[0-9][0-9][0-9]_*.test
do
    if [ -n $FILTER ] && [[ ! "$i" =~ $FILTER ]]; then continue; fi
    needs_features "$i" || continue
    tests/test_ixml_parse.sh "$PROG" "$OUTPUT" "$i"
    if [ "$?" != 0 ]; then echo "Testing aborted"; exit 1 ; fi
done

if [ -z $FILTER ] && [ "$FULL" = "true" ]
then
    tests/test_library.sh "$PROG" "$OUTPUT"
    if [ "$?" != 0 ]; then echo "Testing aborted"; exit 1 ; fi
fi

if has_feature ixml
then
    tests/test_ixml_engine.sh "$PROG" "$OUTPUT"
else
    echo "SKIP: tests/test_ixml_engine.sh (needs ixml)"
fi

if has_feature ixml && has_feature xslt
then
    tests/test_traceixml.sh "$PROG" "$OUTPUT"
else
    echo "SKIP: tests/test_traceixml.sh (needs ixml and xslt)"
fi

for i in tests/test_special_???_*.sh
do
    if [ -n $FILTER ] && [[ ! "$i" =~ $FILTER ]]; then continue; fi
    needs_features "$i" || continue
    $i "$PROG" "$OUTPUT"
    if [ "$?" != 0 ]; then echo "Testing aborted"; exit 1 ; fi
done

for i in tests/test_cmd_???_*.sh
do
    if [ -n $FILTER ] && [[ ! "$i" =~ $FILTER ]]; then continue; fi
    needs_features "$i" || continue
    $i "$PROG" "$OUTPUT"
    if [ "$?" != 0 ]; then echo "Testing aborted"; exit 1 ; fi
done

for i in tests/test_???_*.c
do
    if [ -n $FILTER ] && [[ ! "$i" =~ $FILTER ]]; then continue; fi
    needs_features "$i" || continue
    tests/test_program.sh "$LIB" "$OUTPUT" "$i"
    if [ "$?" != 0 ]; then echo "Testing aborted"; exit 1 ; fi
done

if [ -n "$FILTER" ]; then exit 0; fi
if [ "$FULL" != "true" ]
then
    echo "SKIP: dist, tex, samples and cli grammar tests, parts are excluded:$FEATURES"
    exit 0
fi

if [ "$CONF_MNEMONIC" = "linux64" ]
then
    tests/test_dist.sh $SPEC
fi

if command -v xelatex > /dev/null
then
    tests/test_tex.sh "$PROG" "$OUTPUT"
fi

tests/test_samples.sh "$PROG" "$OUTPUT"

# The xmq command line grammar xmqcli.ixml and the argv joiners.
if [ -z $FILTER ] || [[ "cli_grammar" =~ $FILTER ]]
then
    tests/test_cli_grammar.sh "$PROG" "$OUTPUT/cli_grammar"
    if [ "$?" != 0 ]; then echo "Testing aborted"; exit 1 ; fi
fi
