#!/bin/bash
# Run one D457 stream in Holoscan, viewable over VNC.
#
#   bash ~/run_d457.sh depth     # or: rgb | ir
#   bash ~/run_d457.sh depth --headless --frame-limit 100   # no display, bounded check
#
# View: SSH-tunnel port 5901, then a VNC viewer -> localhost:5901 (password: $VNC_PW). Ctrl-C to stop.
set -u

# If more than one stream is requested (e.g. "depth rgb ir"), this is a multi-stream run -> delegate
# to the multi-stream launcher, which sizes the VNC desktop for N side-by-side views.
_ns=0
for _a in "$@"; do case "$_a" in depth|rgb|ir) _ns=$((_ns + 1)) ;; esac; done
if [ "$_ns" -gt 1 ]; then
    echo "[run_d457] $_ns streams requested -> using run_d457_multi.sh"
    exec bash $HOME/run_d457_multi.sh "$@"
fi

STREAM="${1:-depth}"
shift || true
EXTRA="$*"
export RIG_SUDO_PW="${RIG_SUDO_PW:-}"   # set to the rig sudo password for non-interactive sudo
VNC_DISPLAY=":1"
VNC_PORT=5901
GEOM="${D457_GEOM:-1280x720}"

case "$STREAM" in depth|rgb|ir) ;; *) echo "stream must be depth|rgb|ir"; exit 1;; esac

# Ensure a VNC display at exactly GEOM (restart it if missing or the wrong size).
running_geom=$(pgrep -af "Xvnc ${VNC_DISPLAY}" | grep -oE 'geometry [0-9]+x[0-9]+' | awk '{print $2}' | head -1)
if [ "$running_geom" != "$GEOM" ]; then
    [ -n "$running_geom" ] && echo "[vnc] resizing display ${running_geom} -> ${GEOM}"
    pkill -f "Xvnc ${VNC_DISPLAY}" 2>/dev/null
    sleep 1
    if [ ! -f "$HOME/.vnc/passwd" ]; then
        mkdir -p "$HOME/.vnc"
        : "${VNC_PW:?set VNC_PW to the VNC password you want}"
        echo "$VNC_PW" | vncpasswd -f > "$HOME/.vnc/passwd"
        chmod 600 "$HOME/.vnc/passwd"
    fi
    nohup Xvnc ${VNC_DISPLAY} -geometry ${GEOM} -depth 24 \
        -SecurityTypes VncAuth -PasswordFile "$HOME/.vnc/passwd" \
        -rfbport ${VNC_PORT} -AlwaysShared > "$HOME/xvnc.log" 2>&1 &
    sleep 3
fi
DISPLAY=${VNC_DISPLAY} xhost +local: >/dev/null 2>&1 || true
echo "[vnc] view: tunnel ${VNC_PORT}, then VNC viewer -> localhost:${VNC_PORT} (password: the VNC_PW you set)"

echo "[1/3] building single-sensor query plugin for '$STREAM' ..."
cd $HOME/d457_tests || exit 1
. lib/common.sh
gen_query "$STREAM" || { echo "ERROR: query build failed"; exit 1; }
echo "[2/3] power-cycling the DS5 ..."
power_cycle_ds5
echo "[3/3] launching Holoscan player ('$STREAM') on ${VNC_DISPLAY} ... (Ctrl-C to stop)"
cd $HOME/holoscan-sensor-bridge || exit 1
cat > $HOME/_d457_play.sh <<EOF
cd $HOME/holoscan-sensor-bridge || exit 1
python3 examples/d457_sipl_player.py --camera-config D457_Camera --stream $STREAM $EXTRA
EOF
chmod +x $HOME/_d457_play.sh
DISPLAY=${VNC_DISPLAY} sh docker/demo.sh bash $HOME/_d457_play.sh
