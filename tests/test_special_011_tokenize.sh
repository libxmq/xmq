#!/bin/sh
# libxmq - Copyright 2026 Fredrik Öhrström (spdx: MIT)
# tokenize --type=html/tex/terminal/debugtokens/debugcontent, run by both
# make test (xmq) and make testj (xmqj). Regressions:
#  - xmq -i 'content' tokenize tokenized a file named 'content' (C).
#  - an unclosed compound at the end of the input crashed (Java) and is
#    reported where the input ends, like C.
#  - a failed tokenize prints no tokens, only the error (Java).
#  - debugcontent prints compound quotes and trims comments like C (Java).
PROG=$1
OUTPUT=$2

if [ -z "$OUTPUT" ] || [ -z "$PROG" ]
then
    echo "Usage: tests/test_special_011_tokenize.sh [XMQ_BINARY] [OUTPUT_DIR]"
    exit 1
fi

mkdir -p $OUTPUT
O=$OUTPUT/special_011
mkdir -p $O

# A known environment: a dark background is assumed for --type=terminal.
unset XMQ_BG XMQ_THEME COLORFGBG XMQ_TERM
TERM=xterm
export TERM

printf 'a(x=1) {\n    // c<&>_\n    t = (&lt;\047q\\\\b\047)\n}\n' > $O/t.xmq
printf 'alfa{\n/* {\n   }\n  */\n}\n' > $O/c.xmq
printf 'a = (\n' > $O/bad.xmq

{
    for t in html tex terminal
    do
        echo "== $t"
        $PROG $O/t.xmq tokenize --type=$t 2>&1 | cat -v
        echo "[rc=$?]"
    done
    echo "== -i"
    $PROG -i 'a=1' tokenize --type=html 2>&1; echo "[rc=$?]"
    echo "== bad"
    $PROG $O/bad.xmq tokenize --type=debugtokens 2>&1; echo "[rc=$?]"
    echo "== xml"
    $PROG -i '<a/>' tokenize 2>&1; echo "[rc=$?]"
    echo "== debugcontent"
    $PROG $O/t.xmq tokenize --type=debugcontent 2>&1
    $PROG $O/c.xmq tokenize --type=debugcontent 2>&1
} | sed "s|$O/||" > $O/out

cat > $O/expected << 'EOF'
== html
<pre class="xmq xmq_auto"><xmqEN>a</xmqEN>(<xmqAK>x</xmqAK>=<xmqAKV>1</xmqAKV>) {
    <xmqC>// c&lt;&amp;&gt;_
</xmqC>    <xmqEK>t</xmqEK> = <xmqCP>(</xmqCP><xmqE>&amp;lt;</xmqE><xmqEKV>'q\\b'</xmqEKV><xmqCP>)</xmqCP>
}
</pre>[rc=0]
== tex
\texttt{\flushleft\noindent \xmqEN{a}(\xmqAK{x}=\xmqAKV{1}) {
    \xmqC{// c<\&>\_
}    \xmqEK{t} = \xmqCP{(}\xmqE{\&lt;}\xmqEKV{'q\\\\b'}\xmqCP{)}
}

}
[rc=0]
== terminal
^[[0;38;5;208ma^[[0m(^[[0;38;5;111mx^[[0m=^[[0;38;5;69m1^[[0m)^[[0m ^[[0m{^[[0m
    ^[[0;38;5;37m// c<&>_
^[[0m    ^[[0;38;5;111mt^[[0m ^[[0m=^[[0m ^[[0;38;5;134m(^[[0;38;5;134m&lt;^[[0;1;38;5;35m'q\\b'^[[0;38;5;134m)^[[0m
^[[0m}^[[0m
[rc=0]
== -i
<pre class="xmq xmq_auto"><xmqEK>a</xmqEK>=<xmqEKV>1</xmqEKV></pre>[rc=0]
== bad
bad.xmq:2:1: error: compound is not closed
[rc=1]
== xml
xmq: you can only tokenize the xmq format
[rc=1]
== debugcontent
{value "1"}{comment "c<&>_"}{quote "q\\\\b"}
{comment "{\n}"}
EOF

if ! diff $O/expected $O/out > /dev/null
then
    echo "ERROR: test special 011 tokenize"
    if [ -n "$USE_MELD" ]; then meld $O/expected $O/out; else diff $O/expected $O/out; fi
    exit 1
fi

echo "OK: test special 011 tokenize"
