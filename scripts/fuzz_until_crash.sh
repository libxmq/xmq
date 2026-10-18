#!/bin/bash

# Usage: ./scripts/fuzz_until_crash.sh <fuzzer_bin> <input_dir> <output_dir> <target_bin> [target_args...]

FUZZER_BIN=$1
INPUT_DIR=$2
OUTPUT_DIR=$3
shift 3
TARGET_CMD="$@"

if [ -z "$FUZZER_BIN" ] || [ -z "$INPUT_DIR" ] || [ -z "$OUTPUT_DIR" ] || [ -z "$TARGET_CMD" ]; then
    echo "Usage: $0 <fuzzer_bin> <input_dir> <output_dir> <target_bin> [target_args...]"
    exit 1
fi

# Environment variables to bypass AFL++ checks
#export AFL_I_DONT_CARE_ABOUT_MISSING_CRASHES=1
#export AFL_SKIP_CPUFREQ=1

echo "Starting fuzzer: $FUZZER_BIN"
echo "Target: $TARGET_CMD"
echo "Monitoring $OUTPUT_DIR/default/crashes for crashes..."

# Start afl-fuzz in the background
$FUZZER_BIN -i "$INPUT_DIR" -o "$OUTPUT_DIR" -- $TARGET_CMD &
FUZZER_PID=$!

# Wait for a crash to appear
while true; do
    if [ -d "$OUTPUT_DIR/default/crashes" ] && [ "$(ls -A $OUTPUT_DIR/default/crashes | grep -v 'README.txt')" ]; then
        echo "Crash detected! Stopping fuzzer..."
        kill -SIGINT $FUZZER_PID
        break
    fi
    sleep 2
done

wait $FUZZER_PID
echo "Fuzzer stopped after crash detection."
