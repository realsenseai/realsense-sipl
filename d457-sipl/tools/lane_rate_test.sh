#!/bin/bash
# lane_rate_test.sh <rate_100Mbps> [secs] — set the deser->Tegra D-PHY lane rate on BOTH sides
# (MAX96712 BACKTOP25 + SIPL query dphyRate), rebuild/install, and run a depth capture.
#
#   ./lane_rate_test.sh 15      -> 1500 Mbps/lane, 10 s depth run
#   ./lane_rate_test.sh 25 15   -> 2500 Mbps/lane (MAX96712 max), 15 s
#
# BACKTOP25 bits[4:0] hold the rate in units of 100 Mbps, so <rate_100Mbps> is both the register
# field and (x100000) the query's dphyRate in kbps. The two MUST match.
set -u
export RIG_SUDO_PW=mic-742
echo "$RIG_SUDO_PW" | sudo -S -v 2>/dev/null
R100="${1:?usage: lane_rate_test.sh <rate_100Mbps> [secs]}"
SECS="${2:-10}"
KBPS=$((R100 * 100000))
MBPS=$((R100 * 100))
S=~/sipl_full/usr/src/jetson_sipl_api/sipl
HPP=$S/build/uddf/drivers/deserializers/MAX96712/hsl_gen/MAX967XXHsl.hpp

echo "############ LANE RATE ${MBPS} Mbps/lane (dphyRate=${KBPS} kbps, BACKTOP25=0x$(printf %02x $((0x20 | R100)))) ############"

# --- deser: force the PyHSL cache to regenerate, then rebuild + install ---
rm -f "$S/build/uddf/drivers/deserializers/MAX96712/hsl_gen/MAX967XXHsl.hslc"
touch "$S/uddf/drivers/deserializers/MAX967XX/MAX967XXHsl.py"
cd "$S" || exit 1
if ! RIG_SUDO_PW=mic-742 D457_MAP_LINKS=2 D457_MAP_STREAMS=3 D457_DPHY_RATE_100M="$R100" \
        ~/build_deploy.sh -c deser > /tmp/lane_build.log 2>&1; then
    echo "BUILD FAILED:"; tail -20 /tmp/lane_build.log; exit 1
fi
# Prove the compiled bytecode carries the new BACKTOP25 value. Scope to the set_mipi_d_phy
# sequence ONLY -- 0x0418/0x094A also appear in set_mipi_c_phy and set_mipi_d_phy_4x2, so an
# unscoped grep reports a stale value from another sequence and looks like the build didn't take.
SEQ=$(awk '/ set_mipi_d_phy \{/,/"set_mipi_d_phy"\};/' "$HPP" | tr -d ' \n')
echo "  compiled set_mipi_d_phy: 0x0418 -> $(echo "$SEQ" | grep -o '0x04U,0x18U,0x..U' | head -1) | 0x094A -> $(echo "$SEQ" | grep -o '0x09U,0x4aU,0x..U' | head -1)"

# --- query: same rate on the Tegra side ---
cd ~/d457_tests || exit 1
. lib/common.sh
# MUST be exported, not a one-shot prefix: run_streams calls gen_query AGAIN internally, and a
# per-command env would leave that second generation on the 1100000 default -- which silently
# tests deser=<new rate> against Tegra=1100 and looks like a pass.
export D457_DPHY_RATE=$KBPS
gen_query depth || { echo "QUERY BUILD FAILED"; exit 1; }
echo -n "  installed query: "
strings /usr/lib/nvsipl_drv/libnvsipl_qry_d457.so | grep -o '"dphyRate": [0-9]*' | head -1

# --- run ---
run_streams depth "$SECS"
RC=$?
echo "  --- what the stack ACTUALLY used (must all agree with ${KBPS}) ---"
journalctl --since '-3 min' 2>/dev/null | grep -E 'Setting DPHY|BuildSensorProperty: (lanes|mipiSpeed)' \
    | tail -3 | sed 's/.*: /  SIPL: /'
sudo dmesg 2>/dev/null | grep -E 'Partition: CIL|Physical rate|MIPI clock rate' | tail -3 | sed 's/^/  /'
exit $RC
