#!/bin/bash
# libxmq - Copyright 2026 Fredrik Öhrström (spdx: MIT)
#
# Test ixml grammars in xml form: xmq --ixml=grammar.xml (or .xmq .json). The grammar in
# xml form is the parse tree of the grammar with the ixml grammar. Every grammar below is
# turned into its xml form with library/core/ixml.ixml, and from that into xmq and json.
# Each form must give the same yaep grammar (the grammar of xmq --traceixml) and the same
# result as the ixml text.
# Usage: tests/test_ixml_grammar_forms.sh <xmq binary> <output dir>

PROG=$1
OUTPUT=$2/ixml_grammar_forms
mkdir -p "$OUTPUT"

fail()
{
    echo "ERR: ixml grammar forms $1"
    exit 1
}

# check NAME 'input' with the grammar on stdin
check()
{
    local name=$1 input=$2
    local d="$OUTPUT/$name"
    mkdir -p "$d"
    cat > "$d/g.ixml"
    # The xml form: the grammar parsed with the ixml grammar.
    "$PROG" --ixml=library/core/ixml.ixml "$d/g.ixml" to-xml > "$d/g.xml" || fail "$name: no xml form"
    "$PROG" "$d/g.xml" to-xmq > "$d/g.xmq" || fail "$name: no xmq form"
    "$PROG" "$d/g.xml" to-json > "$d/g.json" || fail "$name: no json form"

    rm -f "$d/trace.ixml.xmq"
    "$PROG" --traceixml="$d/trace.ixml.xmq" --ixml="$d/g.ixml" -i "$input" to-xml > "$d/out.ixml.xml" 2>&1
    "$PROG" "$d/trace.ixml.xmq" select /trace/grammar to-xmq > "$d/rules.ixml" || fail "$name: no trace"
    for form in xml xmq json
    do
        rm -f "$d/trace.$form.xmq"
        "$PROG" --traceixml="$d/trace.$form.xmq" --ixml="$d/g.$form" -i "$input" to-xml > "$d/out.$form.xml" 2>&1
        "$PROG" "$d/trace.$form.xmq" select /trace/grammar to-xmq > "$d/rules.$form" 2>&1 || fail "$name: no trace with the $form form"
        diff -q "$d/rules.ixml" "$d/rules.$form" > /dev/null || fail "$name: the $form form gives another yaep grammar, see $d"
        diff -q "$d/out.ixml.xml" "$d/out.$form.xml" > /dev/null || fail "$name: the $form form gives another result, see $d"
    done
}

check simple 'ab' <<'G'
S: "a", b. b: "b".
G

check marks 'abcd' <<'G'
S: @a, ^b, -c, d. a: "a". -b: "b". c: "c". @d: "d".
G

check terminals 'xyit'"'"'ssay "hi"AB' <<'G'
S: -"x", ^"y", "it's", 'say "hi"', #41, -#42.
G

check insertions 'a' <<'G'
S: +"before", "a", +#21, +"q""uote".
G

check charsets 'b7é?1' <<'G'
S: ["a"-"c"; "x"; #30-#39; L], ~["!"; Zs]+, -[Nd]?.
G

check repeats 'aabbc,cd; de' <<'G'
S: a*, b+, c**",", d++(";", " "?), e?. a: "a". b: "b". c: "c". d: "d". e: "e".
G

check groups 'bcxx' <<'G'
S: ("a"; "b", "c"; ), ("x")*.
G

check empty '' <<'G'
S: ; "a"; .
G

check prolog 'a' <<'G'
ixml version "1.0". S: "a".
G

check comments 'ab' <<'G'
{a comment} S: {one} "a" {two}, b. b {three}: "b".
G

check ambiguous 'x' <<'G'
S: a; b. a: "x". b: "x".
G

# The ixml grammar of the library is older than renaming (>), so this one is written by hand.
cat > "$OUTPUT/alias.xml" <<'GRAMMAR'
<ixml>
  <rule name="S"><alt><nonterminal name="a" alias="first"/><nonterminal name="b"/></alt></rule>
  <rule name="a"><alt><literal string="a"/></alt></rule>
  <rule name="b" alias="second"><alt><literal string="b"/></alt></rule>
</ixml>
GRAMMAR
printf 'S: a>first, b. a: "a". b>second: "b".\n' > "$OUTPUT/alias.ixml"
"$PROG" --ixml="$OUTPUT/alias.ixml" -i ab to-xml > "$OUTPUT/alias.ixml.out" || fail "alias: the ixml grammar failed"
"$PROG" --ixml="$OUTPUT/alias.xml" -i ab to-xml > "$OUTPUT/alias.xml.out" || fail "alias: the xml grammar failed"
diff -q "$OUTPUT/alias.ixml.out" "$OUTPUT/alias.xml.out" > /dev/null || fail "alias: the xml form gives another result"

# Not grammars: an unknown element and a hex that is not hex, both exit 1.
printf '<ixml><rule name="S"><alt><gurka/></alt></rule></ixml>\n' > "$OUTPUT/unknown.xml"
"$PROG" --ixml="$OUTPUT/unknown.xml" -i a to-xml > /dev/null 2> "$OUTPUT/unknown.err" && fail "unknown element: exit 0"
grep -q 'unexpected element gurka' "$OUTPUT/unknown.err" || fail "unknown element: no message"
printf '<ixml><rule name="S"><alt><literal hex="CAFFEINE"/></alt></rule></ixml>\n' > "$OUTPUT/badhex.xml"
"$PROG" --ixml="$OUTPUT/badhex.xml" -i a to-xml > /dev/null 2>&1 && fail "bad hex: exit 0"

echo "OK: ixml grammar forms"
