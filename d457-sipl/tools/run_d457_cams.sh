#!/usr/bin/env bash
# run_d457_cams.sh — deploy the Stage-2 (2 cam) or Stage-3 (4 cam) multi-camera D457 SIPL profile
# on the rig and launch the Holoscan player (holoscan-sensor-bridge/examples/d457_sipl_player.py) showing every stream.
#
# USAGE (run from your dev box; needs SSH to the rig):
#   ./run_d457_cams.sh 2                  # Stage 2: 2 cams x depth+rgb+ir = 6 tiles @720p30, VNC
#   ./run_d457_cams.sh 4                  # Stage 3: 4 cams x depth+rgb   = 8 tiles @VGA30,  VNC
#   ./run_d457_cams.sh 2 --skip-deploy     # reuse whatever profile is already installed
#   ./run_d457_cams.sh 2 --headless --frame-limit 300   # scripted/headless validation run, no VNC
#
# Deploy = regenerate the query for the requested stage, rebuild driver+deser+query with the right
# D457_MAP_LINKS/D457_MAP_STREAMS (deser pixel map is baked in at BUILD time -- the PyHSL cache-force
# gotcha below), and install. Then rebuild+launch the Holoscan player with --link-mask + (for VGA)
# D457_WIDTH/HEIGHT. See  §4 for the underlying recipe
# this automates, and  M1c for why each step exists.
#
# ENV OVERRIDES: RIG_HOST, RIG_USER, RIG_KEY, RIG_SUDO_PW (needed for the sudo cp install step).
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO="$(cd "$HERE/../.." && pwd)"

RIG_HOST=${RIG_HOST:-fw-advantech-thor-1}
RIG_USER=${RIG_USER:-mic-742}
RIG_KEY=${RIG_KEY:-$HOME/.ssh/claude_fw-advantech-thor-1_ed25519}
RIG_HSB=/home/$RIG_USER/holoscan-sensor-bridge
RIG_SIPL=/home/$RIG_USER/sipl_full/usr/src/jetson_sipl_api/sipl

SSH=(ssh -o BatchMode=yes -i "$RIG_KEY" "$RIG_USER@$RIG_HOST")
SCP=(scp -o BatchMode=yes -i "$RIG_KEY")

say() { echo ">> $*"; }
die() { echo "FATAL: $*" >&2; exit 1; }

STAGE="${1:-}"; shift || true
SKIP_DEPLOY=0
HEADLESS=0
FRAME_LIMIT=""
DEPTH_COLORMAP="jet"
while [ $# -gt 0 ]; do
    case "$1" in
        --skip-deploy) SKIP_DEPLOY=1; shift ;;
        --headless) HEADLESS=1; shift ;;
        --frame-limit) FRAME_LIMIT="$2"; shift 2 ;;
        --depth-colormap) DEPTH_COLORMAP="$2"; shift 2 ;;
        *) echo "unknown arg: $1" >&2; exit 1 ;;
    esac
done

case "$STAGE" in
    2)
        LINKS=2; STREAMS=3; MASK=0x0011; WIDTH=1280; HEIGHT=720
        QUERY_LOCAL="$REPO/d457-sipl/query/d457_query.cpp"
        QUERY_SED='s/"0x0001"/"0x0011"/'   # proven recipe: 3-sensor V0 query, mask 0x0001 -> 0x0011
        VNC_GEOM="1920x720"                # 2 rows x 3 cols, clamped to max-window-width 1920
        ;;
    4)
        LINKS=4; STREAMS=2; MASK=0x1111; WIDTH=640; HEIGHT=480
        QUERY_LOCAL="$REPO/d457-sipl/query/d457_query_4cam.cpp"
        QUERY_SED=''                        # already mask 0x1111, depth+rgb only
        VNC_GEOM="1280x1920"               # 4 rows x 2 cols @ VGA, no clamping needed
        ;;
    *)
        echo "usage: $0 {2|4} [--skip-deploy] [--headless] [--frame-limit N] [--depth-colormap jet|gray]" >&2
        exit 1
        ;;
esac

[ -f "$QUERY_LOCAL" ] || die "missing $QUERY_LOCAL"

