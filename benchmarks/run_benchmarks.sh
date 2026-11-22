#!/bin/bash
# libxmq - Copyright 2026 Fredrik Öhrström (spdx: MIT)
#
# Run the benchmarks in benchmarks/*/ with the C xmq and the Java xmqj, and append the
# result as a new run to a copy of benchmarks/results.xmq: benchmarks/results_temporary.xmq.
# With SAVE=true the copy then replaces results.xmq, to keep the result.
#
# A benchmark is a directory with:
#   args         the arguments to xmq on one line, @DIR@ is the benchmark directory and
#                @DATA@ the directory with the generated inputs (benchmarks/generate_inputs.sh)
#   description  what it measures
#   and the grammars, stylesheets etc it needs.
#
# Each benchmark is run REPEAT times (default 3) and the best wall clock time, in seconds,
# is stored. The Java time includes the start of the jvm. The Java result is N/A if xmqj
# is not built (make xmqj), fails (not yet implemented) or prints something else than xmq.
# The saved columns printed are the latest results in results.xmq from the same computer.
#
# Usage: make run_benchmarks [SAVE=true] [REPEAT=n]
#   or   benchmarks/run_benchmarks.sh [xmq] [xmqj]

export LC_ALL=C

XMQ=${1:-build/default/release/xmq}
XMQJ=${2:-build/xmqj}
REPEAT=${REPEAT:-3}
DATA=build/benchmarks
OUT=build/benchmarks/output
RESULTS=benchmarks/results.xmq
TEMPORARY=benchmarks/results_temporary.xmq

if [ ! -x "$XMQ" ]
then
    echo "Cannot find the xmq binary $XMQ, run make first."
    exit 1
fi
if [ ! -x "$XMQJ" ]
then
    echo "Cannot find $XMQJ, all Java results are N/A, run make xmqj to benchmark Java."
    XMQJ=
fi

benchmarks/generate_inputs.sh "$DATA" || exit 1
mkdir -p "$OUT"

# Run a command REPEAT times with its output in a file, print the best time in seconds.
# Returns the exit code of the first failing run.
best_time()
{
    local output=$1
    shift
    local best= start stop t
    for ((r = 0; r < REPEAT; r++))
    do
        start=$EPOCHREALTIME
        "$@" > "$output" 2> "$output.err" || return $?
        stop=$EPOCHREALTIME
        t=$(awk -v a="$start" -v b="$stop" 'BEGIN { printf "%.3f", b - a }')
        if [ -z "$best" ] || awk -v a="$t" -v b="$best" 'BEGIN { exit !(a < b) }'
        then
            best=$t
        fi
    done
    echo "$best"
}

# The latest stored result of a benchmark on this computer, eg previous ixml_words c, or nothing.
previous()
{
    [ -f "$RESULTS" ] || return
    "$XMQ" "$RESULTS" select "(/benchmarks/run[host = '$HOST'])[last()]/benchmark[@name = '$1']/$2" to-text 2> /dev/null
}

COMMIT=$(git rev-parse --short HEAD 2> /dev/null || echo unknown)
DIRTY=false
# Uncommitted changes to tracked files, except the stored results themselves.
if [ -n "$(git status --porcelain --untracked-files=no -- . ":(exclude)$RESULTS" 2> /dev/null)" ]; then DIRTY=true; fi
HOST=$(hostname 2> /dev/null || uname -n)
CPU=$(sed -n 's/^model name[[:space:]]*: //p' /proc/cpuinfo 2> /dev/null | head -n 1)
if [ -z "$CPU" ]; then CPU=$(sysctl -n machdep.cpu.brand_string 2> /dev/null || uname -m); fi
DATE=$(date +'%Y-%m-%d %H:%M')

RUN=$(mktemp)
trap 'rm -f "$RUN"' EXIT
{
    echo "    run {"
    echo "        date   = '$DATE'"
    echo "        commit = $COMMIT"
    if [ "$DIRTY" = "true" ]; then echo "        dirty  = true"; fi
    echo "        host   = '$HOST'"
    echo "        cpu    = '$CPU'"
    echo "        repeat = $REPEAT"
} > "$RUN"

printf "%-12s %10s %10s %10s %10s\n" benchmark "C (s)" "saved C" "Java (s)" "saved Java"
FAILED=0
for dir in benchmarks/*/
do
    dir=${dir%/}
    [ -f "$dir/args" ] || continue
    name=$(basename "$dir")
    read -r -a args <<< "$(sed "s|@DIR@|$dir|g; s|@DATA@|$DATA|g" "$dir/args")"

    c=$(best_time "$OUT/$name.c" "$XMQ" "${args[@]}")
    if [ $? != 0 ]
    then
        echo "ERROR: benchmark $name failed with xmq:"
        head -n 5 "$OUT/$name.c.err"
        FAILED=1
        continue
    fi

    java=N/A
    reason=
    if [ -n "$XMQJ" ]
    then
        if ! j=$(best_time "$OUT/$name.java" "$XMQJ" "${args[@]}")
        then
            reason="xmqj failed: $(head -n 1 "$OUT/$name.java.err")"
        elif ! cmp -s "$OUT/$name.c" "$OUT/$name.java"
        then
            reason="xmqj printed something else than xmq, see $OUT/$name.c and $OUT/$name.java"
        else
            java=$j
        fi
    fi

    printf "%-12s %10s %10s %10s %10s\n" "$name" "$c" "$(previous "$name" c)" "$java" "$(previous "$name" java)"
    if [ -n "$reason" ]; then echo "             Java N/A, $reason"; fi
    {
        echo "        benchmark(name = $name) {"
        echo "            c    = $c"
        echo "            java = $java"
        echo "        }"
    } >> "$RUN"
done
echo "    }" >> "$RUN"

if [ "$FAILED" != "0" ]
then
    echo "Not storing the results since a benchmark failed."
    exit 1
fi

# The copy of results.xmq with the run added before the last }.
if [ -f "$RESULTS" ]
then
    sed '$d' "$RESULTS" > "$TEMPORARY"
    if [ "$(tail -n 1 "$RESULTS")" != "}" ]
    then
        echo "ERROR: $RESULTS does not end with a line with only }"
        exit 1
    fi
else
    echo "benchmarks {" > "$TEMPORARY"
fi
cat "$RUN" >> "$TEMPORARY"
echo "}" >> "$TEMPORARY"

if ! "$XMQ" "$TEMPORARY" to-xml > /dev/null
then
    echo "ERROR: $TEMPORARY is not valid xmq"
    exit 1
fi

if [ "$SAVE" = "true" ]
then
    cp "$TEMPORARY" "$RESULTS"
    echo "Saved the results in $RESULTS"
    if [ "$DIRTY" = "true" ]; then echo "Note: the source has uncommitted changes, the run is marked dirty = true."; fi
else
    echo "Wrote $TEMPORARY, run with SAVE=true to store the results in $RESULTS"
fi
