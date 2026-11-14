#!/bin/sh
# libxmq - Copyright 2026 Fredrik Öhrström (spdx: MIT)
#
# Regression tests for crashes found in the xmq cli (see CRASHES.TOFIX).
# Each case checks the exit code (a crash gives rc>=128) and the output.

PROG=$1
OUTPUT=$2

if [ -z "$OUTPUT" ] || [ -z "$PROG" ]
then
    echo "Usage: tests/test_cmd_003_crash_regressions.sh [XMQ_BINARY] [OUTPUT_DIR]"
    exit 1
fi

OUTPUT=$OUTPUT/crash_regressions
mkdir -p $OUTPUT

FAILED=0

# check NAME EXPECTED_RC EXPECTED_STDOUT EXPECTED_STDERR
# Compares $OUTPUT/out, $OUTPUT/err and $RC from the previous invocation.
# Use the string IGNORE to skip comparing stdout or stderr.
check()
{
    NAME=$1
    if [ "$RC" -ge 128 ]
    then
        echo "ERROR: crash regression $NAME: crashed with rc=$RC"
        FAILED=1
        return
    fi
    if [ "$RC" != "$2" ]
    then
        echo "ERROR: crash regression $NAME: expected rc=$2 got rc=$RC"
        cat $OUTPUT/err
        FAILED=1
        return
    fi
    if [ "$3" != "IGNORE" ] && [ "$(cat $OUTPUT/out)" != "$3" ]
    then
        echo "ERROR: crash regression $NAME: expected stdout"
        echo "$3"
        echo "got"
        cat $OUTPUT/out
        FAILED=1
        return
    fi
    if [ "$4" != "IGNORE" ] && [ "$(cat $OUTPUT/err)" != "$4" ]
    then
        echo "ERROR: crash regression $NAME: expected stderr"
        echo "$4"
        echo "got"
        cat $OUTPUT/err
        FAILED=1
        return
    fi
}

echo 'root { a { b = 1 } }' > $OUTPUT/tree.xmq
printf 'newval = 5\n' > $OUTPUT/new.xmq
echo 'hello world' > $OUTPUT/hello.txt
: > $OUTPUT/empty.xmq

# 1. replace with space separated value must give an error, not crash.
$PROG --xmq $OUTPUT/tree.xmq replace '/root/a/b 2' to-text > $OUTPUT/out 2> $OUTPUT/err
RC=$?
check "replace path value" 1 "" "xmq: replace expects xpath=value, got: /root/a/b 2"

# 2. replace with path<file syntax must give an error, not crash.
$PROG --xmq $OUTPUT/tree.xmq replace "/root/a/b<$OUTPUT/new.xmq" to-text > $OUTPUT/out 2> $OUTPUT/err
RC=$?
check "replace path<file" 1 "" "xmq: replace expects xpath=value, got: /root/a/b<$OUTPUT/new.xmq"

# The working path=value form still works.
$PROG --xmq $OUTPUT/tree.xmq replace '/root/a/b=2' to-xmq --compact > $OUTPUT/out 2> $OUTPUT/err
RC=$?
check "replace path=value" 0 "root{a{b=2}}" ""

# 3. quote-c on an xmq document (elements, no top level text) quotes the text content.
echo 'a { b = 1 }' | $PROG --xmq quote-c to-text > $OUTPUT/out 2> $OUTPUT/err
RC=$?
check "quote-c xmq input" 0 '"1"' ""

echo 'r { a = x b = 2 }' | $PROG --xmq quote-c to-text > $OUTPUT/out 2> $OUTPUT/err
RC=$?
check "quote-c xmq input multiple" 0 '"x2"' ""

# quote-c on empty text input gives an empty quoted string.
$PROG --text $OUTPUT/empty.xmq quote-c to-text > $OUTPUT/out 2> $OUTPUT/err
RC=$?
check "quote-c empty input" 0 '""' ""

# 4. unquote-c on input detected as xmq (no top level text) must not crash.
$PROG $OUTPUT/hello.txt unquote-c to-text > $OUTPUT/out 2> $OUTPUT/err
RC=$?
check "unquote-c xmq input" 0 "" ""

$PROG --text $OUTPUT/hello.txt quote-c to-text > $OUTPUT/out 2> $OUTPUT/err
RC=$?
check "quote-c text input" 0 '"hello world\n"' ""

# 5. replace / replace-entity on an empty document is a no-op.
$PROG --xmq $OUTPUT/empty.xmq replace 'a=1' to-text > $OUTPUT/out 2> $OUTPUT/err
RC=$?
check "replace empty doc" 0 "" ""

$PROG --xmq $OUTPUT/empty.xmq replace-entity E to-text > $OUTPUT/out 2> $OUTPUT/err
RC=$?
check "replace-entity empty doc" 0 "" ""

$PROG --xmq $OUTPUT/empty.xmq replace-entity E 'x' to-text > $OUTPUT/out 2> $OUTPUT/err
RC=$?
check "replace-entity with value empty doc" 0 "" ""

# 6. load without argument, stdin already consumed, must report an error, not crash.
$PROG load < /dev/null > $OUTPUT/out 2> $OUTPUT/err
RC=$?
check "load no argument" 1 IGNORE IGNORE
if ! grep -q "Could not read stdin" $OUTPUT/err
then
    echo "ERROR: crash regression load no argument: missing error message"
    cat $OUTPUT/err
    FAILED=1
fi

# An entity whose replacement text refers to another entity: the children of an
# entity reference is its declaration, recursing into it looped until the stack
# overflowed (fixup_json, fixup_html and the clines printer).
cat > $OUTPUT/entcycle.xml <<'EOF2'
<!DOCTYPE goo [<!ENTITY copy "&#169;">
               <!ENTITY prutt "PRUTT&copy;"> ]>
<goo x="&prutt;alfa">X&copy;Y</goo>
EOF2
for cmd in to-json to-clines to-xml to-html to-yaml
do
    $PROG $OUTPUT/entcycle.xml $cmd > $OUTPUT/out 2> $OUTPUT/err ; RC=$?
    check "entity cycle $cmd" 0 IGNORE IGNORE
done
$PROG $OUTPUT/entcycle.xml to-json > $OUTPUT/out 2> $OUTPUT/err ; RC=$?
check "entity cycle to-json output" 0 '{"!DOCTYPE":"goo [\n<!ENTITY copy \"&#169;\">\n<!ENTITY prutt \"PRUTT&copy;\">\n]","_":"goo","_x":"PRUTT©alfa","|":"X"}' ''

# An attribute that is only an undefined entity reference: xmlNodeListGetString
# returns NULL, which crashed the json and clines printers.
echo 'alfa(c = &nbsp;) { x = 1 }' > $OUTPUT/undefent.xmq
$PROG $OUTPUT/undefent.xmq to-json > $OUTPUT/out 2> $OUTPUT/err ; RC=$?
check "undefined entity attribute to-json" 0 '{"_":"alfa","_c":"","x":1}' ''
$PROG $OUTPUT/undefent.xmq to-clines > $OUTPUT/out 2> $OUTPUT/err ; RC=$?
check "undefined entity attribute to-clines" 0 '/alfa/@c=""
/alfa/x="1"' ''

if [ "$FAILED" = "0" ]
then
    echo "OK: test cmd 003 crash regressions"
else
    exit 1
fi
