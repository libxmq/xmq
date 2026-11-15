#!/bin/bash
# libxmq - Copyright 2026 Fredrik Öhrström (spdx: MIT)
#
# The expand-entities command. Run by both the C (make test) and the Java (make testj) test suites.

PROG=$1
OUTPUT=$2/test_cmd_006

if [ -z "$2" ] || [ -z "$PROG" ]
then
    echo "Usage: tests/test_cmd_006_expand_entities.sh [XMQ_BINARY] [OUTPUT_DIR]"
    exit 1
fi

mkdir -p $OUTPUT

FAILED=0

# check NAME EXPECTED_RC EXPECTED_OUTPUT ARGS...
check()
{
    NAME=$1
    EXPECTED_RC=$2
    EXPECTED=$3
    shift 3
    GOT=$($PROG "$@" 2>&1)
    RC=$?
    if [ "$RC" != "$EXPECTED_RC" ] || [ "$GOT" != "$EXPECTED" ]
    then
        echo "ERROR: test cmd 006 expand-entities $NAME (rc=$RC expected $EXPECTED_RC)"
        diff <(echo "$EXPECTED") <(echo "$GOT")
        FAILED=1
    fi
}

# Nested entities, an entity with elements and an entity in an attribute value,
# whose whitespace is normalized. Character references are kept as characters.
cat > $OUTPUT/doc.xml <<'XML'
<!DOCTYPE r [
<!ENTITY foo "xyz">
<!ENTITY el "<b x='&foo;'>in&foo;</b>tail">
<!ENTITY at "a&#10;b&foo;">
]>
<r a="1&at;2" c="&#65;">Hello World!A&foo;B&el;&#66;</r>
XML

check "before expansion" 0 '<r a="1&at;2" c="A">Hello World!A&foo;B&el;B</r>' \
      $OUTPUT/doc.xml select /r to-xml -o
check "all" 0 '<r a="1a bxyz2" c="A">Hello World!AxyzB<b x="xyz">inxyz</b>tailB</r>' \
      $OUTPUT/doc.xml expand-entities select /r to-xml -o
check "xpath sees the expansion" 0 "'1'" \
      $OUTPUT/doc.xml expand-entities select 'string(/r) = "Hello World!AxyzBinxyztailB" and count(//b) = 1'
check "xpath before expansion" 0 "'1'" \
      $OUTPUT/doc.xml select 'string(/r) = "Hello World!ABB" and count(//b) = 0'
check "xmq" 0 "r(a = '1a bxyz2'
  c = A)
{
    'Hello World!AxyzB'
    b(x = xyz) = inxyz
    'tailB'
}" $OUTPUT/doc.xml expand-entities select /r

# Namespaces inside the replacement text are those in scope at the reference.
cat > $OUTPUT/ns.xml <<'XML'
<!DOCTYPE r [
<!ENTITY e "<q:x/>">
]>
<r xmlns:q="urn:q"><a xmlns:q="urn:other">&e;</a>&e;</r>
XML
check "namespaces" 0 "'1'" \
      $OUTPUT/ns.xml expand-entities select 'namespace-uri(/r/a/*) = "urn:other" and namespace-uri(/r/*[2]) = "urn:q"'

# Undeclared entities in xmq are kept.
check "undeclared kept" 0 'a{b=&x;}' -i 'a{b=&x;}' expand-entities to-xmq --compact

# Errors.
cat > $OUTPUT/loop.xml <<'XML'
<!DOCTYPE r [
<!ENTITY a "x&b;">
<!ENTITY b "y&a;">
]>
<r>&a;</r>
XML
check "loop" 1 'xmq: Detected an entity reference loop' $OUTPUT/loop.xml expand-entities

cat > $OUTPUT/unbalanced.xml <<'XML'
<!DOCTYPE r [
<!ENTITY a "<x>">
]>
<r>&a;</r>
XML
check "unbalanced" 1 'xmq: Premature end of data in tag x line 1' $OUTPUT/unbalanced.xml expand-entities

{
    echo '<!DOCTYPE lolz ['
    echo '<!ENTITY lol "lol">'
    prev=lol
    for n in 1 2 3 4 5 6 7 8 9
    do
        printf '<!ENTITY lol%s "' $n
        for k in 1 2 3 4 5 6 7 8 9 10; do printf '&%s;' $prev; done
        echo '">'
        prev=lol$n
    done
    echo ']>'
    echo '<lolz>&lol9;</lolz>'
} > $OUTPUT/lol.xml
check "billion laughs" 1 'xmq: Maximum entity amplification factor exceeded, see xmlCtxtSetMaxAmplification.' \
      $OUTPUT/lol.xml expand-entities

if [ "$FAILED" = "0" ]
then
    echo "OK: test cmd 006 expand-entities"
else
    exit 1
fi
