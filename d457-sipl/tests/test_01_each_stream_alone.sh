#!/usr/bin/env bash
# Test 1 — each stream can start ALONE (single-VC capture), 10 s, 0 drops.
#   depth (VC0), rgb (VC1), ir (VC2) — one at a time.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
. "$HERE/lib/common.sh"

SECS="${SECS:-10}"
echo "==== Test 1: each stream alone (${SECS}s each) ===="
run_streams "depth" "$SECS" "depth alone (VC0)"
run_streams "rgb"   "$SECS" "rgb alone (VC1)"
run_streams "ir"    "$SECS" "ir alone (VC2)"
summary
