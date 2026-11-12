#!/bin/sh
# Test the xmq library files.
#
# For every library/<category>/test-* directory this script finds, say the
# directory is library/conf/test-nginx, it will use the sibling files:
#
#   library/conf/nginx.ixml      (optional) parse the .inp inputs with this
#                                        ixml grammar
#   library/conf/nginx.xslq      (optional) cleanup transform, the result is
#                                        compared against test_1.xmq etc.
#   library/conf/to-nginx.xslq   (optional) roundtrip transform, back to the
#                                        original format, compared against
#                                        test_1.roundtrip (or test_1.out).
#
# Every test-<name> directory is picked up automatically, so adding a new
# directory to library/ is enough to get it tested.
#
# Usage: tests/test_library.sh [XMQ_BINARY] [OUTPUT_DIR]

PROG=${1:-./build/x86_64-pc-linux-gnu/release/xmq}
OUTPUT=${2:-build/x86_64-pc-linux-gnu/release/test_output}

if [ ! -f "$PROG" ]; then
    echo "Usage: $0 [XMQ_BINARY] [OUTPUT_DIR]"
    exit 1
fi

mkdir -p $OUTPUT
echo "Using xmq: $PROG"

passed=0
failed=0

pass()
{
    passed=$((passed+1));
    echo "pass: $1";
}

fail()
{
    failed=$((failed+1));
    echo "FAIL: $1";
}

# ---------------------------------------------------------------- core checks

# Sanity: the ixml grammar in core can parse itself.
if [ -f library/core/ixml.ixml ]; then
    tmp=$OUTPUT/core_ixml_selfparse
    if $PROG library/core/ixml.ixml library/core/ixml.ixml > $tmp 2>/dev/null; then
        pass "core/ixml.ixml parses itself";
    else
        fail "core/ixml.ixml parses itself";
    fi
fi

# ------------------------------------------------------ library test-<name>

for testdir in library/*/test-* library/test-*
do
    # Skip glob literals that did not match.
    [ -d "$testdir" ] || continue

    tname=`basename "$testdir"`

    case "$tname" in
        test-*) kind=${tname#test-} ;;
        *) echo "SKIP: $tname is not named test-*"; continue ;;
    esac

    catdir=`dirname "$testdir"`
    ixml="$catdir/$kind.ixml"
    xslq="$catdir/$kind.xslq"
    toxslq="$catdir/to-$kind.xslq"
    [ -f "$ixml" ] || ixml=
    [ -f "$xslq" ] || xslq=
    [ -f "$toxslq" ] || toxslq=

    if [ -z "$ixml" ] && [ -z "$xslq" ] && [ -z "$toxslq" ]; then
        echo "SKIP: no $kind.ixml / $kind.xslq / to-$kind.xslq near $testdir";
        continue;
    fi

    for inp in "$testdir"/*.inp
    do
        [ -f "$inp" ] || continue

        stem=`basename ${inp%.inp}`          # example: test_1
        name="$tname.$stem"

        raw=$OUTPUT/$name.raw
        xmqout=$OUTPUT/$name.xmq
        rtout=$OUTPUT/$name.roundtrip

        # Step 1. Parse the input.
        src=$inp
        if [ -n "$ixml" ]; then
            if ! $PROG "$ixml" "$inp" > $raw; then
                fail "$name parse $kind";
                continue;
            fi
            src=$raw
        fi

        # Step 2. Optional cleanup transform, compare the xmq result against
        #          the expected file, <input>.xmq or <input>.out.
        if [ -n "$xslq" ]; then
            if ! $PROG $src transform $xslq > $xmqout; then
                fail "$name xmq $kind";
                continue;
            fi
        else
            cp $src $xmqout
        fi

        exp=${inp%.inp}.xmq
        [ -f "$exp" ] || exp=${inp%.inp}.out
        if [ -f "$exp" ]; then
            if diff -q $xmqout $exp > /dev/null; then
                pass "$name xmq $kind";
            else
                diff $xmqout $exp
                fail "$name xmq $kind";
            fi
        else
            pass "$name xmq $kind (no expected file, smoke test only)";
        fi

        # Step 3. Optional roundtrip transform back to the original format,
        #         compare against <input>.roundtrip (or <input>.out).
        #         If there was no cleanup transform either, there is nothing
        #         new to test here.
        if [ -n "$toxslq" ]; then
            if ! $PROG $xmqout transform $toxslq to-text > $rtout; then
                fail "$name roundtrip $kind";
                continue;
            fi
            exp=${inp%.inp}.roundtrip
            [ -f "$exp" ] || exp=${inp%.inp}.out
            if [ -f "$exp" ]; then
                if diff -q $rtout $exp > /dev/null; then
                    pass "$name roundtrip $kind";
                else
                    diff $rtout $exp
                    fail "$name roundtrip $kind";
                fi
            else
                pass "$name roundtrip $kind (no expected file, smoke test only)";
            fi
        fi
    done
done

echo "====================================="
echo "Passed: $passed Failed: $failed"
[ $failed -eq 0 ]