if [ "$SKIP_DEPLOY" = "0" ]; then
    say "Stage $STAGE: deploying query (mask $MASK) + driver/deser (D457_MAP_LINKS=$LINKS D457_MAP_STREAMS=$STREAMS)"

    STAGED_QUERY="$(mktemp)"
    if [ -n "$QUERY_SED" ]; then
        sed "$QUERY_SED" "$QUERY_LOCAL" > "$STAGED_QUERY"
    else
        cp "$QUERY_LOCAL" "$STAGED_QUERY"
    fi
    "${SCP[@]}" "$STAGED_QUERY" "$RIG_USER@$RIG_HOST:/home/$RIG_USER/d457_query.cpp"
    rm -f "$STAGED_QUERY"

    # ★ BUILD-CACHE GOTCHA: `make` only regenerates the deser PyHSL map when MAX967XXHsl.py's mtime
    # changes, NOT when D457_MAP_* changes -- force it every deploy, else switching 2x3<->4x2 leaves
    # half the streams silently dead (symptom: S2,3,6,7 = 0).
    "${SSH[@]}" "rm -f '$RIG_SIPL/build/uddf/drivers/deserializers/MAX96712/hsl_gen/MAX967XXHsl.hslc' && \
                 touch '$RIG_SIPL/uddf/drivers/deserializers/MAX967XX/MAX967XXHsl.py'"

    # RIG_SUDO_PW must not appear in the remote argv: it would be visible in `ps` to any user on
    # the rig and recorded in the ssh command log. Feed it on stdin and let the remote shell read it.
    printf '%s\n' "${RIG_SUDO_PW:-}" | "${SSH[@]}" \
        "read -r RIG_SUDO_PW; export RIG_SUDO_PW; cd '$RIG_SIPL' && \
         D457_MAP_LINKS=$LINKS D457_MAP_STREAMS=$STREAMS ~/build_deploy.sh -c driver,deser,query"
else
    say "Stage $STAGE: --skip-deploy, reusing whatever profile is currently installed"
fi

say "syncing the Holoscan capture op + player to the rig, and rebuilding there"
# Graft this project's HSB overlay onto the rig's HSB checkout, then rebuild the two targets in the
# demo container (CUDA/nvcc are only on its PATH). tools/graft_hsb.sh operates on a local tree, so
# copy the overlay across first and run it there.
#
# _graft is wiped first: it is pure staging, and reusing it grafted stale sources from the second run
# onward. `mv -f d457-sipl-hsb repo/d457-sipl/hsb` moves the source INSIDE an existing destination
# rather than replacing it, so graft_hsb.sh then copied the previous run's overlay and reported
# success -- and the `2>/dev/null || true` hid it. Same class as the PyHSL cache staleness below.
"${SSH[@]}" "rm -rf '$RIG_HSB/_graft' && mkdir -p '$RIG_HSB/_graft'"
"${SCP[@]}" -q -r "$REPO/d457-sipl/hsb/." "$RIG_USER@$RIG_HOST:$RIG_HSB/_graft/d457-sipl-hsb/"
"${SCP[@]}" -q "$REPO/tools/graft_hsb.sh" "$RIG_USER@$RIG_HOST:$RIG_HSB/_graft/graft_hsb.sh"
"${SSH[@]}" "set -e
    cd '$RIG_HSB/_graft'
    mkdir -p repo/d457-sipl && mv d457-sipl-hsb repo/d457-sipl/hsb
    mkdir -p repo/tools && cp -f graft_hsb.sh repo/tools/
    bash repo/tools/graft_hsb.sh d457 '$RIG_HSB'"
"${SSH[@]}" "cat > '$RIG_HSB/_d457_build.sh'" <<'REMOTE'
#!/bin/bash
set -e
cd build
cmake --build . --target d457_sipl_capture -j4
cmake --build . --target d457_sipl_capture_python -j4
REMOTE
ssh -tt -o BatchMode=yes -i "$RIG_KEY" "$RIG_USER@$RIG_HOST" \
    "cd '$RIG_HSB' && sh docker/demo.sh bash _d457_build.sh; rm -f _d457_build.sh"

# ── launch ──────────────────────────────────────────────────────────────────────────────────────
PLAYER_ARGS=(--camera-config D457_Camera --link-mask "$MASK" --depth-colormap "$DEPTH_COLORMAP")
[ "$HEADLESS" = "1" ] && PLAYER_ARGS+=(--headless)
[ -n "$FRAME_LIMIT" ] && PLAYER_ARGS+=(--frame-limit "$FRAME_LIMIT")

