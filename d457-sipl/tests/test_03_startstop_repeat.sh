#!/usr/bin/env bash
# Test 3 — start/stop robustness: start all 3 streams, stream 10 s, stop. Repeat 10 times.
# Each iteration is a full start -> stream -> stop -> exit cycle (run_streams power-cycles the DS5
# and tears the pipeline down each time). Passes only if ALL iterations capture 3 clean streams.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
. "$HERE/lib/common.sh"

SECS="${SECS:-10}"
ITERS="${ITERS:-10}"
echo "==== Test 3: start/stop all 3 streams x${ITERS} (${SECS}s each) ===="
for i in $(seq 1 "$ITERS"); do
    echo "-- iteration $i/$ITERS --"
    run_streams "depth,rgb,ir" "$SECS" "iter $i: depth+rgb+ir"
done
summary
