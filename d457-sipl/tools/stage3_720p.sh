#!/bin/bash
# stage3_720p.sh [secs] — Stage 3: 4 cameras x (depth+RGB) = 8 streams @ 720p30 over 4 MIPI lanes.
#
# Deploys the whole Stage-3 profile then runs and asserts all 8 sensors. Three build-time pieces
# must agree (§4):
#   deser 8-pipe map   D457_MAP_LINKS=4 D457_MAP_STREAMS=2   (+ the PyHSL hslc-cache force)
#   query              d457_query_4cam.cpp, enableMasks 0x1111, depth+rgb, 720p30 defaults
#   nvsipl_camera      PATCHED build (per-link VC + i2c offset) -- the stock /usr/sbin one CANNOT
#                      drive links 1-3 (identical VCs/i2c collide at the Tegra VI)
set -u
export RIG_SUDO_PW="${RIG_SUDO_PW:-}"   # set to the rig sudo password for non-interactive sudo
echo "$RIG_SUDO_PW" | sudo -S -v 2>/dev/null
SECS="${1:-15}"
S=~/sipl_full/usr/src/jetson_sipl_api/sipl
APP=$S/samples/camera/build_dbg/nvsipl_camera
LOG=/tmp/live/run.log

echo "######## STAGE 3: 4 cams x depth+rgb = 8 streams @ 1280x720@30, 4 lanes ########"

# --- 1. deser: 8-pipe map (4 links x 2 streams). Cache force is mandatory: make regenerates the
#        PyHSL only on .py mtime change, NOT on D457_MAP_* change -> stale map = S2,3,6,7 dead.
rm -f "$S/build/uddf/drivers/deserializers/MAX96712/hsl_gen/MAX967XXHsl.hslc"
touch "$S/uddf/drivers/deserializers/MAX967XX/MAX967XXHsl.py"
cd "$S" || exit 1
echo ">> building deser (D457_MAP_LINKS=4 D457_MAP_STREAMS=2)"
if ! D457_MAP_LINKS=4 D457_MAP_STREAMS=2 D457_DPHY_RATE_100M=${D457_DPHY_RATE_100M:-25} ~/build_deploy.sh -c driver,deser > /tmp/s3_build.log 2>&1; then
    echo "BUILD FAILED:"; tail -25 /tmp/s3_build.log; exit 1
fi
SEQ=$(awk '/ set_mipi_d_phy \{/,/"set_mipi_d_phy"\};/' \
      "$S/build/uddf/drivers/deserializers/MAX96712/hsl_gen/MAX967XXHsl.hpp" | tr -d ' \n')
echo "   lane cfg: 0x094A -> $(echo "$SEQ" | grep -o '0x09U,0x4aU,0x..U') | 0x0418 -> $(echo "$SEQ" | grep -o '0x04U,0x18U,0x..U')"

# --- 2. query: 4-cam, mask 0x1111 ---
echo ">> building + installing the 4-cam query"
g++ -shared -fPIC -O2 -o /tmp/libnvsipl_qry_d457.so ~/d457_query_4cam.cpp || exit 1
sudo cp /tmp/libnvsipl_qry_d457.so /usr/lib/nvsipl_drv/libnvsipl_qry_d457.so || exit 1
echo -n "   query: "; strings /usr/lib/nvsipl_drv/libnvsipl_qry_d457.so \
    | grep -oE '"enableMasks": \[ "[^"]*"|"dphyRate": [0-9]*|"lanes": [0-9]*' | tr '\n' ' '; echo

# --- 3. patched sample app (rebuild from the clean backup so it always matches main.cpp) ---
echo ">> re-patching + rebuilding nvsipl_camera (per-link VC + i2c offset)"
bash ~/repatch2_nvsipl_main.sh 2>&1 | tail -3

# --- 4. run ---
mountpoint -q /tmp/live 2>/dev/null || { sudo mkdir -p /tmp/live; sudo mount -t tmpfs -o size=64M tmpfs /tmp/live; }
sudo rm -f "$LOG"
echo ">> DS5 PoC power-cycle"
sudo i2cset -y 9 0x28 0x01 0x00 2>/dev/null; sleep 3; sudo i2cset -y 9 0x28 0x01 0x1f 2>/dev/null; sleep 2
DMARK="STAGE3_${RANDOM}_${RANDOM}"; sudo sh -c "echo $DMARK > /dev/kmsg" 2>/dev/null
echo ">> running ${SECS}s ..."
    # shellcheck disable=SC2024  # the redirect is intentionally the invoking user: the log lives on a
    # user-writable tmpfs, and piping nvsipl_camera through `sudo tee` would put an extra process in
    # a path that can emit tens of MB/s on failure.
sudo env D457_STREAMS="depth,rgb" D457_WIDTH=1280 D457_HEIGHT=720 D457_FPS=30.0 \
    timeout $((SECS + 25)) "$APP" -H -R -0 -1 -2 -m 0x1111 -c D457_Camera -r "$SECS" -s \
    > "$LOG" 2>&1
sudo pkill -9 -x nvsipl_camera 2>/dev/null; sleep 1

# --- 5. assert all 8 ---
MINF=$(( SECS * 25 ))
OK=1
echo "   --- per-sensor (need >= ${MINF} frames, 0 drops) ---"
for i in $(seq 0 7); do
    LINE=$(grep -E "Sensor ${i}[[:space:]]+Frame captured:" "$LOG" | tail -1)
    FC=$(echo "$LINE" | grep -oE 'Frame captured: [0-9]+' | grep -oE '[0-9]+$')
    FD=$(echo "$LINE" | grep -oE 'Frame drops: [0-9]+'    | grep -oE '[0-9]+$')
    CAM=$((i / 2)); ST=$([ $((i % 2)) -eq 0 ] && echo depth || echo rgb)
    if [ -z "${FC:-}" ]; then
        echo "   S$i (cam$CAM $ST): NO FRAMES"; OK=0
    else
        # Use the APP's own steady-state fps, not frames/SECS. Links come up staggered over ~13 s,
        # so frames/SECS is meaningless (it reported 54 fps for a 30 fps sensor). Skip each sensor's
        # FIRST sample too -- it divides the start-up backlog by a partial interval (230 fps).
        FPS=$(grep -E "Sensor${i}_Out0.*Frame rate" "$LOG" | awk '{print $NF}' | tail -n +2 \
              | sort -n | awk '{v[NR]=$1} END{if(NR)printf "%.2f", v[int((NR+1)/2)]; else printf "n/a"}')
        MARK="ok "; { [ "$FC" -lt "$MINF" ] || [ "${FD:-1}" != "0" ]; } && { MARK="BAD"; OK=0; }
        echo "   S$i (cam$CAM $ST): $MARK frames=$FC  steady fps=${FPS}  drops=${FD:-?}"
    fi
done
FAULTS=$(sudo dmesg 2>/dev/null | sed -n "/$DMARK/,\$p" | grep -icE 'chansel|pix_short|fault')
echo "   Tegra-VI faults since run start: $FAULTS"
[ "${FAULTS:-0}" != "0" ] && OK=0
sudo dmesg 2>/dev/null | grep -E 'Partition: CIL|Physical rate' | tail -2 | sed 's/^/   /'
if [ "$OK" = "1" ]; then echo "   ✅ STAGE 3 @720p30 PASS (8/8)"; else
    echo "   ❌ STAGE 3 @720p30 FAIL"; grep -iE 'error|fail|NOTIF_ERROR' "$LOG" | head -6 | sed 's/^/      /'
fi
exit $((1 - OK))
