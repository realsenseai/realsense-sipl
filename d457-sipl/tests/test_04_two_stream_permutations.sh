#!/usr/bin/env bash
# Test 4 — every 2-stream permutation streams cleanly (10 s each), then stops.
#   pairs: depth+rgb, depth+ir, rgb+ir  (run in a randomized order).
# rgb+ir is the case the old depth-pinned driver could NOT express — it needs D457_STREAMS +
# canonical VCs (rgb VC1, ir VC2, VC0 idle).
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
. "$HERE/lib/common.sh"

SECS="${SECS:-10}"
echo "==== Test 4: all 2-stream permutations (${SECS}s each) ===="

PAIRS=("depth,rgb" "depth,ir" "rgb,ir")
# Randomize order ("start two streams randomly").
PAIRS=($(printf '%s\n' "${PAIRS[@]}" | shuf))

for p in "${PAIRS[@]}"; do
    run_streams "$p" "$SECS" "pair $p"
done
summary
