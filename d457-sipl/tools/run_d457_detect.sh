#!/bin/bash
# Run TAO PeopleNet (person/face detection) on the D457 over SIPL, viewable over VNC.
#
#   bash ~/run_d457_detect.sh            # detection on the RGB stream, live on VNC :1 (port 5901)
#   bash ~/run_d457_detect.sh rgb --frame-limit 300
#
# View: SSH-tunnel port 5901, then a VNC viewer -> localhost:5901 (password: mic-742). Ctrl-C to stop.
# Boxes (green=person, red=face) appear when someone is in view.
# NOTE: the first run builds the TensorRT engine from the ONNX (~4 min, one-time; then cached).
set -u
S="${1:-rgb}"
shift || true
EXTRA="$*"
export RIG_SUDO_PW=mic-742
VNC_DISPLAY=":1"
VNC_PORT=5901
GEOM="${D457_GEOM:-1280x720}"

case "$S" in depth|rgb|ir) ;; *) echo "stream must be depth|rgb|ir"; exit 1;; esac

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

echo "[1/3] building single-sensor query plugin for '$S' ..."
cd /home/mic-742/d457_tests
. lib/common.sh
gen_query "$S" || { echo "ERROR: query build failed"; exit 1; }
echo "[2/3] power-cycling the DS5 ..."
power_cycle_ds5
echo "[3/3] launching PeopleNet detection ('$S') on ${VNC_DISPLAY} ... (Ctrl-C to stop)"
cd /home/mic-742/holoscan-sensor-bridge
cat > /home/mic-742/_d457_play.sh <<EOF
cd /home/mic-742/holoscan-sensor-bridge
python3 examples/d457_tao_peoplenet.py --camera-config D457_Camera --stream $S $EXTRA
EOF
chmod +x /home/mic-742/_d457_play.sh
DISPLAY=${VNC_DISPLAY} sh docker/demo.sh bash /home/mic-742/_d457_play.sh
