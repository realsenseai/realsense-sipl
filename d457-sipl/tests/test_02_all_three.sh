#!/usr/bin/env bash
# Test 2 — all three streams TOGETHER (depth VC0 + rgb VC1 + ir VC2), 0 drops.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
. "$HERE/lib/common.sh"

SECS="${SECS:-15}"
echo "==== Test 2: all 3 streams together (${SECS}s) ===="
run_streams "depth,rgb,ir" "$SECS" "depth+rgb+ir"
summary
