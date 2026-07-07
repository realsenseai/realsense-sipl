#!/bin/bash
# Run MULTIPLE D457 streams simultaneously in Holoscan (side-by-side views), viewable over VNC.
#
#   bash ~/run_d457_multi.sh              # depth + rgb
#   bash ~/run_d457_multi.sh depth rgb ir # all three
#   bash ~/run_d457_multi.sh depth ir
#
# View: SSH-tunnel port 5901, then a VNC viewer -> localhost:5901 (password: mic-742). Ctrl-C to stop.
# The VNC desktop is sized to N*1280 x 720 so each 16:9 stream fills its half with no distortion.
set -u
STREAMS="$*"
[ -z "$STREAMS" ] && STREAMS="depth rgb"
LIST=$(echo $STREAMS | tr ' ' ',')
NVIEWS=$(echo $STREAMS | wc -w)
export RIG_SUDO_PW=mic-742
VNC_DISPLAY=":1"
VNC_PORT=5901
# Grid layout matching the player: 1 col for a single stream, else 2 columns.
case "$NVIEWS" in
    1) COLS=1; ROWS=1 ;;
    2) COLS=2; ROWS=1 ;;
    *) COLS=2; ROWS=$(( (NVIEWS + 1) / 2 )) ;;
esac
GEOM="${D457_GEOM:-$((1280 * COLS))x$((720 * ROWS))}"

for s in $STREAMS; do
    case "$s" in depth|rgb|ir) ;; *) echo "streams must be depth|rgb|ir (got '$s')"; exit 1;; esac
done

# Ensure a VNC display at exactly GEOM (restart it if it's missing or the wrong size).
running_geom=$(pgrep -af "Xvnc ${VNC_DISPLAY}" | grep -oE 'geometry [0-9]+x[0-9]+' | awk '{print $2}' | head -1)
if [ "$running_geom" != "$GEOM" ]; then
    [ -n "$running_geom" ] && echo "[vnc] resizing display ${running_geom} -> ${GEOM}"
    pkill -f "Xvnc ${VNC_DISPLAY}" 2>/dev/null
    sleep 1
    if [ ! -f "$HOME/.vnc/passwd" ]; then
        mkdir -p "$HOME/.vnc"
        echo mic-742 | vncpasswd -f > "$HOME/.vnc/passwd"
        chmod 600 "$HOME/.vnc/passwd"
    fi
    nohup Xvnc ${VNC_DISPLAY} -geometry ${GEOM} -depth 24 \
        -SecurityTypes VncAuth -PasswordFile "$HOME/.vnc/passwd" \
        -rfbport ${VNC_PORT} -AlwaysShared > "$HOME/xvnc.log" 2>&1 &
    sleep 3
fi
DISPLAY=${VNC_DISPLAY} xhost +local: >/dev/null 2>&1 || true
echo "[vnc] view: tunnel ${VNC_PORT}, then VNC viewer -> localhost:${VNC_PORT} (password: mic-742)"
echo "[vnc] desktop ${GEOM} (reconnect your VNC viewer if it was open before this resize)"

echo "[1/3] building query plugin for streams: ${STREAMS} ..."
cd /home/mic-742/d457_tests
. lib/common.sh
gen_query $STREAMS || { echo "ERROR: query build failed"; exit 1; }
echo "[2/3] power-cycling the DS5 ..."
power_cycle_ds5
echo "[3/3] launching Holoscan player for [${LIST}] on ${VNC_DISPLAY} ... (Ctrl-C to stop)"
cd /home/mic-742/holoscan-sensor-bridge
cat > /home/mic-742/_d457_play.sh <<EOF
cd /home/mic-742/holoscan-sensor-bridge
export LD_LIBRARY_PATH=/home/mic-742/sipl_libs:\$LD_LIBRARY_PATH
export D457_STREAMS=${LIST}
python3 examples/d457_sipl_player.py --camera-config D457_Camera
EOF
chmod +x /home/mic-742/_d457_play.sh
DISPLAY=${VNC_DISPLAY} sh docker/demo.sh bash /home/mic-742/_d457_play.sh
