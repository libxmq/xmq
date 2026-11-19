#!/bin/bash
# libxmq - Copyright 2026 Fredrik Öhrström (spdx: MIT)
#
# Java that library/lang/java.ixml must reject. The inputs that must be accepted are
# library/lang/test-java/*.inp (tests/test_library.sh), these are the ones that javac
# rejects too and that the grammar once accepted, or easily could.
#
# A case is a context and the code: F a whole file, C the body of a class, M the body
# of a method.
#
# Usage: tests/test_java_rejects.sh <xmq binary> <output dir>

PROG=$1
OUTPUT=$2/java_rejects
GRAMMAR=library/lang/java.ixml

mkdir -p "$OUTPUT"

passed=0
failed=0

reject()
{
    local context=$1
    local code=$2
    case "$context" in
        F) printf '%s' "$code" > "$OUTPUT/case.java" ;;
        C) printf 'class A { %s }' "$code" > "$OUTPUT/case.java" ;;
        M) printf 'class A { void f() { %s } }' "$code" > "$OUTPUT/case.java" ;;
    esac
    if $PROG "$GRAMMAR" "$OUTPUT/case.java" to-xml 2> /dev/null | grep -q 'ixml:state="failed"'
    then
        passed=$((passed+1))
    else
        failed=$((failed+1))
        echo "ERR: java.ixml accepts ($context) $code"
    fi
}

# Tokens
reject F 'class A { int x = 1_; }'
reject F 'class A { int x = 0x; }'
reject F 'class A { int x = 1_000_; }'
reject F 'class A { char c = '"'ab'"'; }'
reject F 'class A { String s = """abc"""; }'
reject F 'class A { String s = "abc; }'
reject F 'class A { int x = 0b2; }'
reject F 'class A { double d = 1e; }'
reject F 'class A { } /* open'
reject F 'class A { int x = 1.2.3; }'
reject F 'class A { double d = 0x1.8; }'

# Declarations
reject C 'record r;'
reject C 'var v;'
reject F 'class { }'
reject C 'int x'
reject C 'int x = ;'
reject C 'void f()'
reject F 'import a.b'
reject F 'class A extends B, C { }'
reject F 'class A { int f() { }'
reject F 'class A<> { }'
reject F 'enum E { A B }'
reject C '@ int x;'
reject F 'class A { int x; } class'

# Types
reject C 'List<int,> x;'
reject C 'void m(int x, A this) { }'
reject C 'void m(A this, A this) { }'
reject C 'A.this x;'
reject C 'List<? extends> x;'
reject C 'int[ x;'
reject C 'a.b. c;'
reject C 'void m(String... a, int b) { }'
reject F 'record R(int... a, int b) { }'

# Statements
reject M 'if (a) x()'
reject M 'if a x();'
reject M 'else x();'
reject M 'for (int i = 0; i < 3) x();'
reject M 'try { }'
reject M 'try { } catch (E) { }'
reject M 'switch (a) { foo(); }'
reject M 'switch (a) { case 1 -> x(); case 2: y(); }'
reject M 'do x(); while (a)'
reject M 'return x'
reject M 'if (a) int x = 1;'
reject M 'break 1;'
reject M 'case 1: x();'
reject M 'static int x;'
reject M 'public int x;'
reject C 'void f(static int x) { }'
reject M 'sealed s;'
reject M 'permits p;'

# Expressions
reject M 'a + b;'
reject M 'x = ;'
reject M 'x = a +;'
reject M 'x = (a;'
reject M 'x = a ? b;'
reject M 'x = new;'
reject M 'x = new int[];'
reject M 'f(,);'
reject M 'x = a..b;'
reject M 'x = (int) ;'
reject M 'x = a b;'
reject M 'x = a instanceof;'
reject M 'x = ->a;'
reject M 'x = int;'
reject M 'x = a::;'
reject M 'x = (a) ++b;'
reject M 'x = a--b;'

echo "Java rejects passed: $passed failed: $failed"
[ "$failed" = "0" ] && echo "OK: java rejects"
[ "$failed" = "0" ]
