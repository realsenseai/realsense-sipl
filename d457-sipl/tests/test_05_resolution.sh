#!/usr/bin/env bash
# Test 5 — resolution change WITHOUT recompiling the query. Installs the env-driven depth query ONCE
# (gen_query), then streams depth at several resolutions for ${SECS}s each by changing ONLY the
# D457_WIDTH/HEIGHT/FPS env vars, asserting each captures clean frames with ZERO drops and ZERO faults.
#
# Validates the data-driven resolution path end-to-end: env -> query JSON (token replace) -> SIPL
# pipeline sizing + parametric DS5 mode (D457Sensor BuildModeTable). The query .so is built/installed
# once; resolution changes are recompile-free. SIPL owns the PoC power-cycle (powerControlInfo), so no
# manual i2cset is needed between runs.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
. "$HERE/lib/common.sh"

SECS="${SECS:-5}"
# Each entry is WIDTHxHEIGHTxFPS. Override e.g. RES_LIST="1280x720x30 640x480x30". Defaults vary
# width, height AND fps; 848x480 has no legacy per-resolution table — it works only via the parametric
# DS5 mode builder, so it doubles as proof that arbitrary DS5-supported modes need no code change.
RES_LIST="${RES_LIST:-1280x720x30 848x480x30 640x480x30 640x480x60}"

echo "==== Test 5: depth resolution change (recompile-free), ${SECS}s each ===="
echo "     resolutions: $RES_LIST"
gen_query depth || { fail "query build/install failed"; summary; exit 1; }   # env-driven query, ONCE
for res in $RES_LIST; do
    IFS='x' read -r W H F <<< "$res"
    run_res depth "$W" "$H" "$F" "$SECS"
done
summary