# demo.sh forwards args via an UNQUOTED `$*` (word-splits any compound command) -- stage a real
# script and pass just its path, same workaround as the HSB build helper.
PLAY_SCRIPT="$RIG_HSB/_d457_play_stage${STAGE}.sh"
{
    echo "#!/bin/bash"
    echo "set -e"
    echo "cd '$RIG_HSB'"
    # HSB's own C++ log gate (HSB_LOG_*) is separate from --log-level (which only sets Python's
    # logging threshold) -- HOLOSCAN_LOG_LEVEL is the only thing that raises it past INFO. DEBUG
    # surfaces "Starting streaming" / "Allocated and registered N buffers" (init confirmation);
    # bump to TRACE (noisy: full GXF component trace too) to see per-frame "Got buffer" lines when
    # verifying actual frame flow, not just successful setup.
    echo "export HOLOSCAN_LOG_LEVEL=\${D457_HSB_LOG_LEVEL:-DEBUG}"
    # `docker run --rm` discards any in-container fix, so the freshly built op .so (which the graft+build step
    # just rebuilt into $RIG_HSB/build, bind-mounted) must be re-copied over the demo image's baked-in
    # dist-packages copy EVERY launch, in this SAME container invocation
    # comment for the full story. Without this the player silently runs a stale compiled operator.
    echo "cp build/python/lib/operators/d457_sipl_capture/_d457_sipl_capture.cpython-312-aarch64-linux-gnu.so \\"
    echo "   /usr/local/lib/python3.12/dist-packages/hololink/operators/d457_sipl_capture/"
    # (No LD_LIBRARY_PATH=~/sipl_libs any more: the lanes=2 libnvsipl binary patch is retired now
    # that the deser drives 4 lanes -- csi-ab's native 4 Tegra lanes are what we want. See
    # sdk-patches/restore_libnvsipl_stock.sh.)
    echo "export D457_WIDTH=$WIDTH D457_HEIGHT=$HEIGHT"
    printf 'python3 examples/d457_sipl_player.py'
    for a in "${PLAYER_ARGS[@]}"; do printf ' %q' "$a"; done
    printf '\n'
} > /tmp/_d457_play_stage_local.sh
"${SCP[@]}" /tmp/_d457_play_stage_local.sh "$RIG_USER@$RIG_HOST:$PLAY_SCRIPT"
rm -f /tmp/_d457_play_stage_local.sh

"${SSH[@]}" "nvidia-smi -L >/dev/null 2>&1" || say "WARNING: nvidia-smi -L failed -- GPU may be wedged (reboot + wait ~1-2min if the run fails at NvSci/Master setup)"

if [ "$HEADLESS" = "1" ]; then
    say "running headless (Stage $STAGE, mask $MASK, ${WIDTH}x${HEIGHT})"
    ssh -tt -o BatchMode=yes -i "$RIG_KEY" "$RIG_USER@$RIG_HOST" \
        "cd '$RIG_HSB' && sh docker/demo.sh bash '$PLAY_SCRIPT'"
else
    VNC_DISPLAY=":1"; VNC_PORT=5901
    say "setting up VNC display $VNC_DISPLAY @ $VNC_GEOM (port $VNC_PORT)"
    : "${VNC_PW:?set VNC_PW to the VNC password you want}"
    "${SSH[@]}" "bash -s" <<EOF
running_geom=\$(pgrep -af "Xvnc ${VNC_DISPLAY}" | grep -oE 'geometry [0-9]+x[0-9]+' | awk '{print \$2}' | head -1)
if [ "\$running_geom" != "$VNC_GEOM" ]; then
    [ -n "\$running_geom" ] && echo "[vnc] resizing display \$running_geom -> $VNC_GEOM"
    pkill -f "Xvnc ${VNC_DISPLAY}" 2>/dev/null || true
    sleep 1
    if [ ! -f "\$HOME/.vnc/passwd" ]; then
        mkdir -p "\$HOME/.vnc"
        echo "$VNC_PW" | vncpasswd -f > "\$HOME/.vnc/passwd"
        chmod 600 "\$HOME/.vnc/passwd"
    fi
    nohup Xvnc ${VNC_DISPLAY} -geometry $VNC_GEOM -depth 24 \
        -SecurityTypes VncAuth -PasswordFile "\$HOME/.vnc/passwd" \
        -rfbport ${VNC_PORT} -AlwaysShared > "\$HOME/xvnc.log" 2>&1 &
    sleep 3
fi
DISPLAY=${VNC_DISPLAY} xhost +local: >/dev/null 2>&1 || true
EOF
    say "view: ssh -L ${VNC_PORT}:localhost:${VNC_PORT} $RIG_USER@$RIG_HOST, then VNC viewer -> localhost:${VNC_PORT} (password: the VNC_PW you set)"
    say "launching player (Stage $STAGE, mask $MASK, ${WIDTH}x${HEIGHT}) on $VNC_DISPLAY ... Ctrl-C to stop"
    ssh -tt -o BatchMode=yes -i "$RIG_KEY" "$RIG_USER@$RIG_HOST" \
        "cd '$RIG_HSB' && DISPLAY=${VNC_DISPLAY} sh docker/demo.sh bash '$PLAY_SCRIPT'"
fi
